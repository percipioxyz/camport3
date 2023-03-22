#ifndef SAMPLE_COMMON_UTILS_HPP_
#define SAMPLE_COMMON_UTILS_HPP_

/**
 * This file excludes opencv for sample_raw.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <inttypes.h>
#include "TYApi.h"
#include "TYThread.hpp"

#ifndef ASSERT
#define ASSERT(x)   do{ \
                if(!(x)) { \
                    LOGE("Assert failed at %s:%d", __FILE__, __LINE__); \
                    LOGE("    : " #x ); \
                    abort(); \
                } \
            }while(0)
#endif

#ifndef ASSERT_OK
#define ASSERT_OK(x)    do{ \
                int err = (x); \
                if(err != TY_STATUS_OK) { \
                    LOGE("Assert failed: error %d(%s) at %s:%d", err, TYErrorString(err), __FILE__, __LINE__); \
                    LOGE("    : " #x ); \
                    abort(); \
                } \
            }while(0)
#endif


#ifdef _WIN32
# include <windows.h>
# include <time.h>
  static inline char* getLocalTime()
  {
      static char local[26] = {0};
      SYSTEMTIME wtm;
      struct tm tm;
      GetLocalTime(&wtm);
      tm.tm_year     = wtm.wYear - 1900;
      tm.tm_mon     = wtm.wMonth - 1;
      tm.tm_mday     = wtm.wDay;
      tm.tm_hour     = wtm.wHour;
      tm.tm_min     = wtm.wMinute;
      tm.tm_sec     = wtm.wSecond;
      tm.tm_isdst    = -1;
  
      strftime(local, 26, "%Y-%m-%d %H:%M:%S", &tm);

      return local;
  }

  static inline uint64_t getSystemTime()
  {
      SYSTEMTIME wtm;
      struct tm tm;
      GetLocalTime(&wtm);
      tm.tm_year     = wtm.wYear - 1900;
      tm.tm_mon     = wtm.wMonth - 1;
      tm.tm_mday     = wtm.wDay;
      tm.tm_hour     = wtm.wHour;
      tm.tm_min     = wtm.wMinute;
      tm.tm_sec     = wtm.wSecond;
      tm. tm_isdst    = -1;
      return mktime(&tm) * 1000 + wtm.wMilliseconds;
  }
  static inline void MSleep(uint32_t ms)
  {
      Sleep(ms);
  }
#else
# include <sys/time.h>
# include <unistd.h>
  static inline char* getLocalTime()
  {
      static char local[26] = {0};
      time_t time;

      struct timeval tv;
      gettimeofday(&tv, NULL);

      time = tv.tv_sec;
      struct tm* p_time = localtime(&time); 
      strftime(local, 26, "%Y-%m-%d %H:%M:%S", p_time);  

      return local; 
  }

  static inline uint64_t getSystemTime()
  {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      return tv.tv_sec*1000 + tv.tv_usec/1000;
  }
  static inline void MSleep(uint32_t ms)
  {
      usleep(ms * 1000);
  }
#endif


#define LOGD(fmt,...)  printf("%" PRIu64 " (%s) " fmt "\n", getSystemTime(), getLocalTime(), ##__VA_ARGS__)
#define LOGI(fmt,...)  printf("%" PRIu64 " (%s) " fmt "\n", getSystemTime(), getLocalTime(), ##__VA_ARGS__)
#define LOGW(fmt,...)  printf("%" PRIu64 " (%s) " fmt "\n", getSystemTime(), getLocalTime(), ##__VA_ARGS__)
#define LOGE(fmt,...)  printf("%" PRIu64 " (%s) Error: " fmt "\n", getSystemTime(), getLocalTime(), ##__VA_ARGS__)
#define xLOGD(fmt,...)
#define xLOGI(fmt,...)
#define xLOGW(fmt,...)
#define xLOGE(fmt,...)


#ifdef _WIN32
#  include <windows.h>
#  define MSLEEP(x)     Sleep(x)
   // windows defined macro max/min
#  ifdef max
#    undef max
#  endif
#  ifdef min
#    undef min
#  endif
#else
#  include <unistd.h>
#  include <sys/time.h>
#  define MSLEEP(x)     usleep((x)*1000)
#endif

static inline const char* colorFormatName(TY_PIXEL_FORMAT fmt)
{
#define FORMAT_CASE(a) case (a): return #a
    switch(fmt){
        FORMAT_CASE(TY_PIXEL_FORMAT_UNDEFINED);
        FORMAT_CASE(TY_PIXEL_FORMAT_MONO);
        FORMAT_CASE(TY_PIXEL_FORMAT_RGB);
        FORMAT_CASE(TY_PIXEL_FORMAT_YVYU);
        FORMAT_CASE(TY_PIXEL_FORMAT_YUYV);
        FORMAT_CASE(TY_PIXEL_FORMAT_DEPTH16);
        FORMAT_CASE(TY_PIXEL_FORMAT_BAYER8GB);   
        FORMAT_CASE(TY_PIXEL_FORMAT_BAYER8BG);   
        FORMAT_CASE(TY_PIXEL_FORMAT_BAYER8GR);   
        FORMAT_CASE(TY_PIXEL_FORMAT_BAYER8RG);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_MONO10);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER10GBRG);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER10BGGR);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER10GRBG);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER10RGGB);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_MONO12);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER12GBRG);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER12BGGR);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER12GRBG);
        FORMAT_CASE(TY_PIXEL_FORMAT_CSI_BAYER12RGGB);
        FORMAT_CASE(TY_PIXEL_FORMAT_BGR);   
        FORMAT_CASE(TY_PIXEL_FORMAT_JPEG);   
        FORMAT_CASE(TY_PIXEL_FORMAT_MJPG);   
        default: return "UNKNOWN FORMAT";
    }
#undef FORMAT_CASE
}


static inline const TY_IMAGE_DATA* TYImageInFrame(const TY_FRAME_DATA& frame
        , const TY_COMPONENT_ID comp)
{
    for(int i = 0; i < frame.validCount; i++){
        if(frame.image[i].componentID == comp){
            return &frame.image[i];
        }
    }
    return NULL;
}
static void *updateThreadFunc(void *userdata)
{
    TY_INTERFACE_HANDLE iface = (TY_INTERFACE_HANDLE)userdata;
    TYUpdateDeviceList(iface);
    return NULL;
}

static TY_STATUS updateDevicesParallel(std::vector<TY_INTERFACE_HANDLE> &ifaces,
    uint64_t timeout=2000)
{
    if(ifaces.size() != 0) {
        TYThread *updateThreads = new TYThread[ifaces.size()];
        for(int i = 0; i < ifaces.size(); i++) {
            updateThreads[i].create(updateThreadFunc, ifaces[i]);
        }
        for(int i = 0; i < ifaces.size(); i++) {
           updateThreads[i].destroy();
        }
        delete [] updateThreads;
        updateThreads = NULL;
    }
    return TY_STATUS_OK;
}

static inline TY_STATUS selectDevice(TY_INTERFACE_TYPE iface
    , const std::string& ID, const std::string& IP
    , uint32_t deviceNum, std::vector<TY_DEVICE_BASE_INFO>& out)
{
    LOGD("Update interface list");
    ASSERT_OK( TYUpdateInterfaceList() );

    uint32_t n = 0;
    ASSERT_OK( TYGetInterfaceNumber(&n) );
    LOGD("Got %u interface list", n);
    if(n == 0){
      LOGE("interface number incorrect");
      return TY_STATUS_ERROR;
    }

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    ASSERT_OK( TYGetInterfaceList(&ifaces[0], n, &n) );
    ASSERT( n == ifaces.size() );
    for(uint32_t i = 0; i < n; i++){
      LOGI("Found interface %u:", i);
      LOGI("  name: %s", ifaces[i].name);
      LOGI("  id:   %s", ifaces[i].id);
      LOGI("  type: 0x%x", ifaces[i].type);
      if(TYIsNetworkInterface(ifaces[i].type)){
        LOGI("    MAC: %s", ifaces[i].netInfo.mac);
        LOGI("    ip: %s", ifaces[i].netInfo.ip);
        LOGI("    netmask: %s", ifaces[i].netInfo.netmask);
        LOGI("    gateway: %s", ifaces[i].netInfo.gateway);
        LOGI("    broadcast: %s", ifaces[i].netInfo.broadcast);
      }
    }

    out.clear();
    std::vector<TY_INTERFACE_TYPE> ifaceTypeList;
    std::vector<TY_INTERFACE_HANDLE> hIfaces;
    ifaceTypeList.push_back(TY_INTERFACE_USB);
    ifaceTypeList.push_back(TY_INTERFACE_ETHERNET);
    ifaceTypeList.push_back(TY_INTERFACE_IEEE80211);
    for(size_t t = 0; t < ifaceTypeList.size(); t++){
      for(uint32_t i = 0; i < ifaces.size(); i++){
        if(ifaces[i].type == ifaceTypeList[t] && (ifaces[i].type & iface) && deviceNum > out.size()){
          TY_INTERFACE_HANDLE hIface;
          ASSERT_OK( TYOpenInterface(ifaces[i].id, &hIface) );
          hIfaces.push_back(hIface);
        }
      }
    }
    updateDevicesParallel(hIfaces);
    for (uint32_t i = 0; i < hIfaces.size(); i++) {
        TY_INTERFACE_HANDLE hIface = hIfaces[i];
        uint32_t n = 0;
        TYGetDeviceNumber(hIface, &n);
        if(n > 0){
          std::vector<TY_DEVICE_BASE_INFO> devs(n);
          TYGetDeviceList(hIface, &devs[0], n, &n);
          for(uint32_t j = 0; j < n; j++){
            if(deviceNum > out.size() && ((ID.empty() && IP.empty())
                || (!ID.empty() && devs[j].id == ID)
                || (!IP.empty() && IP == devs[j].netInfo.ip)))
            {
              if (devs[j].iface.type == TY_INTERFACE_ETHERNET || devs[j].iface.type == TY_INTERFACE_IEEE80211) {
                LOGI("*** Select %s on %s, ip %s", devs[j].id, devs[j].iface.id, devs[j].netInfo.ip);
              } else {
                LOGI("*** Select %s on %s", devs[j].id, devs[j].iface.id);
              }
              out.push_back(devs[j]);
            }
          }
        }
        TYCloseInterface(hIface);
    }

    if(out.size() == 0){
      LOGE("not found any device");
      return TY_STATUS_ERROR;
    }

    return TY_STATUS_OK;
}

static inline TY_STATUS get_feature_enum_list(TY_DEV_HANDLE handle,
                                       TY_COMPONENT_ID compID,
                                       TY_FEATURE_ID featID,
                                       std::vector<TY_ENUM_ENTRY> &feature_info){
    uint32_t n = 0;
    ASSERT_OK(TYGetEnumEntryCount(handle, compID, featID, &n));
    LOGD("===         %14s: entry count %d", "", n);
    feature_info.clear();
    if (n == 0){
        return TY_STATUS_ERROR;
    }
    feature_info.resize(n);
    ASSERT_OK(TYGetEnumEntryInfo(handle, compID, featID, &feature_info[0], n, &n));
    return TY_STATUS_OK;
}

static inline TY_STATUS get_image_mode(TY_DEV_HANDLE handle
    , TY_COMPONENT_ID compID
    , int32_t &image_mode, int idx)
{
    std::vector<TY_ENUM_ENTRY> image_mode_list;
    ASSERT_OK(get_feature_enum_list(handle, compID, TY_ENUM_IMAGE_MODE, image_mode_list));
    if (image_mode_list.size() == 0 || idx < 0
        || idx > image_mode_list.size() -1){
        return TY_STATUS_ERROR;
    }
    image_mode = image_mode_list[idx].value;
    return TY_STATUS_OK;
}

static inline TY_STATUS get_default_image_mode(TY_DEV_HANDLE handle
    , TY_COMPONENT_ID compID
    , int32_t &image_mode)
{
    return get_image_mode(handle, compID, image_mode, 0);
}
#endif
