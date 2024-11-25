#include "common.hpp"


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
    int32_t resend = 1;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        } else if(strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        } else if(strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        } else if (strcmp(argv[i], "-resend=off") == 0) {
            resend = 0;
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_StreamAsync [-h]");
            return 0;
        }
    }

    if (!color && !depth && !ir) {
        LOGD("=== At least one component need to be on");
        return -1;
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
    TY_COMPONENT_ID componentIDs = 0;
    if(allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("=== Has RGB camera, open RGB cam");
        componentIDs |= TY_COMPONENT_RGB_CAM;
    }
    LOGD("=== Configure components, open depth cam");
    if (depth) {
        componentIDs |= TY_COMPONENT_DEPTH_CAM;
    }

    if (ir) {
        componentIDs |= TY_COMPONENT_IR_CAM_LEFT;
    }

    ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

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

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[4];
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    frameBuffer[2] = new char[frameSize];
    frameBuffer[3] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[2], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[2], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[3], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[3], frameSize) );

    LOGD("=== Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("=== Set trigger to slave mode");
    TY_TRIGGER_PARAM trigger;
    trigger.mode = TY_TRIGGER_MODE_SLAVE;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));

    //for network only
    LOGD("=== resend: %d", resend);
    if (resend) {
        bool hasResend;
        ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, &hasResend));
        if (hasResend) {
            LOGD("=== Open resend");
            ASSERT_OK(TYSetBool(hDevice, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true));
        } else {
            LOGD("=== Not support feature TY_BOOL_GVSP_RESEND");
        }
    }

    LOGD("=== Set Stream Async");
    ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEVICE, TY_ENUM_STREAM_ASYNC, TY_STREAM_ASYNC_ALL));

    LOGD("=== Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("=== While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;

    int32_t comp_expect = componentIDs;
    int err = TY_STATUS_OK;
    LOGD("=== Send Soft Trigger");
    while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(hDevice)));
    if (err != TY_STATUS_OK) {
        /*
         * Sometime we got other errors
         *     -1005 before 3.6.53 indicate trigger cmd timeout
         *     new sdk will return -1014 when trigger cmd timeout
         * If we got a timeout err, we do not know whether the
         * deivce missed the cmd or the app missied the ack.
         * We'd better restart the capture
         */
        LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
        exit_main = true;
    }


    while(!exit_main) {
        err = TYFetchFrame(hDevice, &frame, 20000);
        if( err == TY_STATUS_OK ) {
            LOGD("=== Get frame %d", ++index);

            for (int i=0; i<frame.validCount; i++) {
                if (frame.image[i].status == TY_STATUS_OK) {
                    void *  image_pos  = frame.image[i].buffer;
                    int32_t image_size = frame.image[i].height * TYPixelLineSize(frame.image[i].width, frame.image[i].pixelFormat);
                    if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT) {
                        LOGD("===   Image %d (LEFT_IR , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                        comp_expect &= ~TY_COMPONENT_IR_CAM_LEFT;
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT) {
                        LOGD("===   Image %d (RIGHT_IR, %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                        comp_expect &= ~TY_COMPONENT_IR_CAM_RIGHT;
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_RGB_CAM) {
                        LOGD("===   Image %d (RGB     , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                        comp_expect &= ~TY_COMPONENT_RGB_CAM;
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
                        LOGD("===   Image %d (DEPTH   , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                        comp_expect &= ~TY_COMPONENT_DEPTH_CAM;
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_BRIGHT_HISTO) {
                        LOGD("===   Image %d (HISTO   , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                        comp_expect &= ~TY_COMPONENT_BRIGHT_HISTO;
                    }
                }
            }
            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color);
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

            LOGD("=== Re-enqueue buffer(%p, %d)"
                , frame.userBuffer, frame.bufferSize);
            ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );

            if (comp_expect == 0) {
                comp_expect = componentIDs;
                LOGD("=== Send Soft Trigger");
                while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(hDevice)));
                if (err != TY_STATUS_OK) {
                    /*
                     * Sometime we got other errors
                     *     -1005 before 3.6.53 indicate trigger cmd timeout
                     *     new sdk will return -1014 when trigger cmd timeout
                     * If we got a timeout err, we do not know whether the
                     * deivce missed the cmd or the app missied the ack.
                     * We'd better restart the capture
                     */
                    LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
                    break;
                }

                    }
                }
        else {
            comp_expect = componentIDs;
            LOGD("=== Lost Frame Here!!!");
            LOGD("=== Send Soft Trigger");
            while(TY_STATUS_BUSY == TYSendSoftTrigger(hDevice));
            while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(hDevice)));
            if (err != TY_STATUS_OK) {
                /*
                 * Sometime we got other errors
                 *     -1005 before 3.6.53 indicate trigger cmd timeout
                 *     new sdk will return -1014 when trigger cmd timeout
                 * If we got a timeout err, we do not know whether the
                 * deivce missed the cmd or the app missied the ack.
                 * We'd better restart the capture
                 */
                LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
                break;
            }
        }
    }

    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];
    delete frameBuffer[2];
    delete frameBuffer[3];

    LOGD("=== Main done!");
    return 0;
}
