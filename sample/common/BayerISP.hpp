#ifndef SAMPLE_COMMON_ISP_HPP_
#define SAMPLE_COMMON_ISP_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include "TyIsp.h"

/**
 *The RGB image data output by some cameras is the original Bayer array. 
 *By calling the API provided by this file, Bayer data can be converted to BGR array.
 *You can refer to the sample code: SimpleView_FetchFrame.
*/

static int __TYCompareFirmwareVersion(const TY_DEVICE_BASE_INFO &info, int major, int minor){
    const TY_VERSION_INFO &v = info.firmwareVersion;
    if (v.major < major){
        return -1;
    }
    if (v.major == major && v.minor < minor){
        return -1;
    }
    if (v.major == major && v.minor == minor){
        return 0;
    }
    return 1;
}

static TY_STATUS __TYDetectOldVer21ColorCam(TY_DEV_HANDLE dev_handle,bool *is_v21_color_device){
    TY_DEVICE_BASE_INFO info;
    TY_STATUS res = TYGetDeviceInfo(dev_handle, &info);
    if (res != TY_STATUS_OK){
        LOGI("get device info failed");
        return res;
    }
    *is_v21_color_device = false;
    if (info.iface.type == TY_INTERFACE_USB){
        *is_v21_color_device = true;
    }
    if ((info.iface.type == TY_INTERFACE_ETHERNET || info.iface.type == TY_INTERFACE_RAW) &&
        __TYCompareFirmwareVersion(info, 2, 2) < 0){
        *is_v21_color_device = true;
    }
    return TY_STATUS_OK;
}

static void __TYParseSizeFromImageMode(TY_IMAGE_MODE mode , int *image_size) {
    const int mask = ((0x01 << 12) - 1);
    int height = mode & mask;
    int width = (mode >> 12) & mask;
    image_size[0] = width;
    image_size[1] = height;

}

///init color isp setting
///for bayer raw image process
static TY_STATUS ColorIspInitSetting(TY_ISP_HANDLE isp_handle, TY_DEV_HANDLE dev_handle){
    bool is_v21_color_device ;
    TY_STATUS res = __TYDetectOldVer21ColorCam(dev_handle, &is_v21_color_device);//old version device has different config
    if (res != TY_STATUS_OK){
        return res;
    }
    if (is_v21_color_device){
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_BLACK_LEVEL, 11));
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_BLACK_LEVEL_GAIN, 256.f / (256 - 11)));
    }
    else{
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_BLACK_LEVEL, 0));
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_BLACK_LEVEL_GAIN, 1.f));
        bool b;
        ASSERT_OK(TYHasFeature(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_ANALOG_GAIN, &b));
        if (b){
            TYSetInt(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_ANALOG_GAIN, 1);
        }
    }
    TYISPSetFeature(isp_handle, TY_ISP_FEATURE_BAYER_PATTERN, TY_ISP_BAYER_AUTO);
    float shading[9] = { 0.30890417098999026, 10.63355541229248, -6.433426856994629,
                         0.24413758516311646, 11.739893913269043, -8.148622512817383,
                         0.1255662441253662, 11.88359546661377, -7.865192413330078 };
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_SHADING, (uint8_t*)shading, sizeof(shading)));
    int shading_center[2] = { 640, 480 };
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_SHADING_CENTER, (uint8_t*)shading_center, sizeof(shading_center)));
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_CCM_ENABLE, 0));//we are not using ccm by default
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_CAM_DEV_HANDLE, (uint8_t*)&dev_handle, sizeof(dev_handle)));
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_CAM_DEV_COMPONENT, int32_t(TY_COMPONENT_RGB_CAM)));
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_GAMMA, 1.f));
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTOBRIGHT, 1));//enable auto bright control
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_ENABLE_AUTO_EXPOSURE_GAIN, 0));//disable ae by default
    int default_image_size[2] = { 1280, 960 };// image size 
    int current_image_size[2] = { 1280, 960 };// image size for current parameters
    TY_IMAGE_MODE img_mode;
#if 1
    res = TYGetEnum(dev_handle, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, &img_mode);
    if (res == TY_STATUS_OK) {
        __TYParseSizeFromImageMode(img_mode, current_image_size);
    }
    TY_ENUM_ENTRY mode_entry[10];
    uint32_t num;
    res = TYGetEnumEntryInfo(dev_handle, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, mode_entry, 10, &num);
    if (res == TY_STATUS_OK) {
        __TYParseSizeFromImageMode(mode_entry[0].value, default_image_size);
    }

#else
    //some device may not support WIDTH & HEIGHT feature. image mode is recommended
    TYGetInt(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_WIDTH, &image_size[0]);
    TYGetInt(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_HEIGHT, &image_size[1]);
