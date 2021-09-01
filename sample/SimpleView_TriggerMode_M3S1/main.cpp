#include "../common/common.hpp"
#include <signal.h>

static bool exit_main = false;
static bool capture_started = false;

struct CamInfo
{
    char                sn[32];
    std::string         tag;
    TY_INTERFACE_HANDLE hIface;
    TY_DEV_HANDLE       hDev;
    std::vector<char>   fb[6];
    TY_FRAME_DATA       frame;
    int                 idx;
    DepthRender         render;

    CamInfo() : hDev(0), idx(0) {}
};


void frameHandler(TY_FRAME_DATA* frame, void* userdata)
{
    CamInfo* pData = (CamInfo*)userdata;

    cv::Mat depth, irl, irr, color;
    parseFrame(*frame, &depth, &irl, &irr, &color);

    char win[64];
    if (!depth.empty()) {
        cv::Mat colorDepth = pData->render.Compute(depth);
        sprintf(win, "depth-%s", pData->tag.c_str());
        cv::imshow(win, colorDepth);
    }
    if (!irl.empty()) {
        sprintf(win, "LeftIR-%s", pData->tag.c_str());
        cv::imshow(win, irl);
    }
    if (!irr.empty()) {
        sprintf(win, "RightIR-%s", pData->tag.c_str());
        cv::imshow(win, irr);
    }
    if (!color.empty()) {
        sprintf(win, "color-%s", pData->tag.c_str());
        cv::imshow(win, color);
    }

    pData->idx++;

    LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
    ASSERT_OK(TYEnqueueBuffer(pData->hDev, frame->userBuffer, frame->bufferSize));
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

void signalHandle(int signum)
{
    LOGD("Interrupt signal %d received", signum);
    signal(SIGINT, signalHandle);
    if (capture_started) {
        exit_main = true;
    } else {
        ASSERT_OK(TYDeinitLib());
        exit(0);
    }
}

int main(int argc, char* argv[])
{
    int32_t resend = 1;
    int32_t deviceType = TY_INTERFACE_ETHERNET | TY_INTERFACE_USB;
    std::vector<char*> list;
    int32_t cam_size = 0;
    int32_t found;
    bool    trigger_mode = false;


    if ((argc < 2) || (strcmp(argv[1], "-list") != 0)) {
        LOGI("Usage: %s -list masterSN [slaveSN ......]", argv[0]);
        return 0;
    }

    for(int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: %s -list [xxx, xxx, ....] [-h]", argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-list") == 0) {
            if (argc == i + 1) {
                LOGI("===== no list input");
                return 0;
            }
            int32_t j;
            for (j = 0; j < argc - (i + 1); j++) {
                list.push_back(argv[i + 1 + j]);
                LOGI("===== Select device %s", list[j]);
            }
        }
    }

    signal(SIGINT, signalHandle);

    LOGD("=== Init lib");
    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK(TYLibVersion(&ver));
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK(selectDevice(deviceType, "", "", 100, selected));

    //printf("size: %d\n", list.size()); 
    if (list.size()) {
        cam_size = list.size();
    }
    else {
        cam_size = selected.size();
    }

    std::vector<CamInfo> cams(cam_size);
    int32_t count = 0;
    for (uint32_t i = 0; i < selected.size(); i++) {
        if (list.size()) {
            found = 0;
            for (uint32_t j = 0; j < list.size(); j++) {
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
        ASSERT_OK(TYOpenInterface(selected[i].iface.id, &cams[count].hIface));
        ASSERT_OK(TYOpenDevice(cams[count].hIface, selected[i].id, &cams[count].hDev));

        LOGD("=== Configure components, open depth cam");
        int componentIDs = TY_COMPONENT_DEPTH_CAM;
        ASSERT_OK(TYEnableComponents(cams[count].hDev, componentIDs));

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
        ASSERT_OK(TYGetFrameBufferSize(cams[count].hDev, &frameSize));
        LOGD("     - Get size of framebuffer, %d", frameSize);

        LOGD("     - Allocate & enqueue buffers");
        for (int i = 0; i < 6; i++) {
            cams[count].fb[i].resize(frameSize);
            LOGD("     - Enqueue buffer (%p, %d)", cams[count].fb[i].data(), frameSize);
            ASSERT_OK(TYEnqueueBuffer(cams[count].hDev, cams[count].fb[i].data(), frameSize));
        }

        LOGD("=== Register event callback");
        ASSERT_OK(TYRegisterEventCallback(cams[count].hDev, eventCallback, NULL));

        LOGD("=== Set trigger mode");

        if (((strcmp(selected[i].id, list[0]) == 0) && (list.size() > 0))
            || ((count == 0) && (list.size() == 0))) {
            LOGD("=== set master device");
            cams[count].tag = std::string(cams[count].sn) + "_master";
            TY_TRIGGER_PARAM param;
            param.mode = TY_TRIGGER_MODE_M_PER;
            param.fps = 5;
            ASSERT_OK(TYSetStruct(cams[count].hDev, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, (void*)&param, sizeof(param)));
        }
        else {
            cams[count].tag = std::string(cams[count].sn) + "_slave";
            TY_TRIGGER_PARAM param;
            param.mode = TY_TRIGGER_MODE_SLAVE;
            ASSERT_OK(TYSetStruct(cams[count].hDev, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, (void*)&param, sizeof(param)));
        }

        //for network only
        LOGD("=== resend: %d", resend);
        if (resend) {
            bool hasResend;
            ASSERT_OK(TYHasFeature(cams[count].hDev, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, &hasResend));
            if (hasResend) {
                LOGD("=== Open resend");
                ASSERT_OK(TYSetBool(cams[count].hDev, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true));
            }
            else {
                LOGD("=== Not support feature TY_BOOL_GVSP_RESEND");
            }
        }

        //LOGD("=== Start capture");
        //ASSERT_OK(TYStartCapture(cams[count].hDev));
        count++;
    }


    if (count != cam_size) {
        LOGD("Invalid ids in input id list");
        return 0;
    }

    LOGD("=== Start capture for salve");
    for (uint32_t i = 0; i < cams.size(); i++) {
        if (cams[i].tag.compare(13, 6, "master") != 0) {
            ASSERT_OK(TYStartCapture(cams[i].hDev));
        }
    }

    LOGD("=== Start capture for master");
    for (uint32_t i = 0; i < cams.size(); i++) {
        if (cams[i].tag.compare(13, 6, "master") == 0) {
            ASSERT_OK(TYStartCapture(cams[i].hDev));
        }
    }

    MSLEEP(1000);

    LOGD("=== While loop to fetch frame");
    capture_started = true;
    exit_main = false;
    int cam_index = 0;
    while (!exit_main) {
        int err = TYFetchFrame(cams[cam_index].hDev, &cams[cam_index].frame, 20000);
        if (err != TY_STATUS_OK) {
            LOGD("cam %s %d ... Drop one frame", cams[cam_index].sn, cams[cam_index].idx);
        }
        else {
            LOGD("cam %s %d got one frame", cams[cam_index].sn, cams[cam_index].idx);

            frameHandler(&cams[cam_index].frame, &cams[cam_index]);

            int key = cv::waitKey(1);
            switch (key & 0xff) {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            default:
                LOGD("Unmapped key %d", key);
            }
        }
        cam_index = (cam_index + 1) % cams.size();
    }

    for (uint32_t i = 0; i < cams.size(); i++) {
        ASSERT_OK(TYStopCapture(cams[i].hDev));
        ASSERT_OK(TYCloseDevice(cams[i].hDev));
        ASSERT_OK(TYCloseInterface(cams[i].hIface));
    }
    ASSERT_OK(TYDeinitLib());

    LOGD("=== Main done!");
    return 0;
}
