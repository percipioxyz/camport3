#include "../common/common.hpp"

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        *(bool*)userdata = true;
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        }else if(strcmp(argv[i], "-h") == 0){
            LOGI("Usage: SimpleView_Callback [-h] [-id <ID>]");
            return 0;
        }
    }

    LOGD("=== Init lib");
    int loop_index = 1;
    bool loop_exit = false;

    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    while(!loop_exit) {
        LOGD("==========================");
        LOGD("========== loop %d", loop_index++);
        LOGD("==========================");


        TY_INTERFACE_HANDLE hIface;
        TY_DEV_HANDLE hDevice;
        std::vector<TY_DEVICE_BASE_INFO> selected;

        int ret = 0;
        ret = selectDevice(TY_INTERFACE_USB, ID, IP, 1, selected);
        printf("ret: %d\n", ret);

        while (ret == TY_STATUS_ERROR) {
            LOGI("Wait device ...");
            ret = selectDevice(TY_INTERFACE_USB, ID, IP, 1, selected);
            if (ret == TY_STATUS_ERROR) {
                MSleep(2000); 
            }
        }
        ASSERT(selected.size() > 0);
        TY_DEVICE_BASE_INFO& selectedDev = selected[0];

        LOGD("=== Open interface: %s", selectedDev.iface.id);
        ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );

        LOGD("=== Open device: %s", selectedDev.id);
        ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

        int32_t allComps;
        ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
        if(allComps & TY_COMPONENT_RGB_CAM){
            LOGD("=== Has RGB camera, open RGB cam");
            ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );
        }

        LOGD("=== Configure components, open depth cam");
        int32_t componentIDs = TY_COMPONENT_DEPTH_CAM;
        ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

        LOGD("=== Configure feature, set resolution to 640x480.");
        int err = TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_DEPTH16_640x480);
        ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);

        LOGD("=== Prepare image buffer");
        uint32_t frameSize;
        ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
        LOGD("     - Get size of framebuffer, %d", frameSize);
        ASSERT( frameSize >= 640*480*2 );

        LOGD("     - Allocate & enqueue buffers");
        char* frameBuffer[2];
        frameBuffer[0] = new char[frameSize];
        frameBuffer[1] = new char[frameSize];
        LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
        ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
        LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
        ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

        bool device_offline = false;;
        LOGD("=== Register event callback");
        LOGD("Note: Callback may block internal data receiving,");
        LOGD("      so that user should not do long time work in callback.");
        ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, &device_offline));

        LOGD("=== Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));

        LOGD("=== Start capture");
        ASSERT_OK( TYStartCapture(hDevice) );

        bool saveFrame = false;
        int saveIdx = 0;
        cv::Mat depth;
        cv::Mat leftIR;
        cv::Mat rightIR;
        cv::Mat color;

        LOGD("=== Wait for callback");
        bool exit_main = false;
        DepthViewer depthViewer("LoopDetect");
        int count = 0;
        TY_FRAME_DATA frame;
        while(!exit_main){
            int err = TYFetchFrame(hDevice, &frame, 100);
            if( err == TY_STATUS_OK ) {
                LOGD("=== Get frame %d", ++count);
                parseFrame(frame, &depth, &leftIR, &rightIR, &color);

                if(!color.empty()){
                    LOGI("Color format is %s", colorFormatName(TYImageInFrame(frame, TY_COMPONENT_RGB_CAM)->pixelFormat));
                }

                LOGD("=== Callback: Re-enqueue buffer(%p, %d)", frame.userBuffer, frame.bufferSize);
                ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );
                if(!depth.empty()){
                    depthViewer.show(depth);
                }
                if(!leftIR.empty()){
                    cv::imshow("LeftIR", leftIR);
                }
                if(!rightIR.empty()){
                    cv::imshow("RightIR", rightIR);
                }
                if(!color.empty()){
                    cv::imshow("color", color);
                }

                if(saveFrame && !depth.empty() && !leftIR.empty() && !rightIR.empty()){
                    LOGI(">>>> save frame %d", saveIdx);
                    char f[32];
                    sprintf(f, "%d.img", saveIdx++);
                    FILE* fp = fopen(f, "wb");
                    fwrite(depth.data, 2, depth.size().area(), fp);
                    fwrite(color.data, 3, color.size().area(), fp);

                    fclose(fp);

                    saveFrame = false;
                }
            }
              
            if(device_offline){
                LOGI("Found device offline");
                break;
            }

            int key = cv::waitKey(10);
            switch(key & 0xff){
                case 0xff:
                    break;
                case 'q':
                    exit_main = true;
                    break;
                case 's':
                    saveFrame = true;
                    break;
                case 'x':
                    exit_main = true;
                    loop_exit = true;
                    break;
                default:
                    LOGD("Unmapped key %d", key);
            }
        }

        if(device_offline) {
            LOGI("device offline release resource");
        } else {
            LOGI("normal exit");
        }
        ASSERT_OK( TYStopCapture(hDevice) );
        ASSERT_OK( TYCloseDevice(hDevice) );
        ASSERT_OK( TYCloseInterface(hIface) );
        delete frameBuffer[0];
        delete frameBuffer[1];
    }

    LOGD("=== Deinit lib");
    ASSERT_OK( TYDeinitLib() );
    LOGD("=== Main done!");
    return 0;
}
