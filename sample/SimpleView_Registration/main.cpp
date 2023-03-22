#include "../common/common.hpp"
#include "TYImageProc.h"

#define MAP_DEPTH_TO_COLOR  0

struct CallbackData {
  int             index;
  TY_ISP_HANDLE   IspHandle;
  TY_DEV_HANDLE   hDevice;
  DepthRender*    render;
  DepthViewer*    depthViewer;
  bool            needUndistort;

  float           scale_unit;

  bool            isTof;
  TY_CAMERA_CALIB_INFO depth_calib;
  TY_CAMERA_CALIB_INFO color_calib;
};

cv::Mat tofundis_mapx, tofundis_mapy;
static void doRegister(const TY_CAMERA_CALIB_INFO& depth_calib
                      , const TY_CAMERA_CALIB_INFO& color_calib
                      , const cv::Mat& depth
                      , const float f_scale_unit
                      , const cv::Mat& color
                      , bool needUndistort
                      , cv::Mat& undistort_color
                      , cv::Mat& out
                      , bool map_depth_to_color
                      )
{
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
  // do undistortion
  if (needUndistort) {
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
    dst.pixelFormat = color_fmt;
    dst.buffer = undistort_color.data;
    ASSERT_OK(TYUndistortImage(&color_calib, &src, NULL, &dst));
  }
  else {
    undistort_color = color;
  }

  // do register
  if (map_depth_to_color) {
    out = cv::Mat::zeros(undistort_color.size(), CV_16U);
    ASSERT_OK(
      TYMapDepthImageToColorCoordinate(
        &depth_calib,
        depth.cols, depth.rows, depth.ptr<uint16_t>(),
        &color_calib,
        out.cols, out.rows, out.ptr<uint16_t>(), f_scale_unit
      )
    );
    cv::Mat temp;
    //you may want to use median filter to fill holes in projected depth image
    //or do something else here
    cv::medianBlur(out, temp, 5);
    out = temp;
  }
  else {
    if(color_fmt == TY_PIXEL_FORMAT_MONO16)
    {
      out = cv::Mat::zeros(depth.size(), CV_16U);
      ASSERT_OK(
        TYMapMono16ImageToDepthCoordinate(
          &depth_calib,
          depth.cols, depth.rows, depth.ptr<uint16_t>(),
          &color_calib,
          undistort_color.cols, undistort_color.rows, undistort_color.ptr<uint16_t>(),
          out.ptr<uint16_t>(), f_scale_unit
        )
      );
    }
    else if(color_fmt == TY_PIXEL_FORMAT_RGB48)
    {
      out = cv::Mat::zeros(depth.size(), CV_16UC3);
      ASSERT_OK(
        TYMapRGB48ImageToDepthCoordinate(
          &depth_calib,
          depth.cols, depth.rows, depth.ptr<uint16_t>(),
          &color_calib,
          undistort_color.cols, undistort_color.rows, undistort_color.ptr<uint16_t>(),
          out.ptr<uint16_t>(), f_scale_unit
        )
      );
    }
    else{
      out = cv::Mat::zeros(depth.size(), CV_8UC3);
      ASSERT_OK(
        TYMapRGBImageToDepthCoordinate(
          &depth_calib,
          depth.cols, depth.rows, depth.ptr<uint16_t>(),
          &color_calib,
          undistort_color.cols, undistort_color.rows, undistort_color.ptr<uint8_t>(),
          out.ptr<uint8_t>(), f_scale_unit
        )
      );
    }
  }
}


