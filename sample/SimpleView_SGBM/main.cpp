#include "../common/common.hpp"

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE)
    {
        LOGD("=== Event Callback: Device Offline!");
        // Note:
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR)
    {
        LOGD("=== Event Callback: License Error!");
    }
}


static void init_cmd_argv(CommandLineFeatureHelper &ctx)
{
    ctx.add_feature("img_num", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_IMAGE_NUM, -1, "frames per depth");
    ctx.add_feature("disp_num", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_DISPARITY_NUM, -1, "disparity num");
    ctx.add_feature("disp_off", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_DISPARITY_OFFSET, -1, "disparity offset");
    ctx.add_feature("win_h", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_MATCH_WIN_HEIGHT, -1, "match win height");
    ctx.add_feature("p1", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_SEMI_PARAM_P1, -1, "semi p1");
    ctx.add_feature("p2", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_SEMI_PARAM_P2, -1, "semi p2");
    ctx.add_feature("uniq_factor", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_UNIQUE_FACTOR, -1, "uniqueness factor");
    ctx.add_feature("uniq_absdiff", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_UNIQUE_ABSDIFF, -1, "uniqueness min absoulut diff");
    ctx.add_feature("half_win", TY_COMPONENT_DEPTH_CAM, TY_BOOL_SGBM_HFILTER_HALF_WIN, -1, "enalble half window size");
    ctx.add_feature("win_w", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_MATCH_WIN_WIDTH, -1, "match win width");
    ctx.add_feature("med_filter", TY_COMPONENT_DEPTH_CAM, TY_BOOL_SGBM_MEDFILTER, -1, "enalble median filter");
    ctx.add_feature("lrc", TY_COMPONENT_DEPTH_CAM, TY_BOOL_SGBM_LRC, -1, "enalble left right consist check");
    ctx.add_feature("lrc_diff", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_LRC_DIFF, -1, "max left right consist diff");
    ctx.add_feature("med_thresh", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_MEDFILTER_THRESH, -1, "median filter thresh");
    ctx.add_feature("p1_scale", TY_COMPONENT_DEPTH_CAM, TY_INT_SGBM_SEMI_PARAM_P1_SCALE, -1, "semi param p1 scale");
    ctx.add_feature("id", 0, 0, "", "device id");
    ctx.add_feature("ip", 0, 0, "", "device ip");
    ctx.add_feature("color=off", 0, 0, "", "disable color image", true);
    ctx.add_feature("ir=off", 0, 0, "", "disable ir image", true);
    ctx.add_feature("depth=off", 0, 0, "", "disable depth image", true);
    ctx.add_feature("h", 0, 0, "", "print help message", true);
}

static CommandLineFeatureHelper param_ctx;

////////////////////////////////////////

int main(int argc, char *argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_ISP_HANDLE hColorIspHandle = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth; ///< flag for data stream 0: off, 1: on
    color = ir = depth = 1;

    init_cmd_argv(param_ctx);
    param_ctx.parse_argv(argc, argv);

    ID = param_ctx.get_feature("id")->get_str_val();
    IP = param_ctx.get_feature("ip")->get_str_val();
    color = !param_ctx.get_feature("color=off")->has_set;
    ir = !param_ctx.get_feature("ir=off")->has_set;
    depth = !param_ctx.get_feature("depth=off")->has_set;
    if (param_ctx.get_feature("h")->has_set)
    {
        //show command line parameter usage
        LOGD("%s", param_ctx.usage_describe().c_str());
        return 0;
    }

    if (!color && !depth && !ir)
    {
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
    TY_DEVICE_BASE_INFO &selectedDev = selected[0];

    ASSERT_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
    ASSERT_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));

    int32_t allComps;
    ASSERT_OK(TYGetComponentIDs(hDevice, &allComps));

    /// try to enable color camera
    if (allComps & TY_COMPONENT_RGB_CAM && color)
    {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
        // create a isp handle to convert raw image(color bayer format) to rgb image
        ASSERT_OK(TYISPCreate(&hColorIspHandle));
        // Init code can be modified in common.hpp
        // NOTE: Should set RGB image format & size before init ISP
        ASSERT_OK(ColorIspInitSetting(hColorIspHandle, hDevice));
        // You can  call follow function to show  color isp supported features
#if 0
        ColorIspShowSupportedFeatures(hColorIspHandle);
#endif
        // You can turn on auto exposure function as follow ,but frame rate may reduce .
        // Device may be casually stucked  1~2 seconds while software is trying to adjust device exposure time value
#if 0
        ASSERT_OK(ColorIspInitAutoExposure(hColorIspHandle, hDevice));
#endif
    }

    if (allComps & TY_COMPONENT_IR_CAM_LEFT && ir)
    {
        LOGD("Has IR left camera, open IR left cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }

    if (allComps & TY_COMPONENT_IR_CAM_RIGHT && ir)
    {
        LOGD("Has IR right camera, open IR right cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
    }

    // try to enable depth map
    LOGD("Configure components, open depth cam");
    DepthViewer depthViewer("Depth");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth)
    {
        int32_t image_mode;
        ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
        LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
        ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));

        // depth map pixel format is uint16_t ,which default unit is  1 mm
        // the acutal depth (mm)= PixelValue * ScaleUnit
        float scale_unit = 1.;
        TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
        depthViewer.depth_scale_unit = scale_unit;
    }

    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    char *frameBuffer[2];
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
    if (hasTrigger)
    {
        LOGD("Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
    }

    // set other device feature param from command line argv
    // ref header file : CommandLineFeatureHelper.hpp for detail
    param_ctx.set_device_feature(hDevice);

    LOGD("Start capture");
    ASSERT_OK(TYStartCapture(hDevice));

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    while (!exit_main)
    {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if (err == TY_STATUS_OK)
        {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0)
            {
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color, hColorIspHandle);
            if (!depth.empty())
            {
                depthViewer.show(depth);
            }
            if (!irl.empty())
            {
                cv::imshow("LeftIR", irl);
            }
            if (!irr.empty())
            {
                cv::imshow("RightIR", irr);
            }
            if (!color.empty())
            {
                cv::imshow("Color", color);
            }

            int key = cv::waitKey(1);
            switch (key & 0xff)
            {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            default:
                LOGD("Unmapped key %d", key);
            }

            TYISPUpdateDevice(hColorIspHandle);
            LOGD("Re-enqueue buffer(%p, %d)", frame.userBuffer, frame.bufferSize);
            ASSERT_OK(TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize));
        }
    }
    ASSERT_OK(TYStopCapture(hDevice));
    ASSERT_OK(TYCloseDevice(hDevice));
    ASSERT_OK(TYCloseInterface(hIface));
    ASSERT_OK(TYISPRelease(&hColorIspHandle));
    ASSERT_OK(TYDeinitLib());
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
