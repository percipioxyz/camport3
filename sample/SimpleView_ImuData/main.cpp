#include "../common/common.hpp"
#include <inttypes.h>

void eventCallback(TY_EVENT_INFO* event_info, void* userdata)
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

void imuCallback(TY_IMU_DATA* imu_data, void* userdata)
{
    static int imu_data_count = 0;

    imu_data_count++;
    if ((imu_data_count % 400) == 0) {
        LOGD("[COUNT] %d [ACC] %4.2f %4.2f %4.2f [GYRO] %4.2f %4.2f %4.2f [TEMP] %4.2f [TS] %" PRIu64 "",
            imu_data_count,
            imu_data->acc_x,
            imu_data->acc_y,
            imu_data->acc_z,
            imu_data->gyro_x,
            imu_data->gyro_y,
            imu_data->gyro_z,
            imu_data->temperature,
            imu_data->timestamp);
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_ISP_HANDLE hColorIspHandle = NULL;
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
            LOGI("Usage: SimpleView_ImuData [-h] [-id <ID>] [-ip <IP>]");
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

    ///try to enable color camera
    if (allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
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

    LOGD("=== Set IMU data onoff");
    ASSERT_OK(TYSetBool(hDevice, TY_COMPONENT_IMU, TY_BOOL_IMU_DATA_ONOFF, true));
    LOGD("=== Register IMU callback");
    ASSERT_OK(TYRegisterImuCallback(hDevice, imuCallback, NULL));

    TY_ACC_BIAS acc_bias;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_ACC_BIAS
        , &acc_bias, sizeof(TY_ACC_BIAS)));
    LOGD("acc_bias: %f %f %f",
        acc_bias.data[0],
        acc_bias.data[1],
        acc_bias.data[2]);

    TY_ACC_MISALIGNMENT acc_misalignment;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_ACC_MISALIGNMENT
        , &acc_misalignment, sizeof(TY_ACC_MISALIGNMENT)));
    LOGD("acc_misalignment: %f %f %f %f %f %f %f %f %f",
        acc_misalignment.data[0],
        acc_misalignment.data[1],
        acc_misalignment.data[2],
        acc_misalignment.data[3],
        acc_misalignment.data[4],
        acc_misalignment.data[5],
        acc_misalignment.data[6],
        acc_misalignment.data[7],
        acc_misalignment.data[8]);

    TY_ACC_SCALE acc_scale;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_ACC_SCALE
        , &acc_scale, sizeof(TY_ACC_SCALE)));
    LOGD("acc_scale: %f %f %f %f %f %f %f %f %f",
        acc_scale.data[0],
        acc_scale.data[1],
        acc_scale.data[2],
        acc_scale.data[3],
        acc_scale.data[4],
        acc_scale.data[5],
        acc_scale.data[6],
        acc_scale.data[7],
        acc_scale.data[8]);

    TY_GYRO_BIAS gyro_bias;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_GYRO_BIAS
        , &gyro_bias, sizeof(TY_GYRO_BIAS)));
    LOGD("gyro_bias: %f %f %f",
        gyro_bias.data[0],
        gyro_bias.data[1],
        gyro_bias.data[2]);

    TY_GYRO_MISALIGNMENT gyro_misalignment;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_GYRO_MISALIGNMENT
        , &gyro_misalignment, sizeof(TY_GYRO_MISALIGNMENT)));
    LOGD("gyro_misalignment: %f %f %f %f %f %f %f %f %f",
        gyro_misalignment.data[0],
        gyro_misalignment.data[1],
        gyro_misalignment.data[2],
        gyro_misalignment.data[3],
        gyro_misalignment.data[4],
        gyro_misalignment.data[5],
        gyro_misalignment.data[6],
        gyro_misalignment.data[7],
        gyro_misalignment.data[8]);

    TY_GYRO_SCALE gyro_scale;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_GYRO_SCALE
        , &gyro_scale, sizeof(TY_GYRO_SCALE)));
    LOGD("gyro_scale: %f %f %f %f %f %f %f %f %f",
        gyro_scale.data[0],
        gyro_scale.data[1],
        gyro_scale.data[2],
        gyro_scale.data[3],
        gyro_scale.data[4],
        gyro_scale.data[5],
        gyro_scale.data[6],
        gyro_scale.data[7],
        gyro_scale.data[8]);

    TY_CAMERA_TO_IMU cam_to_imu;
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_IMU, TY_STRUCT_IMU_CAM_TO_IMU
        , &cam_to_imu, sizeof(TY_CAMERA_TO_IMU)));
    LOGD("cam_to_imu: %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
        cam_to_imu.data[0],
        cam_to_imu.data[1],
        cam_to_imu.data[2],
        cam_to_imu.data[3],
        cam_to_imu.data[4],
        cam_to_imu.data[5],
        cam_to_imu.data[6],
        cam_to_imu.data[7],
        cam_to_imu.data[8],
        cam_to_imu.data[9],
        cam_to_imu.data[10],
        cam_to_imu.data[11],
        cam_to_imu.data[12],
        cam_to_imu.data[13],
        cam_to_imu.data[14],
        cam_to_imu.data[15]);

    LOGD("Start capture");
    ASSERT_OK(TYStartCapture(hDevice));

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    while (!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if (err == TY_STATUS_OK) {
            LOGD("=== Get frame %d(%" PRIu64 ")", ++index, frame.image[0].timestamp);

            int fps = get_fps();
            if (fps > 0) {
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color, hColorIspHandle);
            if (!depth.empty()) {
                depthViewer.show(depth);
            }
            if (!irl.empty()) { cv::imshow("LeftIR", irl); }
            if (!irr.empty()) { cv::imshow("RightIR", irr); }
            if (!color.empty()) { cv::imshow("Color", color); }

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

            TYISPUpdateDevice(hColorIspHandle);
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
