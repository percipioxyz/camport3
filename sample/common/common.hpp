#ifndef SAMPLE_COMMON_COMMON_HPP_
#define SAMPLE_COMMON_COMMON_HPP_

#include "Utils.hpp"

#include <fstream>
#include <iterator>
#include <opencv2/opencv.hpp>
#include "DepthRender.hpp"
#include "MatViewer.hpp"
#include "TYThread.hpp"
#include "TyIsp.h"


static inline int parseFrame(const TY_FRAME_DATA& frame, cv::Mat* pDepth
                             , cv::Mat* pLeftIR, cv::Mat* pRightIR
                             , cv::Mat* pColor, TY_ISP_HANDLE color_isp_handle = NULL)
{
    for (int i = 0; i < frame.validCount; i++){
        if (frame.image[i].status != TY_STATUS_OK) continue;

        // get depth image
        if (pDepth && frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM){
            *pDepth = cv::Mat(frame.image[i].height, frame.image[i].width
                              , CV_16U, frame.image[i].buffer).clone();
        }
        // get left ir image
        if (pLeftIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT){
            *pLeftIR = cv::Mat(frame.image[i].height, frame.image[i].width
                               , CV_8U, frame.image[i].buffer).clone();
        }
        // get right ir image
        if (pRightIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT){
            *pRightIR = cv::Mat(frame.image[i].height, frame.image[i].width
                                , CV_8U, frame.image[i].buffer).clone();
        }
        // get BGR
        if (pColor && frame.image[i].componentID == TY_COMPONENT_RGB_CAM){
            if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_JPEG){
                cv::Mat jpeg(frame.image[i].height, frame.image[i].width
                             , CV_8UC1, frame.image[i].buffer);
                *pColor = cv::imdecode(jpeg, CV_LOAD_IMAGE_COLOR);
            }
            if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_YVYU){
                cv::Mat yuv(frame.image[i].height, frame.image[i].width
                            , CV_8UC2, frame.image[i].buffer);
                cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YVYU);
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_YUYV){
                cv::Mat yuv(frame.image[i].height, frame.image[i].width
                            , CV_8UC2, frame.image[i].buffer);
                cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YUYV);
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_RGB){
                cv::Mat rgb(frame.image[i].height, frame.image[i].width
                            , CV_8UC3, frame.image[i].buffer);
                cv::cvtColor(rgb, *pColor, cv::COLOR_RGB2BGR);
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_BGR){
                *pColor = cv::Mat(frame.image[i].height, frame.image[i].width
                                  , CV_8UC3, frame.image[i].buffer);
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_BAYER8GB){
                if (!color_isp_handle){
                    cv::Mat raw(frame.image[i].height, frame.image[i].width
                                , CV_8U, frame.image[i].buffer);
                    cv::cvtColor(raw, *pColor, cv::COLOR_BayerGB2BGR);
                }
                else{
                    cv::Mat raw(frame.image[i].height, frame.image[i].width
                                , CV_8U, frame.image[i].buffer);
                    TY_IMAGE_DATA _img = frame.image[i];
                    pColor->create(_img.height, _img.width, CV_8UC3);
                    int sz = _img.height* _img.width * 3;
                    TY_IMAGE_DATA out_buff = TYInitImageData(sz, pColor->data, _img.width, _img.height);
                    out_buff.pixelFormat = TY_PIXEL_FORMAT_BGR;
                    int res = TYISPProcessImage(color_isp_handle, &_img, &out_buff);
                    if (res != TY_STATUS_OK){
                        //fall back to  using opencv api
                        cv::Mat raw(frame.image[i].height, frame.image[i].width
                                    , CV_8U, frame.image[i].buffer);
                        cv::cvtColor(raw, *pColor, cv::COLOR_BayerGB2BGR);
                    }
                }
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_MONO){
                cv::Mat gray(frame.image[i].height, frame.image[i].width
                             , CV_8U, frame.image[i].buffer);
                cv::cvtColor(gray, *pColor, cv::COLOR_GRAY2BGR);
            }
        }
    }

    return 0;
}

enum{
    PC_FILE_FORMAT_XYZ = 0,
};

static void writePC_XYZ(const cv::Point3f* pnts, const cv::Vec3b *color, size_t n, FILE* fp)
{
    if (color){
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f %d %d %d\n", pnts[i].x, pnts[i].y, pnts[i].z, color[i][0], color[i][1], color[i][2]);
            }
        }
    }
    else{
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f 0 0 0\n", pnts[i].x, pnts[i].y, pnts[i].z);
            }
        }
    }
}

