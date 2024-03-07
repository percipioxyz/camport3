#include "../common/common.hpp"


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
    std::string IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_ISP_HANDLE hColorIspHandle = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;
    int got = 0;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        } else if(strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        } else if(strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: %s  [-h] [-ip <IP>]", argv[0]);
            return 0;
        }
    }

    if (IP.empty()) {
        LOGD("Must set IP with -ip IP");
        return -1;
    }
    if (!color && !depth && !ir) {
        LOGD("At least one component need to be on");
        return -1;
    }

    LOGD("Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    LOGD("Update interface list");
    ASSERT_OK( TYUpdateInterfaceList() );

    uint32_t n = 0;
    ASSERT_OK( TYGetInterfaceNumber(&n) );
    LOGD("Got %u interface list", n);
    if(n == 0){
      LOGE("interface number incorrect");
      return TY_STATUS_ERROR;
    }

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    ASSERT_OK( TYGetInterfaceList(&ifaces[0], n, &n) );
    ASSERT( n == ifaces.size() );
    for(uint32_t i = 0; i < n; i++){
      if(TYIsNetworkInterface(ifaces[i].type)){
        ASSERT_OK( TYOpenInterface(ifaces[i].id, &hIface) );
        if (TYOpenDeviceWithIP(hIface, IP.c_str(), &hDevice) == TY_STATUS_OK) {
          LOGI("Found interface %u:", i);
          LOGI("  name: %s", ifaces[i].name);
          LOGI("  id:   %s", ifaces[i].id);
          LOGI("  type: 0x%x", ifaces[i].type);
          LOGI("    MAC: %s", ifaces[i].netInfo.mac);
          LOGI("    ip: %s", ifaces[i].netInfo.ip);
          LOGI("    netmask: %s", ifaces[i].netInfo.netmask);
          LOGI("    gateway: %s", ifaces[i].netInfo.gateway);
          LOGI("    broadcast: %s", ifaces[i].netInfo.broadcast);
          LOGI("*** Select device on %s, ip %s", ifaces[i].id, IP.c_str());
          got = 1;
          break;
        }
      }
    }
    if (got == 0) {
        LOGI("*** cannot open device with ip %s", IP.c_str());
        return 0;
    }
    ASSERT(hDevice);

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );

    ///try to enable color camera
    if(allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );
        //create a isp handle to convert raw image(color bayer format) to rgb image
        ASSERT_OK(TYISPCreate(&hColorIspHandle));
        //Init code can be modified in common.hpp
        //NOTE: Should set RGB image format & size before init ISP
        ASSERT_OK(ColorIspInitSetting(hColorIspHandle, hDevice));
        //You can  call follow function to show  color isp supported features
#if 0
        ColorIspShowSupportedFeatures(hColorIspHandle);
#endif
        //You can turn on auto exposure function as follow ,but frame rate may reduce .
        //Device may be casually stucked  1~2 seconds while software is trying to adjust device exposure time value
#if 0
        ASSERT_OK(ColorIspInitAutoExposure(hColorIspHandle, hDevice));
#endif
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
        TY_IMAGE_MODE image_mode;
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

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    bool hasTrigger;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTrigger));
    if (hasTrigger) {
        LOGD("Disable trigger mode");
        TY_TRIGGER_PARAM_EX trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
    }

    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color, hColorIspHandle);
            if(!depth.empty()){
                depthViewer.show(depth);
            }
            if(!irl.empty()){ cv::imshow("LeftIR", irl); }
            if(!irr.empty()){ cv::imshow("RightIR", irr); }
            if(!color.empty()){ cv::imshow("Color", color); }

            int key = cv::waitKey(1);
            switch(key & 0xff) {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            default:
                LOGD("Unmapped key %d", key);
            }

            TYISPUpdateDevice(hColorIspHandle);
            LOGD("Re-enqueue buffer(%p, %d)"
                , frame.userBuffer, frame.bufferSize);
            ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );
        }
    }
    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK(TYISPRelease(&hColorIspHandle));
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
