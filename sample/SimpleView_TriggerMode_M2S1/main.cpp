#include "../common/common.hpp"


struct CamInfo
{
    char                sn[32];
    std::string         tag;
    TY_DEV_HANDLE       hIface;
    TY_DEV_HANDLE       hDev;
    char*               fb[2];
    TY_FRAME_DATA       frame;
    int                 idx;
    DepthRender         render;

    CamInfo() : hDev(0), idx(0) {fb[0]=0; fb[1]=0;}
};


void frameHandler(TY_FRAME_DATA* frame, void* userdata)
{
    CamInfo* pData = (CamInfo*) userdata;

    cv::Mat depth, irl, irr, color;
    parseFrame(*frame, &depth, &irl, &irr, &color);

    char win[64];
    if(!depth.empty()){
        cv::Mat colorDepth = pData->render.Compute(depth);
        sprintf(win, "depth-%s", pData->tag.c_str());
        cv::imshow(win, colorDepth);
    }
    if(!irl.empty()){
        sprintf(win, "LeftIR-%s", pData->tag.c_str());
        cv::imshow(win, irl);
    }
    if(!irr.empty()){
        sprintf(win, "RightIR-%s", pData->tag.c_str());
        cv::imshow(win, irr);
    }
    if(!color.empty()){
        sprintf(win, "color-%s", pData->tag.c_str());
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

int main(int argc, char* argv[])
{
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: %s [-h]", argv[0]);
            return 0;
        }
    }

    LOGD("=== Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selectedDev;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, "", "", 10, selectedDev) );

    std::vector<CamInfo> cams(selectedDev.size());
    for(uint32_t i = 0; i < selectedDev.size(); i++){
        LOGD("=== Open device: %s", selectedDev[i].id);
        strncpy(cams[i].sn, selectedDev[i].id, sizeof(cams[i].sn));
        ASSERT_OK( TYOpenInterface(selectedDev[i].iface.id, &cams[i].hIface) );
        ASSERT_OK( TYOpenDevice(cams[i].hIface, selectedDev[i].id, &cams[i].hDev) );

        LOGD("=== Configure components, open depth cam");
        int componentIDs = TY_COMPONENT_DEPTH_CAM;
        ASSERT_OK( TYEnableComponents(cams[i].hDev, componentIDs) );

        LOGD("=== Configure feature, set resolution to 640x480.");
        int err = TYSetEnum(cams[i].hDev, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_DEPTH16_640x480);
        ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);

        LOGD("=== Prepare image buffer");
        uint32_t frameSize;
        ASSERT_OK( TYGetFrameBufferSize(cams[i].hDev, &frameSize) );
        LOGD("     - Get size of framebuffer, %d", frameSize);
        ASSERT( frameSize >= 640 * 480 * 2 );

        LOGD("     - Allocate & enqueue buffers");
        cams[i].fb[0] = new char[frameSize];
        cams[i].fb[1] = new char[frameSize];
        LOGD("     - Enqueue buffer (%p, %d)", cams[i].fb[0], frameSize);
        ASSERT_OK( TYEnqueueBuffer(cams[i].hDev, cams[i].fb[0], frameSize) );
        LOGD("     - Enqueue buffer (%p, %d)", cams[i].fb[1], frameSize);
        ASSERT_OK( TYEnqueueBuffer(cams[i].hDev, cams[i].fb[1], frameSize) );

        LOGD("=== Register event callback");
        ASSERT_OK(TYRegisterEventCallback(cams[i].hDev, eventCallback, NULL));

        LOGD("=== Set trigger mode");
        if(0 == i ){
            cams[i].tag = std::string(cams[i].sn) + "_master";
            TY_TRIGGER_PARAM param;
            param.mode = TY_TRIGGER_MODE_M_SIG;
            ASSERT_OK(TYSetStruct(cams[i].hDev, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, (void*)&param, sizeof(param)));
        }else{
            cams[i].tag = std::string(cams[i].sn) + "_slave";
            TY_TRIGGER_PARAM param;
            param.mode = TY_TRIGGER_MODE_SLAVE;
            ASSERT_OK(TYSetStruct(cams[i].hDev, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, (void*)&param, sizeof(param)));
        }

        LOGD("=== Start capture");
        ASSERT_OK( TYStartCapture(cams[i].hDev) );
    }

    LOGD("=== While loop to fetch frame");
    bool exit_main = false;
    int cam_index = 0;
    while(!exit_main) {
        int err = TYFetchFrame(cams[cam_index].hDev, &cams[cam_index].frame, 20000);
        if( err != TY_STATUS_OK ) {
            LOGD("cam %s %d ... Drop one frame", cams[cam_index].sn, cams[cam_index].idx);
        } else {
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
        delete cams[i].fb[0];
        delete cams[i].fb[1];
    }
    ASSERT_OK( TYDeinitLib() );

    LOGD("=== Main done!");
    return 0;
}
