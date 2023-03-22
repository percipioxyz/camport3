#ifndef SAMPLE_COMMON_COMMON_HPP_
#define SAMPLE_COMMON_COMMON_HPP_

#include "Utils.hpp"

#include <fstream>
#include <iterator>
#include <opencv2/opencv.hpp>
#include "DepthRender.hpp"
#include "MatViewer.hpp"
#include "TYThread.hpp"
#include "DepthInpainter.hpp"
#include "TyIsp.h"
#include "BayerISP.hpp"
#include "CommandLineParser.hpp"
#include "CommandLineFeatureHelper.hpp"

static inline int decodeCsiRaw10(unsigned char* src, unsigned short* dst, int width, int height)
{
    if(width & 0x3) {
        return -1;
    }
    int raw10_line_size = 5 * width / 4;
    for(size_t i = 0, j = 0; i < raw10_line_size * height; i+=5, j+=4)
    {
        //[A2 - A9] | [B2 - B9] | [C2 - C9] | [D2 - D9] | [A0A1-B0B1-C0C1-D0D1]
        dst[j + 0] = ((uint16_t)src[i + 0] << 2) | ((src[i + 4] & 0x3)  >> 0);
        dst[j + 1] = ((uint16_t)src[i + 1] << 2) | ((src[i + 4] & 0xc)  >> 2);
        dst[j + 2] = ((uint16_t)src[i + 2] << 2) | ((src[i + 4] & 0x30) >> 4);
        dst[j + 3] = ((uint16_t)src[i + 3] << 2) | ((src[i + 4] & 0xc0) >> 6);
    }
    return 0;
}

static inline int decodeCsiRaw12(unsigned char* src, unsigned short* dst, int width, int height)
{
    if(width & 0x1) {
        return -1;
    }
    int raw12_line_size = 3 * width / 2;
    for(size_t i = 0, j = 0; i < raw12_line_size * height; i+=3, j+=2)
    {
        //[A4 - A11] | [B4 - B11] | [A0A1A2A3-B0B1B2B3]
        dst[j + 0] = ((uint16_t)src[i + 0] << 4) | ((src[i + 2] & 0x0f)  >> 0);
        dst[j + 1] = ((uint16_t)src[i + 1] << 4) | ((src[i + 2] & 0xf0)  >> 4);
    }
    return 0;
}

static inline int decodeCsiRaw14(unsigned char* src, unsigned short* dst, int width, int height)
{
    if(width & 0x3) {
        return -1;
    }
    int raw14_line_size = 7 * width / 4;
    for(size_t i = 0, j = 0; i < raw14_line_size * height; i+=7, j+=4)
    {
        //[A6 - A13] | [B6 - B13] | [C6 - C13] | [D6 - D13] | [A0A1A2A3A4A5-B0B1] | [B2B3B4B5-C0C1C2C3] | [C4C5-D0D1D2D3D4D5]
        dst[j + 0] = ((uint16_t)src[i + 0] << 6) | ((src[i + 4] & 0x3f) >> 0);
        dst[j + 1] = ((uint16_t)src[i + 1] << 6) | ((src[i + 4] & 0xc0) >> 6) | ((src[i + 5] & 0x0f) << 2);
        dst[j + 2] = ((uint16_t)src[i + 2] << 6) | ((src[i + 5] & 0xf0) >> 4) | ((src[i + 6] & 0x03) << 4);
        dst[j + 3] = ((uint16_t)src[i + 3] << 6) | ((src[i + 6] & 0xfc) >> 2);
    }
    return 0;
}

static inline int parseCsiRaw10(unsigned char* src, cv::Mat &dst, int width, int height)
{
    cv::Mat m(height, width, CV_16U);
    decodeCsiRaw10(src, (ushort*)m.data, width, height);
    //convert valid 10bit from lsb to msb, d = s * 64
    dst = m * 64;
    return 0;
}

