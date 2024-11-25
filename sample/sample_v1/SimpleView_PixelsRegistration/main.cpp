#include "common.hpp"
#include "TYImageProc.h"

// The distance(mm) range defined by the macro is related to the camera model
#define MIN_DEPTH			(400)
#define MAX_DEPTH			(4000)

// The rect size used for rgbd registration in color image.
// Located in the center of the color image.
#define DEFAULT_RECT_WIDTH 80
#define DEFAULT_RECT_HEIGHT 60

struct CallbackData {
  int             index;
  TY_ISP_HANDLE   IspHandle;
  TY_DEV_HANDLE   hDevice;
  DepthRender*    render;

  float           scale_unit;

  TY_CAMERA_CALIB_INFO depth_calib;
  TY_CAMERA_CALIB_INFO color_calib;
};

static void doRectRegister(const TY_CAMERA_CALIB_INFO& depth_calib
                      , const TY_CAMERA_CALIB_INFO& color_calib
                      , cv::Mat& depth
                      , float f_scale_unit
                      , cv::Mat& undistort_color
                      , const cv::Rect& src
                      , std::vector<cv::Point>& point_array
                      )
{
  std::vector<TY_PIXEL_COLOR_DESC> src_rgb_data(4);
  std::vector<TY_PIXEL_COLOR_DESC> dst_rgb_data(4);

  src_rgb_data[0].x = src.x;              src_rgb_data[0].y = src.y;
  src_rgb_data[1].x = src.x + src.width;  src_rgb_data[1].y = src.y;
  src_rgb_data[2].x = src.x + src.width;  src_rgb_data[2].y = src.y + src.height;
  src_rgb_data[3].x = src.x;              src_rgb_data[3].y = src.y + src.height;

  cv::Mat temp;
  cv::medianBlur(depth, temp, 5);
  depth = temp;

  ASSERT_OK(
    TYMapRGBPixelsToDepthCoordinate(
      &depth_calib,
      depth.cols, depth.rows, depth.ptr<uint16_t>(),
      &color_calib,
      undistort_color.cols, undistort_color.rows, &src_rgb_data[0], src_rgb_data.size(),
      MIN_DEPTH, MAX_DEPTH,
      &dst_rgb_data[0], f_scale_unit
    )
  );
  
  int size = dst_rgb_data.size();
  for (int i = 0; i < dst_rgb_data.size(); i++) {
    point_array[i].x = dst_rgb_data[i].x * f_scale_unit;
    point_array[i].y = dst_rgb_data[i].y * f_scale_unit;
  }
}

void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
  CallbackData* pData = (CallbackData*)userdata;
  LOGD("=== Get frame %d", ++pData->index);

  cv::Mat depth, color;
  parseFrame(*frame, &depth, 0, 0, &color, pData->IspHandle);
  if (!depth.empty() && !color.empty()) {
    // do undistortion
    int32_t         image_size;   
    TY_PIXEL_FORMAT color_fmt;
    if(color.type() == CV_16U) {
      image_size = color.size().area() * 2;
      color_fmt = TY_PIXEL_FORMAT_MONO16;
    }
    else if(color.type() == CV_16UC3)
    {
      image_size = color.size().area() * 6;
      color_fmt = TY_PIXEL_FORMAT_RGB48;
    }
    else {
      image_size = color.size().area() * 3;
      color_fmt = TY_PIXEL_FORMAT_RGB;
    }

    cv::Mat  undistort_color;
    if(color_fmt == TY_PIXEL_FORMAT_MONO16)
      undistort_color = cv::Mat(color.size(), CV_16U);
    else if(color_fmt == TY_PIXEL_FORMAT_RGB48)
      undistort_color = cv::Mat(color.size(), CV_16UC3);
    else
      undistort_color = cv::Mat(color.size(), CV_8UC3);

    TY_IMAGE_DATA src;
    src.width = color.cols;
    src.height = color.rows;
    src.size = image_size;
    src.pixelFormat = color_fmt;
    src.buffer = color.data;

    TY_IMAGE_DATA dst;
    dst.width = color.cols;
    dst.height = color.rows;
    dst.size = image_size;
    dst.buffer = undistort_color.data;
    dst.pixelFormat = color_fmt;
    ASSERT_OK(TYUndistortImage(&pData->color_calib, &src, NULL, &dst));

    //use default roi area
    cv::Rect roi_src = cv::Rect(
      (undistort_color.cols - DEFAULT_RECT_WIDTH) / 2, 
      (undistort_color.rows - DEFAULT_RECT_HEIGHT) / 2, 
      DEFAULT_RECT_WIDTH, DEFAULT_RECT_HEIGHT);

    std::vector<cv::Point> dst_pos(4);
    doRectRegister(pData->depth_calib, pData->color_calib, depth, pData->scale_unit, undistort_color, roi_src, dst_pos);
	
    cv::rectangle(undistort_color, roi_src, cv::Scalar(100, 100, 100), 2);
    cv::imshow("undistort color", undistort_color);

    cv::Mat depth_display = pData->render->Compute(depth);

    int m_valid_pixels_cnt = 0;
    for (int i = 0; i < dst_pos.size(); i++) {
      if ((dst_pos[i].x >= 0) && (dst_pos[i].y >= 0))
        m_valid_pixels_cnt++;
    }
    if (m_valid_pixels_cnt == dst_pos.size()) {
      cv::line(depth_display, dst_pos[0], dst_pos[1], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[1], dst_pos[2], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[2], dst_pos[3], cv::Scalar(200, 0, 0), 1);
      cv::line(depth_display, dst_pos[3], dst_pos[0], cv::Scalar(200, 0, 0), 1);
    }
    else {
      LOGW("Some pixels map failed.\n");
    }
    cv::imshow("Depth", depth_display);
  }

  LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
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

  for(int i = 1; i < argc; i++){
    if(strcmp(argv[i], "-id") == 0){
      ID = argv[++i];
    } else if(strcmp(argv[i], "-ip") == 0) {
      IP = argv[++i];
    }else if(strcmp(argv[i], "-h") == 0){
      LOGI("Usage: SimpleView_PixelsRegistration [-h] [-id <ID>]");
      return 0;
    }
  }

  LOGD("=== Init lib");
  ASSERT_OK( TYInitLib() );
  TY_VERSION_INFO ver;
  ASSERT_OK( TYLibVersion(&ver) );
  LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

  std::vector<TY_DEVICE_BASE_INFO> selected;
  ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
  ASSERT(selected.size() > 0);
  TY_DEVICE_BASE_INFO& selectedDev = selected[0];

  ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
  ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

  TY_COMPONENT_ID allComps;
  ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
  if(!(allComps & TY_COMPONENT_RGB_CAM)){
    LOGE("=== Has no RGB camera, cant do registration");
    return -1;
  }
  TY_ISP_HANDLE isp_handle;
  ASSERT_OK(TYISPCreate(&isp_handle));
  ASSERT_OK(ColorIspInitSetting(isp_handle, hDevice));
  //You can turn on auto exposure function as follow ,but frame rate may reduce .
  //Device also may be casually stucked  1~2 seconds when software trying to adjust device exposure time value
