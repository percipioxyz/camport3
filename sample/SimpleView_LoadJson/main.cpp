
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
//#include "rapidjson/document.h"
//#include "rapidjson/prettywriter.h"
//#include "rapidjson/stringbuffer.h"
#include "../common/common.hpp"
#include "json11.hpp"

using namespace json11;

TY_STATUS write_int_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetInt(hDevice, comp, feat, static_cast<int>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_float_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetFloat(hDevice, comp, feat, static_cast<float>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_enum_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetEnum(hDevice, comp, feat, static_cast<uint>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_bool_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_bool())
        return TYSetBool(hDevice, comp, feat, value.bool_value());
    else
        return TY_STATUS_ERROR;
}

bool json_parse_arrar(const Json& value, std::vector<char>& buff)
{
    buff.clear();
    if(value.is_array()) {
        size_t size = value.array_items().size();
        std::vector<char> buff(size+1);
        for(size_t i = 0; i < size; i++)
            buff[i] = static_cast<char>(value[i].number_value());
        return true;
    } else {
        return false;
    }
}

TY_STATUS write_string_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        buff.push_back(0);
        return TYSetString(hDevice, comp, feat, &buff[0]);
    } else {
        return TY_STATUS_ERROR;
    }
}

TY_STATUS write_bytearray_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        return TYSetByteArray(hDevice, comp, feat, (uint8_t*)(&buff[0]), buff.size());
    } else {
        return TY_STATUS_ERROR;
    }
}

TY_STATUS write_struct_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        return TYSetStruct(hDevice, comp, feat, (void*)(&buff[0]), buff.size());
    } else {
        return TY_STATUS_ERROR;
    }
}


TY_STATUS device_write_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    TY_STATUS status = TY_STATUS_OK;
    TY_FEATURE_TYPE type = TYFeatureType(feat);
    switch (type)
    {
    case TY_FEATURE_INT:
        status = write_int_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_FLOAT:
        status = write_float_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_ENUM:
        status = write_enum_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_BOOL:
        status = write_bool_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_STRING:
        status = write_string_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_BYTEARRAY:
        status = write_bytearray_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_STRUCT:
        status = write_struct_feature(hDevice, comp, feat, value);
        break;
    default:
        status = TY_STATUS_INVALID_FEATURE;
        break;
    }
    return status;
}

void json_parse(const TY_DEV_HANDLE hDevice, const char* jscode)
{
    std::string err;
    const auto json = Json::parse(jscode, err);
    LOGD("Device sn:\t\t%s", json["sn"].string_value().c_str());
    LOGD("Sdk version:\t\t%s", json["version"].string_value().c_str());
    LOGD("Timestamp:\t\t%s", json["timestamp"].string_value().c_str());

    Json components = json["component"];
    if(components.is_array()) {
        for (auto &k : components.array_items()) {
            const Json& comp_id = k["id"];
            const Json& comp_desc = k["desc"];
            const Json& features = k["feature"];

            if(!comp_id.is_string()) continue;
            if(!comp_desc.is_string()) continue;
            if(!features.is_array()) continue;

            const char* comp_desc_str = comp_desc.string_value().c_str();
            const char* comp_id_str   = comp_id.string_value().c_str();

            TY_COMPONENT_ID m_comp_id;
            sscanf(comp_id_str,"%x",&m_comp_id);

            LOGD("\tComp ID : 0x%08x", m_comp_id);
            LOGD("\tDesc : %s", comp_desc_str);

            for (auto &f : features.array_items()) {
                const Json& feat_name   = f["name"];
                const Json& feat_id     = f["id"];
                const Json& feat_value  = f["value"];

                if(!feat_id.is_string()) continue;
                if(!feat_name.is_string()) continue;

                const char* feat_name_str = feat_name.string_value().c_str();
                const char* feat_id_str = feat_id.string_value().c_str();

                TY_FEATURE_ID m_feat_id;
                sscanf(feat_id_str,"%x",&m_feat_id);

                LOGD("\t\tFeat ID : 0x%08x", m_feat_id);
                LOGD("\t\tFeat Name : %s", feat_name_str);
                device_write_feature(hDevice, m_comp_id, m_feat_id, feat_value);
            }
        }
    }
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
    std::string json_file;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_ISP_HANDLE hColorIspHandle = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;

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
        } else if(strcmp(argv[i], "-json") == 0) {
            json_file = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_LoadJson [-h] [-id <ID>] [-ip <IP>] [-json <FILE>]");
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

    if(!json_file.empty()) {
        std::ifstream ifs(json_file);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        ifs.close();

        std::string text(buffer.str());
        json_parse(hDevice, text.c_str());
    }

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