void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
  CallbackData* pData = (CallbackData*)userdata;
  LOGD("=== Get frame %d", ++pData->index);

  cv::Mat depth, color;
  parseFrame(*frame, &depth, 0, 0, &color, pData->IspHandle);
  if (!depth.empty()) {
    if (pData->isTof)
    {
        TY_IMAGE_DATA src;
        src.width = depth.cols;
        src.height = depth.rows;
        src.size = depth.size().area() * 2;
        src.pixelFormat = TY_PIXEL_FORMAT_DEPTH16;
        src.buffer = depth.data;

        cv::Mat undistort_depth = cv::Mat(depth.size(), CV_16U);
        TY_IMAGE_DATA dst;
        dst.width = depth.cols;
        dst.height = depth.rows;
        dst.size = undistort_depth.size().area() * 2;
        dst.buffer = undistort_depth.data;
        dst.pixelFormat = TY_PIXEL_FORMAT_DEPTH16;
        ASSERT_OK(TYUndistortImage(&pData->depth_calib, &src, NULL, &dst));

        depth = undistort_depth.clone();
    }
    pData->depthViewer->show(depth);
  }
  if (!color.empty()) {
    cv::imshow("color", color);
  }

  if (!depth.empty() && !color.empty()) {
    cv::Mat undistort_color, out;
    if (pData->needUndistort || MAP_DEPTH_TO_COLOR) {
      doRegister(pData->depth_calib, pData->color_calib, depth, pData->scale_unit, color, pData->needUndistort, undistort_color, out, MAP_DEPTH_TO_COLOR);
    }
    else {
      undistort_color = color;
      out = color;
    }
    cv::imshow("undistort color", undistort_color);

    cv::Mat tmp, gray8, bgr;
    if (MAP_DEPTH_TO_COLOR) {
      cv::Mat depthDisplay = pData->render->Compute(out);
      if(undistort_color.type() == CV_16U) {
        gray8 = cv::Mat(undistort_color.size(), CV_8U);
        cv::normalize(undistort_color, tmp, 0, 255, cv::NORM_MINMAX);
        cv::convertScaleAbs(tmp, gray8);
        cv::cvtColor(gray8, undistort_color, cv::COLOR_GRAY2BGR);
      } else if(undistort_color.type() == CV_16UC3) {
        bgr = cv::Mat(undistort_color.size(), CV_8UC3);
        cv::normalize(undistort_color, tmp, 0, 255, cv::NORM_MINMAX);
        cv::convertScaleAbs(tmp, bgr);
        undistort_color = bgr.clone();
      }
      depthDisplay = depthDisplay / 2 + undistort_color / 2;
      cv::imshow("depth2color RGBD", depthDisplay);
    }
    else {
      cv::imshow("mapped RGB", out);
      if(out.type() == CV_16U) {
        gray8 = cv::Mat(out.size(), CV_8U);
        cv::normalize(out, tmp, 0, 255, cv::NORM_MINMAX);
        cv::convertScaleAbs(tmp, gray8);
        cv::cvtColor(gray8, out, cv::COLOR_GRAY2BGR);
      } else if(undistort_color.type() == CV_16UC3) {
        bgr = cv::Mat(out.size(), CV_8UC3);
        cv::normalize(out, tmp, 0, 255, cv::NORM_MINMAX);
        cv::convertScaleAbs(tmp, bgr);
        out = bgr.clone();
      }
      cv::Mat depthDisplay = pData->render->Compute(depth);
      depthDisplay = depthDisplay / 2 + out / 2;
      cv::imshow("color2depth RGBD", depthDisplay);
    }
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
            LOGI("Usage: SimpleView_Registration [-h] [-id <ID>]");
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

    int32_t allComps;
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
    int32_t componentIDs = TY_COMPONENT_DEPTH_CAM | TY_COMPONENT_RGB_CAM;
    ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

    // ASSERT_OK( TYSetEnum(hDevice, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_YUYV_640x480) );
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
    ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &hasTriggerParam) );
    if (hasTriggerParam) {
        LOGD("=== Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
    }

    DepthViewer depthViewer("Depth");
    DepthRender render;
    CallbackData cb_data;
    cb_data.index = 0;
    cb_data.hDevice = hDevice;
    cb_data.depthViewer = &depthViewer;
    cb_data.render = &render;
    cb_data.needUndistort = !hasUndistortSwitch && hasDistortionCoef;
    cb_data.IspHandle = isp_handle;

    float scale_unit = 1.;
    TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
    cb_data.scale_unit = scale_unit;
    depthViewer.depth_scale_unit = scale_unit;


    LOGD("=== Read depth calib info");
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
          , &cb_data.depth_calib, sizeof(cb_data.depth_calib)) );

    LOGD("=== Read color calib info");
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA
          , &cb_data.color_calib, sizeof(cb_data.color_calib)) );

    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &cb_data.isTof));

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