#if 0
  ASSERT_OK(ColorIspInitAutoExposure(isp_handle, hDevice));
#endif

  LOGD("=== Configure components");
  TY_COMPONENT_ID componentIDs = TY_COMPONENT_DEPTH_CAM | TY_COMPONENT_RGB_CAM;
  ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

  bool hasUndistortSwitch, hasDistortionCoef;
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, &hasUndistortSwitch) );
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_DISTORTION, &hasDistortionCoef) );
  if (hasUndistortSwitch) {
    ASSERT_OK( TYSetBool(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, true) );
  }

  LOGD("=== Prepare image buffer");
  uint32_t frameSize;
  ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
  LOGD("     - Get size of framebuffer, %d", frameSize);
  LOGD("     - Allocate & enqueue buffers");
  char* frameBuffer[2];
  frameBuffer[0] = new char[frameSize];
  frameBuffer[1] = new char[frameSize];
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
  ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
  LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
  ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

  LOGD("=== Register event callback");
  ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

  bool hasTriggerParam = false;
  ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTriggerParam) );
  if (hasTriggerParam) {
    LOGD("=== Disable trigger mode");
    TY_TRIGGER_PARAM_EX trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
  }

  DepthRender render;
  CallbackData cb_data;
  cb_data.index = 0;
  cb_data.hDevice = hDevice;
  cb_data.render = &render;
  cb_data.IspHandle = isp_handle;

  float scale_unit = 1.;
  TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
  cb_data.scale_unit = scale_unit;

  LOGD("=== Read depth calib info");
  ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
      , &cb_data.depth_calib, sizeof(cb_data.depth_calib)) );

  LOGD("=== Read color calib info");
  ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA
      , &cb_data.color_calib, sizeof(cb_data.color_calib)) );

  LOGD("=== Start capture");
  ASSERT_OK( TYStartCapture(hDevice) );

  LOGD("=== Wait for callback");
  bool exit_main = false;

  while(!exit_main){
    TY_FRAME_DATA frame;
    int err = TYFetchFrame(hDevice, &frame, -1);
    if( err != TY_STATUS_OK ) {
      LOGE("Fetch frame error %d: %s", err, TYErrorString(err));
      break;
    }

    handleFrame(&frame, &cb_data);
    TYISPUpdateDevice(cb_data.IspHandle);
    int key = cv::waitKey(1);
    switch(key & 0xff){
      case 0xff:
        break;
      case 'q':
        exit_main = true;
        break;
      default:
        LOGD("Pressed key %d", key);
    }
  }

  ASSERT_OK( TYStopCapture(hDevice) );
  ASSERT_OK( TYCloseDevice(hDevice) );
  ASSERT_OK( TYCloseInterface(hIface) );
  ASSERT_OK( TYDeinitLib() );
  delete frameBuffer[0];
  delete frameBuffer[1];

  LOGD("=== Main done!");
  return 0;
}