#endif
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_IMAGE_SIZE, (uint8_t*)&default_image_size, sizeof(default_image_size)));//the orignal raw image size
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_INPUT_RESAMPLE_SCALE, default_image_size[0] / current_image_size[0]));//resampled input 
#if 1
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_ENABLE_AUTO_WHITEBALANCE, 1)); //eanble auto white balance
#else
    //manual wb gain control
    const float wb_rgb_gain[3] = { 2.0123140811920168, 1, 1.481866478919983 };
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_WHITEBALANCE_GAIN, (uint8_t*)wb_rgb_gain, sizeof(wb_rgb_gain)));
#endif

    //try to load specifical device config from  device storage
    int32_t comp_all;
    ASSERT_OK(TYGetComponentIDs(dev_handle, &comp_all));
    if (!(comp_all & TY_COMPONENT_STORAGE)){
        return TY_STATUS_OK;
    }
    bool has_isp_block = false;
    ASSERT_OK(TYHasFeature(dev_handle, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, &has_isp_block));
    if (!has_isp_block){
        return TY_STATUS_OK;
    }
    uint32_t sz = 0;
    ASSERT_OK(TYGetByteArraySize(dev_handle, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, &sz));
    if (sz <= 0){
        return TY_STATUS_OK;
    }
    std::vector<uint8_t> buff(sz);
    ASSERT_OK(TYGetByteArray(dev_handle, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, &buff[0], buff.size()));
    res = TYISPLoadConfig(isp_handle, &buff[0], buff.size());
    if (res == TY_STATUS_OK){
        LOGD("Load RGB ISP Config From Device");
    }
    return TY_STATUS_OK;
}


static TY_STATUS ColorIspInitAutoExposure(TY_ISP_HANDLE isp_handle, TY_DEV_HANDLE dev_handle){
    bool is_v21_color_device;
    TY_STATUS res = __TYDetectOldVer21ColorCam(dev_handle, &is_v21_color_device);//old version device has different config
    if (res != TY_STATUS_OK){
        return res;
    }
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_ENABLE_AUTO_EXPOSURE_GAIN, 1));

    // do not enable gain auto control by default
# if 1
    int auto_gain_range[2] = { -1, -1 };
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_GAIN_RANGE, (uint8_t*)&auto_gain_range, sizeof(auto_gain_range)));
#else 
    if(is_v21_color_device){
        const int old_auto_gain_range[2] = { 33, 255 };
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_GAIN_RANGE, (uint8_t*)&old_auto_gain_range, sizeof(old_auto_gain_range)));
    }
    else{
#define CHECK_GO_FAILED(a)  {if((a)!=TY_STATUS_OK) break;} 
        do{
            TY_FEATURE_ID_LIST feature_id = TY_INT_GAIN;
            bool val;
            CHECK_GO_FAILED(TYHasFeature(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_GAIN, &val));
            if (val) {
                feature_id = TY_INT_GAIN;
            }
            CHECK_GO_FAILED(TYHasFeature(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_R_GAIN, &val));
            if (val) {
                feature_id = TY_INT_R_GAIN;
            }
            int auto_gain_range[2] = { 15, 255 };
            TY_INT_RANGE range;
            CHECK_GO_FAILED(TYGetIntRange(dev_handle, TY_COMPONENT_RGB_CAM, feature_id, &range));
            auto_gain_range[0] = std::min(range.min + 1, range.max);
            auto_gain_range[1] = std::max(range.max - 1, range.min);
            ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_GAIN_RANGE, (uint8_t*)&auto_gain_range, sizeof(auto_gain_range)));
        } while(0);
#undef CHECK_GO_FAILED
    }
#endif 

    //constraint exposure time 
    int auto_expo_range[2] = { 10, 100 };
    TY_INT_RANGE range;
    res = TYGetIntRange(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, &range);
    if (res == TY_STATUS_OK) {
        auto_expo_range[0] = std::min(range.min + 1, range.max);
        auto_expo_range[1] = std::max(range.max - 1, range.min);
    }
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_EXPOSURE_RANGE, (uint8_t*)&auto_expo_range, sizeof(auto_expo_range)));
    return TY_STATUS_OK;
}


static TY_STATUS ColorIspShowSupportedFeatures(TY_ISP_HANDLE handle){
    int sz;
    TY_STATUS res = TYISPGetFeatureInfoListSize(handle,&sz);
    if (res != TY_STATUS_OK){
        return res;
    }
    std::vector<TY_ISP_FEATURE_INFO> info;
    info.resize(sz);
    TYISPGetFeatureInfoList(handle, &info[0], info.size());
    for (int idx = 0; idx < sz; idx++){
        printf("feature name : %-50s  type : %s \n", info[idx].name, info[idx].value_type);
    }
    return TY_STATUS_OK;
}

#endif
