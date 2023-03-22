#include "../common/common.hpp"


struct CamInfo
{
    char                sn[32];
    TY_INTERFACE_HANDLE hIface;
    TY_DEV_HANDLE       hDev;
    std::vector<char>   fb[2];
    TY_FRAME_DATA       frame;
    int                 idx;
    DepthRender         render;

    CamInfo() : hDev(0), idx(0) {}
};


void frameHandler(TY_FRAME_DATA* frame, void* userdata)
{
    CamInfo* pData = (CamInfo*) userdata;

    cv::Mat depth, irl, irr, color;
    parseFrame(*frame, &depth, &irl, &irr, &color);

    char win[64];
    if(!depth.empty()){
        cv::Mat colorDepth = pData->render.Compute(depth);
        sprintf(win, "depth-%s", pData->sn);
        cv::imshow(win, colorDepth);
    }
    if(!irl.empty()){
        sprintf(win, "LeftIR-%s", pData->sn);
        cv::imshow(win, irl);
    }
    if(!irr.empty()){
        sprintf(win, "RightIR-%s", pData->sn);
        cv::imshow(win, irr);
    }
    if(!color.empty()){
        sprintf(win, "color-%s", pData->sn);
        cv::imshow(win, color);
    }

    pData->idx++;

    LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
    ASSERT_OK( TYEnqueueBuffer(pData->hDev, frame->userBuffer, frame->bufferSize) );
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


void display_help() {
    printf("Usage:\n");
    printf("    -t <type>  : Select the device of the specified interface type (usb / net)\n");
    printf("    -list <sn1 sn2 sn3>: Select the device of the specified serial numbers\n");
    printf("\nExamples:\n");
    printf("   SimpleView_MultiDevice -t usb                            : Select all usb devices.\n");
    printf("   SimpleView_MultiDevice -list 207xxx1  207xxx2  207xxx3   : Select devices 207xxx1  207xxx2 207xxx3.\n");
}

int main(int argc, char* argv[])
{
    int32_t deviceType = TY_INTERFACE_ETHERNET | TY_INTERFACE_USB;

    std::vector<char*> list;
    int32_t cam_size = 0;
    int32_t found;
    bool    trigger_mode = false;

    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0) {
            display_help();
            return 0;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (argc == i+1) {
                LOGI("===== no type input");
                return 0;
            } else if (strcmp(argv[i+1], "usb") == 0) {
                LOGI("===== Select usb device");
                deviceType = TY_INTERFACE_USB;
            } else if (strcmp(argv[i+1], "net") == 0) {
                LOGI("===== Select ethernet device");
                deviceType = TY_INTERFACE_ETHERNET;
            }
        } else if (strcmp(argv[i], "-list") == 0) {
            if (argc == i+1) {
                LOGI("===== no list input");
                return 0;
            }
            int32_t j;
            for (j = 0;j < argc - (i + 1); j++) {
                list.push_back(argv[i+1+j]);
                LOGI("===== Select device %s", list[j]);
            }
        } else if (strcmp(argv[i], "-trigger") == 0) {
           trigger_mode = true;
        }
    }

    //printf("deviceType: %d\n", deviceType);

    LOGD("=== Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(deviceType, "", "", 100, selected) );
    
    //printf("size: %d\n", list.size()); 
    if (list.size()) {
        cam_size = list.size();
    } else {
        cam_size = selected.size();
    }

    std::vector<CamInfo> cams(cam_size);
    int32_t count = 0;
    for(uint32_t i = 0; i < selected.size(); i++){
        if (list.size()) {
            found = 0; 
            for(uint32_t j = 0; j < list.size(); j++){
               LOGD("=== check device: %s, %s", selected[i].id, list[j]);
               if (strcmp(selected[i].id, list[j]) == 0) {
                   LOGD("=== check device success");
                   found = 1; 
                   continue;
               } 
            }
           
            if (!found) {
                continue;
            } 
        }

        LOGD("=== Open device: %s", selected[i].id);
        strncpy(cams[count].sn, selected[i].id, sizeof(cams[count].sn));
        ASSERT_OK( TYOpenInterface(selected[i].iface.id, &cams[count].hIface) );
        ASSERT_OK( TYOpenDevice(cams[count].hIface, selected[i].id, &cams[count].hDev) );

        LOGD("=== Configure components, open depth cam");
        int componentIDs = TY_COMPONENT_DEPTH_CAM;
        ASSERT_OK( TYEnableComponents(cams[count].hDev, componentIDs) );

        //try to enable depth map
        LOGD("Configure components, open depth cam");
        if (componentIDs & TY_COMPONENT_DEPTH_CAM) {
            int32_t image_mode;
            ASSERT_OK(get_default_image_mode(cams[count].hDev, TY_COMPONENT_DEPTH_CAM, image_mode));
            LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
            ASSERT_OK(TYSetEnum(cams[count].hDev, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
            ASSERT_OK(TYEnableComponents(cams[count].hDev, TY_COMPONENT_DEPTH_CAM));
        }

        LOGD("=== Prepare image buffer");
        uint32_t frameSize;
        ASSERT_OK( TYGetFrameBufferSize(cams[count].hDev, &frameSize) );
        LOGD("     - Get size of framebuffer, %d", frameSize);

        LOGD("     - Allocate & enqueue buffers");
        cams[count].fb[0].resize(frameSize);
        cams[count].fb[1].resize(frameSize);
        LOGD("     - Enqueue buffer (%p, %d)", cams[count].fb[0].data(), frameSize);
        ASSERT_OK( TYEnqueueBuffer(cams[count].hDev, cams[count].fb[0].data(), frameSize) );
        LOGD("     - Enqueue buffer (%p, %d)", cams[count].fb[1].data(), frameSize);
        ASSERT_OK( TYEnqueueBuffer(cams[count].hDev, cams[count].fb[1].data(), frameSize) );

        LOGD("=== Register event callback");
        ASSERT_OK(TYRegisterEventCallback(cams[count].hDev, eventCallback, NULL));

        LOGD("=== Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        if(trigger_mode){
          bool hasResend;
          ASSERT_OK(TYHasFeature(cams[count].hDev, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, &hasResend));
          if (hasResend) {
            LOGD("=== Open resend");
            ASSERT_OK(TYSetBool(cams[count].hDev, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true));
          }
          trigger.mode = TY_TRIGGER_MODE_SLAVE;
        } else {
          trigger.mode = TY_TRIGGER_MODE_OFF;
        }
        ASSERT_OK(TYSetStruct(cams[count].hDev, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));

        LOGD("=== Start capture");
        ASSERT_OK( TYStartCapture(cams[count].hDev) );
        count++;
    }
    
    if (count != cam_size) {
        LOGD("Invalid ids in input id list"); 
        return 0;
    }

    LOGD("=== While loop to fetch frame");
    bool exit_main = false;
    int cam_index = 0;
    int triggered = 0;
    while(!exit_main) {
        if(trigger_mode && triggered == 0){
            for(size_t i = 0; i < cams.size(); i++){
                ASSERT_OK( TYSendSoftTrigger(cams[i].hDev) );
            }
            triggered = cams.size();
            LOGD("triggered once");
        }

        int err = TYFetchFrame(cams[cam_index].hDev, &cams[cam_index].frame, 10000);
        if( err != TY_STATUS_OK ) {
            LOGD("cam %s %d ... Drop one frame", cams[cam_index].sn, cams[cam_index].idx);
        } else {
            LOGD("cam %s %d got one frame", cams[cam_index].sn, cams[cam_index].idx);
            triggered--;

            frameHandler(&cams[cam_index].frame, &cams[cam_index]);

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
        }
        cam_index = (cam_index+1) % cams.size();
    }

    for(uint32_t i = 0; i < cams.size(); i++){
        ASSERT_OK( TYStopCapture(cams[i].hDev) );
        ASSERT_OK( TYCloseDevice(cams[i].hDev) );
        ASSERT_OK( TYCloseInterface(cams[i].hIface) );
    }
    ASSERT_OK( TYDeinitLib() );

    LOGD("=== Main done!");
    return 0;
}