static void writePointCloud(const cv::Point3f* pnts, const cv::Vec3b *color, size_t n, const char* file, int format)
{
    FILE* fp = fopen(file, "w");
    if (!fp){
        return;
    }

    switch (format){
    case PC_FILE_FORMAT_XYZ:
        writePC_XYZ(pnts, color, n, fp);
        break;
    default:
        break;
    }

    fclose(fp);
}


class CallbackWrapper
{
public:
    typedef void(*TY_FRAME_CALLBACK) (TY_FRAME_DATA*, void* userdata);

    CallbackWrapper(){
        _hDevice = NULL;
        _cb = NULL;
        _userdata = NULL;
    }

    TY_STATUS TYRegisterCallback(TY_DEV_HANDLE hDevice, TY_FRAME_CALLBACK v, void* userdata)
    {
        _hDevice = hDevice;
        _cb = v;
        _userdata = userdata;
        _exit = false;
        _cbThread.create(&workerThread, this);
        return TY_STATUS_OK;
    }

    void TYUnregisterCallback()
    {
        _exit = true;
        _cbThread.destroy();
    }

private:
    static void* workerThread(void* userdata)
    {
        CallbackWrapper* pWrapper = (CallbackWrapper*)userdata;
        TY_FRAME_DATA frame;

        while (!pWrapper->_exit)
        {
            int err = TYFetchFrame(pWrapper->_hDevice, &frame, 100);
            if (!err) {
                pWrapper->_cb(&frame, pWrapper->_userdata);
            }
        }
        LOGI("frameCallback exit!");
        return NULL;
    }

    TY_DEV_HANDLE _hDevice;
    TY_FRAME_CALLBACK _cb;
    void* _userdata;

    bool _exit;
    TYThread _cbThread;
};



#ifdef _WIN32
static int get_fps() {
    static int fps_counter = 0;
    static clock_t fps_tm = 0;
   const int kMaxCounter = 250;
   fps_counter++;
   if (fps_counter < kMaxCounter) {
     return -1;
   }
   int elapse = (clock() - fps_tm);
   int v = (int)(((float)fps_counter) / elapse * CLOCKS_PER_SEC);
   fps_tm = clock();

   fps_counter = 0;
   return v;
 }
#else
static int get_fps() {
    static int fps_counter = 0;
    static clock_t fps_tm = 0;
    const int kMaxCounter = 200;
    struct timeval start;
    fps_counter++;
    if (fps_counter < kMaxCounter) {
        return -1;
    }

    gettimeofday(&start, NULL);
    int elapse = start.tv_sec * 1000 + start.tv_usec / 1000 - fps_tm;
    int v = (int)(((float)fps_counter) / elapse * 1000);
    gettimeofday(&start, NULL);
    fps_tm = start.tv_sec * 1000 + start.tv_usec / 1000;

    fps_counter = 0;
    return v;
}
#endif

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
    int image_size[2] = { 1280, 960 };// image size for current parameters
    int current_image_width = 1280;
    TYGetInt(dev_handle, TY_COMPONENT_RGB_CAM, TY_INT_WIDTH, &current_image_width);
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_IMAGE_SIZE, (uint8_t*)&image_size, sizeof(image_size)));//input raw image size
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_INPUT_RESAMPLE_SCALE, image_size[0] / current_image_width));
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
    if(is_v21_color_device){
        const int old_auto_gain_range[2] = { 33, 255 };
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_GAIN_RANGE, (uint8_t*)&old_auto_gain_range, sizeof(old_auto_gain_range)));
    }
    else{
        const int auto_gain_range[2] = { 15, 255 };
        ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_GAIN_RANGE, (uint8_t*)&auto_gain_range, sizeof(auto_gain_range)));
    }
#if 1
    //constraint exposure time 
    const int auto_expo_range[2] = { 10, 100 };
    ASSERT_OK(TYISPSetFeature(isp_handle, TY_ISP_FEATURE_AUTO_EXPOSURE_RANGE, (uint8_t*)&auto_expo_range, sizeof(auto_expo_range)));
#endif
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

static std::vector<uint8_t> TYReadBinaryFile(const char* filename)
{
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()){
        return std::vector<uint8_t>();
    }
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    std::vector<uint8_t> vec;
    vec.reserve(fileSize);

    // read the data:
    vec.insert(vec.begin(),
               std::istream_iterator<uint8_t>(file),
               std::istream_iterator<uint8_t>());

    return vec;
}

#endif
