#include "../common/common.hpp"


static volatile bool fakeLock = false; // NOTE: fakeLock may lock failed

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    TY_ISP_HANDLE   hIspHandle;
    bool            exit;
    cv::Mat         depth;
    cv::Mat         leftIR;
    cv::Mat         rightIR;
    cv::Mat         color;
    DepthViewer*    dViewer;
};

void frameCallback(TY_FRAME_DATA* frame, void* userdata)
{
  CallbackData* pData = (CallbackData*)userdata;
  LOGD("=== Get frame %d", ++pData->index);

  int fps = get_fps();
  if (fps > 0){
      LOGI("fps: %d", fps);
  }

  parseFrame(*frame, &pData->depth, &pData->leftIR, &pData->rightIR, &pData->color,pData->hIspHandle);

  if(!pData->depth.empty()){
      pData->dViewer->show(pData->depth);
  }
  if(!pData->leftIR.empty()) { cv::imshow("LeftIR", pData->leftIR); }
  if(!pData->rightIR.empty()){ cv::imshow("RightIR", pData->rightIR); }
  if(!pData->color.empty()){ cv::imshow("Color", pData->color); }
  TYISPUpdateDevice(pData->hIspHandle);

  if (!pData->color.empty()) {
    LOGI("Color format is %s", colorFormatName(TYImageInFrame(*frame, TY_COMPONENT_RGB_CAM)->pixelFormat));
  }
  int key = cv::waitKey(1);
    switch (key & 0xff) {
    case 0xff:
      break;
    case 'q':
      pData->exit = true;
      // have to call TYUnregisterCallback to release thread
      break;
    default:
      LOGD("Unmapped key %d", key);
    }

  LOGD("=== Callback: Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
  ASSERT_OK(TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize));
}

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

int main(int argc, char* argv[])
{
  std::string ID, IP;
  TY_INTERFACE_HANDLE hIface = NULL;
  TY_DEV_HANDLE hDevice = NULL;
  int32_t color, ir, depth;
  color = ir = depth = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-id") == 0) {
      ID = argv[++i];
    }
    else if (strcmp(argv[i], "-ip") == 0) {
      IP = argv[++i];
    }
    else if (strcmp(argv[i], "-color=off") == 0) {
      color = 0;
    }
    else if (strcmp(argv[i], "-depth=off") == 0) {
      depth = 0;
    }
    else if (strcmp(argv[i], "-ir=off") == 0) {
      ir = 0;
    }
    else if (strcmp(argv[i], "-h") == 0) {
      LOGI("Usage: SimpleView_Callback [-h] [-id <ID>] [-ip <IP>]");
      return 0;
    }
  }

  if (!color && !depth && !ir) {
    LOGD("At least one component need to be on");
    return -1;
  }

  LOGD("Init lib");
  ASSERT_OK(TYInitLib());
  TY_VERSION_INFO ver;
  ASSERT_OK(TYLibVersion(&ver));
  LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

  std::vector<TY_DEVICE_BASE_INFO> selected;
  ASSERT_OK(selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected));
  ASSERT(selected.size() > 0);
  TY_DEVICE_BASE_INFO& selectedDev = selected[0];

  ASSERT_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
  ASSERT_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));

  int32_t allComps;
  ASSERT_OK(TYGetComponentIDs(hDevice, &allComps));
  if (allComps & TY_COMPONENT_RGB_CAM  && color) {
    LOGD("Has RGB camera, open RGB cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
  }

  if (allComps & TY_COMPONENT_IR_CAM_LEFT && ir) {
    LOGD("Has IR left camera, open IR left cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
  }

  if (allComps & TY_COMPONENT_IR_CAM_RIGHT && ir) {
    LOGD("Has IR right camera, open IR right cam");
    ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
  }

  //try to enable depth map
  LOGD("Configure components, open depth cam");
  DepthViewer depthViewer("Depth");
  if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
      int32_t image_mode;
      ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
      LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
      ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
      ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));
  
      //depth map pixel format is uint16_t ,which default unit is  1 mm
      //the acutal depth (mm)= PixelValue * ScaleUnit 
      float scale_unit = 1.;
      TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
      depthViewer.depth_scale_unit = scale_unit;
  }

  LOGD("Prepare image buffer");
  uint32_t frameSize;
  ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
  LOGD("     - Get size of framebuffer, %d", frameSize);

  LOGD("     - Allocate & enqueue buffers");
  char* frameBuffer[2];
  frameBuffer[0] = new char[frameSize];
  frameBuffer[1] = new char[frameSize];
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
  ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize));
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
  ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize));

  LOGD("Register event callback");
  ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

  bool hasTrigger;
  ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &hasTrigger));
  if (hasTrigger) {
    LOGD("Disable trigger mode");
    TY_TRIGGER_PARAM trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
  }
  
  // Create callback thread
  CallbackData cb_data;
  cb_data.index = 0;
  cb_data.hDevice = hDevice;
  cb_data.exit = false;
  cb_data.hIspHandle = NULL;
  cb_data.dViewer = &depthViewer;
  // Register Callback
  CallbackWrapper cbWrapper;
  cbWrapper.TYRegisterCallback(hDevice, frameCallback, &cb_data);

  int32_t componentIDs = 0;
  ASSERT_OK(TYGetEnabledComponents(hDevice, &componentIDs));
  if (allComps & TY_COMPONENT_RGB_CAM & componentIDs){
      ASSERT_OK(TYISPCreate(&cb_data.hIspHandle));//create a isp handle to convert raw image(color bayer format) to rgb image
      ASSERT_OK(ColorIspInitSetting(cb_data.hIspHandle, hDevice));//Init code can be modified in common.hpp
      //You can  call follow function to show  color isp supported features
#if 0
        ColorIspShowSupportedFeatures(hColorIspHandle);
#endif
      //You can turn on auto exposure function as follow ,but frame rate may reduce .
      //Device may be casually stucked  1~2 seconds while software is trying to adjust device exposure time value
#if 0
      ASSERT_OK(ColorIspInitAutoExposure(cb_data.hIspHandle, hDevice));
#endif
  }

  LOGD("Start capture");
  ASSERT_OK(TYStartCapture(hDevice));
  
  LOGD("While loop to fetch frame");
  while (!cb_data.exit) {
     MSLEEP(1000*10);
  }

  cbWrapper.TYUnregisterCallback();
  ASSERT_OK(TYStopCapture(hDevice));
  ASSERT_OK(TYCloseDevice(hDevice));
  ASSERT_OK(TYCloseInterface(hIface));
  ASSERT_OK(TYISPRelease(&cb_data.hIspHandle));
  ASSERT_OK(TYDeinitLib());
  delete frameBuffer[0];
  delete frameBuffer[1];

  LOGD("Main done!");
  return 0;
}