static inline int parseCsiRaw12(unsigned char* src, cv::Mat &dst, int width, int height)
{
    cv::Mat m(height, width, CV_16U);
    decodeCsiRaw12(src, (ushort*)m.data, width, height);
    //convert valid 12bit from lsb to msb, d = s * 16
    dst = m * 16;
    return 0;
}

static inline int parseIrFrame(const TY_IMAGE_DATA* img, cv::Mat* pIR)
{
  if (img->pixelFormat == TY_PIXEL_FORMAT_MONO16 || img->pixelFormat==TY_PIXEL_FORMAT_TOF_IR_MONO16){
    *pIR = cv::Mat(img->height, img->width, CV_16U, img->buffer).clone();
  } else if(img->pixelFormat == TY_PIXEL_FORMAT_CSI_MONO10) {
    *pIR = cv::Mat(img->height, img->width, CV_16U);
    parseCsiRaw10((uchar*)img->buffer, (*pIR), img->width, img->height);
  } else if(img->pixelFormat == TY_PIXEL_FORMAT_MONO) {
    *pIR = cv::Mat(img->height, img->width, CV_8U, img->buffer).clone();
  } 
  else {
	  return -1;
  }

  return 0;
}

static inline int parseBayer8Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor, TY_ISP_HANDLE color_isp_handle = NULL)
{
  int code = cv::COLOR_BayerGB2BGR;
  switch (img->pixelFormat)
  {
  case TY_PIXEL_FORMAT_BAYER8GBRG:
    code = cv::COLOR_BayerGR2BGR;
    break;
  case TY_PIXEL_FORMAT_BAYER8BGGR:
    code = cv::COLOR_BayerRG2BGR;
    break;                
  case TY_PIXEL_FORMAT_BAYER8GRBG:
    code = cv::COLOR_BayerGB2BGR;
    break;                
  case TY_PIXEL_FORMAT_BAYER8RGGB:
    code = cv::COLOR_BayerBG2BGR;
    break;
  default:
    LOGE("Invalid bayer8 fmt!");
    return -1;
  }

  if (!color_isp_handle){
    cv::Mat raw(img->height, img->width, CV_8U, img->buffer);
    cv::cvtColor(raw, *pColor, code);
  }
  else{
    cv::Mat raw(img->height, img->width, CV_8U, img->buffer);
    pColor->create(img->height, img->width, CV_8UC3);
    int sz = img->height* img->width * 3;
    TY_IMAGE_DATA out_buff = TYInitImageData(sz, pColor->data, img->width, img->height);
    out_buff.pixelFormat = TY_PIXEL_FORMAT_BGR;
    int res = TYISPProcessImage(color_isp_handle, img, &out_buff);
    if (res != TY_STATUS_OK){
      //fall back to  using opencv api
      cv::Mat raw(img->height, img->width, CV_8U, img->buffer);
      cv::cvtColor(raw, *pColor, code);
    }
  }
  return 0;
}

static inline int parseBayer10Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
  int code = cv::COLOR_BayerGB2BGR;
  switch (img->pixelFormat)
  {
  case TY_PIXEL_FORMAT_CSI_BAYER10GBRG:
    code = cv::COLOR_BayerGR2BGR;
    break;
  case TY_PIXEL_FORMAT_CSI_BAYER10BGGR:
    code = cv::COLOR_BayerRG2BGR;
    break;                
  case TY_PIXEL_FORMAT_CSI_BAYER10GRBG:
    code = cv::COLOR_BayerGB2BGR;
    break;                
  case TY_PIXEL_FORMAT_CSI_BAYER10RGGB:
    code = cv::COLOR_BayerBG2BGR;
    break;
  default:
    LOGE("Invalid bayer10 fmt!");
    return -1;
  }
  cv::Mat raw16(img->height, img->width, CV_16U);
  parseCsiRaw10((uchar*)img->buffer, raw16, img->width, img->height);
  cv::cvtColor(raw16, *pColor, code);
  
  return 0;
}

