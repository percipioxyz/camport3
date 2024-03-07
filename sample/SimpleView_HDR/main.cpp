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

static inline int covertToLinear(cv::Mat &raw16, cv::Mat raw32,
    uint32_t *param)
{
    uint16_t *src = (uint16_t *)raw16.data;
    int *dst = (int *)raw32.data;
    uint32_t R1 = param[0];
    uint32_t R2 = param[1];
    R1 = 1 << (R1 + 2);
    R2 = 1 << (R2 + 2);
    uint32_t P1 = 1 << param[6];
    uint32_t P2 = 1 << param[7];
    uint32_t Pk = (int)((P2 - P1)/(4.0f*R1)) + P1;
    for (int i = 0; i < raw16.cols * raw16.rows; i++) {
        if (src[i] <= P1) {
            dst[i] = (int)src[i];
        } else if (src[i] > P1 && src[i] <= Pk) {
            dst[i] = ((int)src[i] - P1)* 4* R1 + P1;
        } else {
            dst[i] = ((int)src[i] - Pk)* 4* R1 * R2 + P2;
        }
    }
    return 0;
}

static inline int parseHDRRaw10(TY_FRAME_DATA& frame,
    cv::Mat* pColor, uint32_t *param)
{
    if (pColor == NULL) {
        return -1;
    }
    for(int idx=0;idx<frame.validCount;idx++){
        TY_IMAGE_DATA &img = frame.image[idx];
        if(img.componentID == TY_COMPONENT_RGB_CAM && 
                             img.pixelFormat == TY_PIXEL_FORMAT_CSI_MONO10){
            cv::Mat raw16 = cv::Mat(img.height,img.width,CV_16U);
            cv::Mat raw32 = cv::Mat(img.height,img.width,CV_32S);
            //decode csi_mono10 to gray16
            decodeCsiRaw10((uchar *)img.buffer, (ushort *)raw16.data,
                img.width, img.height);
            //recover 10bit hdr data to 12 bit hdr Data
            raw16 = raw16 * 4;
            //covert compressed 12bit hdr data to 20 bit linear data
            covertToLinear(raw16, raw32, param);
            //TODO: shitft the 16bit with most details to the lsb for force convert, will show most details when preview
            //raw32 = raw32 / (1 << 2);
            //Here May overflow, force type convert here
            raw32.convertTo(raw32, CV_16U);
            *pColor = raw32.clone();
            break;
        }
    }
    return 0;
}

static void save_frame_to_file(cv::Mat left, cv::Mat right,
        cv::Mat depth, cv::Mat color,cv::Mat raw) {
    char buff[100];
    static int save_index = 0;
    if (!left.empty()) {
        sprintf(buff, "%d-left.png", save_index);
        cv::imwrite(buff, left);
    }
    if (!right.empty()) {
        sprintf(buff, "%d-right.png", save_index);
        cv::imwrite(buff, right);
    }
    if (!depth.empty()) {
        //sprintf(buff, "%d-depth.bin", save_index);
        //std::ofstream ofs(buff, std::ios::binary);
        //ofs.write((const char*)&depth.cols, sizeof(depth.cols));
        //ofs.write((const char*)&depth.rows, sizeof(depth.rows));
        //ofs.write((const char*)depth.data, 2 * depth.rows * depth.cols);
        ////ofs << depth;
        //ofs.close();

        sprintf(buff, "%d-depth.png", save_index);
        cv::imwrite(buff, depth);

    }
    if (!color.empty()) {
        sprintf(buff, "%d-color.png", save_index);
        cv::imwrite(buff, color);
    }
    if (!raw.empty()) {
        sprintf(buff, "%d-raw.png", save_index);
        cv::imwrite(buff, raw);
    }
    printf("saved image data index = %d\n", save_index);
    save_index++;
}


int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;
    int R1 = 0, R2 = 0;
    bool hdr_enable = true;
    int expo = -1;

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
        } else if(strcmp(argv[i], "-R1") == 0) {
            R1 = atoi(argv[++i]);
            if (R1 > 2) {
                LOGD("R1 %d is out of range force to 2\n", R1);
                R1 = 2;
            }
        } else if(strcmp(argv[i], "-R2") == 0) {
            R2 = atoi(argv[++i]);
            if (R2 > 2) {
                LOGD("R2 %d is out of range force to 2\n", R2);
                R2 = 2;
            }
        } else if(strcmp(argv[i], "-HDR") == 0) {
            hdr_enable = atoi(argv[++i]) == 0 ? false : true;
        } else if (strcmp(argv[i], "-expo") == 0) {
            expo = atoi(argv[++i]);
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_HDR [-h] [-id <ID>] [-HDR en] [-R1 r1] [-R2 r2] [-expo ex]");
            return 0;
        }
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

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );

    ///try to enable color camera
    if(allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );
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
    bool hasHDR = false;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_HDR, &hasHDR));
    if (hasHDR) {
        ASSERT_OK(TYSetBool(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_HDR, hdr_enable));
    }
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, &hasHDR));
    if (hdr_enable && hasHDR) {
        uint32_t hdr_para[8];
        hdr_para[0] = R1;
        hdr_para[1] = R2;
        ASSERT_OK(TYSetByteArray(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, (uint8_t *)&hdr_para[0], 32));
        LOGD("set hdr param R1 %d, R2 %d\n", R1, R2);
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

    if (!(expo < 0)) {
        ASSERT_OK(TYSetInt(hDevice, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, expo));
    }
    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    uint32_t hdr_param[8];
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            if (hdr_enable && hasHDR) {
                uint32_t _hdr_param[8];
                memset(_hdr_param, 0, sizeof(_hdr_param));
                //May changed first frame after adjust R1 R2
                //can disabled when is stabled, seems to depends on R1 R2
                TYGetByteArray(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, (uint8_t *)&_hdr_param[0], 32);
                if (hdr_param[0] != _hdr_param[0] ||
                    hdr_param[1] != _hdr_param[1] ||
                    hdr_param[6] != _hdr_param[6] ||
                    hdr_param[7] != _hdr_param[7]) {
                    LOGD("hdr param changed {%u %u %u %u}->{%u %u %u %u}",
                    hdr_param[0],hdr_param[1],hdr_param[6],hdr_param[7],
                    _hdr_param[0],_hdr_param[1],_hdr_param[6],_hdr_param[7]);
                    memcpy(hdr_param, _hdr_param, sizeof(_hdr_param));
                }
                //Color cannot use normal parse as is HDR
                parseFrame(frame, &depth, &irl, &irr, NULL, NULL);
                parseHDRRaw10(frame, &color, &_hdr_param[0]);
            } else {
                parseFrame(frame, &depth, &irl, &irr, &color, NULL);
            }
            if(!depth.empty()){
                depthViewer.show(depth);
            }
            if(!irl.empty()){ cv::imshow("LeftIR", irl); }
            if(!irr.empty()){ cv::imshow("RightIR", irr); }
            if(!color.empty()){ 
                cv::imshow("Color", color);
            }

            cv::Mat raw;
            int key = cv::waitKey(1);
            switch(key & 0xff) {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            case 's':
                save_frame_to_file(irl, irr, depth, color, raw);
                break;
            default:
                LOGD("Unmapped key %d", key);
            }

            LOGD("Re-enqueue buffer(%p, %d)"
                , frame.userBuffer, frame.bufferSize);
            ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );
        }
    }
    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