static inline int parseBayer12Frame(const TY_IMAGE_DATA* img, cv::Mat* pColor)
{
  int code = cv::COLOR_BayerGB2BGR;
  switch (img->pixelFormat)
  {
  case TY_PIXEL_FORMAT_CSI_BAYER12GBRG:
    code = cv::COLOR_BayerGR2BGR;
    break;
  case TY_PIXEL_FORMAT_CSI_BAYER12BGGR:
    code = cv::COLOR_BayerRG2BGR;
    break;                
  case TY_PIXEL_FORMAT_CSI_BAYER12GRBG:
    code = cv::COLOR_BayerGB2BGR;
    break;                
  case TY_PIXEL_FORMAT_CSI_BAYER12RGGB:
    code = cv::COLOR_BayerBG2BGR;
    break;
  default:
    LOGE("Invalid bayer12 fmt!");
    return -1;
  }
  cv::Mat raw16(img->height, img->width, CV_16U);
  parseCsiRaw12((uchar*)img->buffer, raw16, img->width, img->height);
  cv::cvtColor(raw16, *pColor, code);

  return 0;
}

static inline int parseColorFrame(const TY_IMAGE_DATA* img, cv::Mat* pColor, TY_ISP_HANDLE color_isp_handle = NULL)
{
  int ret = 0;
  if (img->pixelFormat == TY_PIXEL_FORMAT_JPEG){
    std::vector<uchar> _v((uchar*)img->buffer, (uchar*)img->buffer + img->size);
    *pColor = cv::imdecode(_v, cv::IMREAD_COLOR);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_YVYU){
    cv::Mat yuv(img->height, img->width, CV_8UC2, img->buffer);
    cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YVYU);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_YUYV){
    cv::Mat yuv(img->height, img->width, CV_8UC2, img->buffer);
    cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YUYV);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_RGB){
    cv::Mat rgb(img->height, img->width, CV_8UC3, img->buffer);
    cv::cvtColor(rgb, *pColor, cv::COLOR_RGB2BGR);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_BGR){
    *pColor = cv::Mat(img->height, img->width, CV_8UC3, img->buffer);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_BAYER8GBRG || 
           img->pixelFormat == TY_PIXEL_FORMAT_BAYER8BGGR || 
           img->pixelFormat == TY_PIXEL_FORMAT_BAYER8GRBG || 
           img->pixelFormat == TY_PIXEL_FORMAT_BAYER8RGGB) 
  {
    ret = parseBayer8Frame(img, pColor, color_isp_handle);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER10GBRG || 
           img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER10BGGR || 
           img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER10GRBG || 
           img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER10RGGB) 
  {
    ret = parseBayer10Frame(img, pColor);
  }
  else if(img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER12GBRG || 
          img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER12BGGR || 
          img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER12GRBG || 
          img->pixelFormat == TY_PIXEL_FORMAT_CSI_BAYER12RGGB) 
  {
    ret = parseBayer12Frame(img, pColor);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_MONO){
    cv::Mat gray(img->height, img->width, CV_8U, img->buffer);
    cv::cvtColor(gray, *pColor, cv::COLOR_GRAY2BGR);
  }
  else if (img->pixelFormat == TY_PIXEL_FORMAT_CSI_MONO10){
    cv::Mat gray16(img->height, img->width, CV_16U);
    parseCsiRaw10((uchar*)img->buffer, gray16, img->width, img->height);
    *pColor = gray16.clone();
  }

  return ret;
}

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
            parseIrFrame(&frame.image[i], pLeftIR);
        }
        // get right ir image
        if (pRightIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT){
            parseIrFrame(&frame.image[i], pRightIR);
        }
        // get BGR
        if (pColor && frame.image[i].componentID == TY_COMPONENT_RGB_CAM){
            parseColorFrame(&frame.image[i], pColor, color_isp_handle);
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
        _exit = true;
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
        if (!_exit) {
            _exit = true;
            _cbThread.destroy();
        }
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
