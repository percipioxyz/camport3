/**@file      TYApi.h
 * @brief     TYApi.h includes camera control and data receiving interface, 
 *            which supports configuration for image resolution,  frame rate, exposure  
 *            time, gain, working mode,etc.
 *
 */

/**@mainpage 
* 
* Copyright(C)2016-2023 Percipio All Rights Reserved
*
*
*
* @section Note
*   Depth camera, called "device", consists of several components. Each component
*   is a hardware module or virtual module, such as RGB sensor, depth sensor.
*   Each component has its own features, such as image width, exposure time, etc..
* 
*   NOTE: The component TY_COMPONENT_DEVICE is a virtual component that contains
*         all features related to the whole device, such as trigger mode, device IP.
* 
*   Each frame consists of several images. Normally, all the images have identical
*   timestamp, means they are captured at the same time.
* 
* */

#ifndef TY_API_H_
#define TY_API_H_

#include "TYDefs.h"
typedef void (*TY_EVENT_CALLBACK) (TY_EVENT_INFO*, void* userdata);
typedef void (*TY_IMU_CALLBACK) (TY_IMU_DATA*, void* userdata);


//------------------------------------------------------------------------------
// inlines
//------------------------------------------------------------------------------
static inline bool TYIsNetworkInterface(int32_t interfaceType)
{
  return (interfaceType == TY_INTERFACE_ETHERNET) || 
         (interfaceType == TY_INTERFACE_IEEE80211);
}

static inline void TYIntToIPv4(uint32_t addr, uint8_t out[4])
{
  out[0] = (addr>>24) & 0xff;
  out[1] = (addr>>16) & 0xff;
  out[2] = (addr>>8) & 0xff;
  out[3] = (addr>>0) & 0xff;
}

static inline uint32_t TYIPv4ToInt(uint8_t ip[4])
{
  return (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
}

///init a TY_IMAGE_DATA struct
static inline TY_IMAGE_DATA TYInitImageData(size_t size, void* buffer
    , size_t width, size_t height)
{
  TY_IMAGE_DATA out;
  out.timestamp = 0;
  out.imageIndex = 0;
  out.status = 0;
  out.componentID = 0;
  out.size = size;
  out.buffer = buffer;
  out.width = width;
  out.height = height;
  out.pixelFormat = 0;
  return out;
}

///get feature format type from feature id
static inline TY_FEATURE_TYPE TYFeatureType(TY_FEATURE_ID id)
{
    return id & 0xf000;
}

///deprecated: get pixel size in byte, Invalid for 10/12/14bit mode
static inline int32_t TYPixelSize(TY_IMAGE_MODE imageMode)
{
    return ((imageMode >> 28) & 0xf);
}

///get pixel size in bits
static inline int32_t TYBitsPerPixel(TY_IMAGE_MODE imageMode)
{
    TY_PIXEL_BITS bits = imageMode & (0xf  << 28);
    switch(bits){
      case TY_PIXEL_16BIT:
        return 16;
      case TY_PIXEL_24BIT:
        return 24;
      case TY_PIXEL_32BIT:
        return 32;
      case TY_PIXEL_48BIT:
        return 48;
      case TY_PIXEL_64BIT:
        return 64;
      case TY_PIXEL_10BIT:
        return 10;
      case TY_PIXEL_12BIT:
        return 12;
      case TY_PIXEL_8BIT:
      default:
        return 8;
    }
}

///get line size in bytes
static inline int32_t TYPixelLineSize(int width, TY_IMAGE_MODE imageMode)
{
    return (width * TYBitsPerPixel(imageMode)) >> 3;
}

///make a image mode from pixel format & resolution mode
static inline TY_IMAGE_MODE TYImageMode(TY_PIXEL_FORMAT pix, TY_RESOLUTION_MODE res)
{
    return pix | res;
}

///get a resoltuion mode from width & height
static inline TY_RESOLUTION_MODE TYResolutionMode2(int width, int height){
    return (TY_RESOLUTION_MODE)((width << 12) + height);
}

///create a image mode from pixel format , width & height  
static inline TY_IMAGE_MODE TYImageMode2(TY_PIXEL_FORMAT pix, int width,int height)
{
    return pix | TYResolutionMode2(width, height);
}

///get pixel format from image mode
static inline TY_PIXEL_FORMAT TYPixelFormat(TY_IMAGE_MODE imageMode)
{
    return imageMode & 0xff000000;
}

///get a resoltuion mode from image mode 
static inline TY_RESOLUTION_MODE TYResolutionMode(TY_IMAGE_MODE imageMode)
{
    return imageMode & 0x00ffffff;
}

///get image width from image mode
static inline int32_t TYImageWidth(TY_IMAGE_MODE imageMode)
{
    return TYResolutionMode(imageMode) >> 12;
}

///get image height from image mode
static inline int32_t TYImageHeight(TY_IMAGE_MODE imageMode)
{
    return TYResolutionMode(imageMode) & 0x0fff;
}

//------------------------------------------------------------------------------
//  C API
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  Version check
//------------------------------------------------------------------------------
TY_CAPI _TYInitLib(void);
TY_CAPI TYLibVersion (TY_VERSION_INFO* version);
static inline TY_STATUS TYInitLib(void)
{
    TY_VERSION_INFO soVersion;
    TYLibVersion(&soVersion);
    if(!(soVersion.major == TY_LIB_VERSION_MAJOR && soVersion.minor >= TY_LIB_VERSION_MINOR)){
        abort();   // generate fault directly
    }
    return _TYInitLib();
}
///@brief  Get error information.
///@param  [in]  errorID       Error id.
///@retval Error string.                    
TY_EXTC TY_EXPORT const char* TY_STDC TYErrorString (TY_STATUS errorID);

///@brief  Init this library.
///        
///        We make this function to be static inline, because we do a version check here.
///        Some user may use the mismatched header file and dynamic library, and
///        that's quite difficult to locate the error.
///        
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_ERROR                  Has been inited.
inline TY_STATUS TYInitLib        (void);

///@brief  Deinit this library.
///@retval TY_STATUS_OK                     Succeed.
TY_CAPI TYDeinitLib               (void);

///@brief  Get current library version.
///@param  [out] version       Version infomation to be filled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NULL_POINTER           TYLibVersion called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYLibVersion(ver);
///                           ^ is NULL 
///          
TY_CAPI TYLibVersion              (TY_VERSION_INFO* version);

///@brief  Set log level.
///@param  [in]  lvl           Log level.
///@retval TY_STATUS_OK                     Succeed.
TY_CAPI TYSetLogLevel             (TY_LOG_LEVEL lvl);

///@brief  set log prefix
///@param  [in]  prefix        Prefix string.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_PARAMETER      Prefix is empty or prefix is too long
///          
///          Suggestions:
///            Prefix is empty or prefix is too long, cannot be set
///            Like this:
///              TYSetLogPrefix(prefix);
///                             ^ prefix is empty or prefix is too long
///          
TY_CAPI TYSetLogPrefix            (const char* prefix);

///@brief  Append log to specified file.
///@param  [in]  filePath      Path to the log file.
///@param  [in]  lvl           Log level.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_ERROR                  Failed to add file
///          
///          Suggestions:
///            Please check if the file path is correct and if you have permission to write to the file
///          
TY_CAPI TYAppendLogToFile         (const char* filePath, TY_LOG_LEVEL lvl);

///@brief  Remove log file.
///@param  [in]  filePath      Path to the log file.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_ERROR                  Failed to remove file
///          
///          Suggestions:
///            Please check if the file path is correct
///          
TY_CAPI TYRemoveLogFile           (const char* filePath);

///@brief  Append log to Tcp/Udp server.
///@param  [in]  protocol      Protocol of the server, "tcp" or "udp".
///@param  [in]  ip            IP address of the server.
///@param  [in]  port          Port of the server.
///@param  [in]  lvl           Log level.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_ERROR                  Failed to add server
///          
///          Suggestions:
///            Please check if the ip and port are correct
///          
///@retval TY_STATUS_INVALID_PARAMETER      Unsupported protocol
///          
///          Suggestions:
///            Unsupported protocol, please use tcp or udp
///          
TY_CAPI TYAppendLogToServer       (const char* protocol, const char* ip, uint16_t port, TY_LOG_LEVEL lvl);

///@brief  Remove log server.
///@param  [in]  protocol      Protocol of the server, "tcp" or "udp".
///@param  [in]  ip            IP address of the server.
///@param  [in]  port          Port of the server.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_ERROR                  Failed to remove server
///          
///          Suggestions:
///            Please check if the ip and port are correct
///          
///@retval TY_STATUS_INVALID_PARAMETER      Unsupported protocol
///          
///          Suggestions:
///            Unsupported protocol, please use tcp or udp
///          
TY_CAPI TYRemoveLogServer         (const char* protocol, const char* ip, uint16_t port);

///@brief  Update current interfaces.
///        call before TYGetInterfaceList
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
TY_CAPI TYUpdateInterfaceList     (void);

///@brief  Get number of current interfaces.
///@param  [out] pNumIfaces    Number of interfaces.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_NULL_POINTER           TYGetInterfaceNumber called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetInterfaceNumber(pNumIfaces);
///                                   ^ is NULL
///          
TY_CAPI TYGetInterfaceNumber      (uint32_t* pNumIfaces);

///@brief  Get interface info list.
///@param  [out] pIfaceInfos   Array of interface infos to be filled.
///@param  [in]  bufferCount   Array size of interface infos.
///@param  [out] filledCount   Number of filled TY_INTERFACE_INFO.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_NULL_POINTER           TYGetInterfaceList called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetInterfaceList(pIfaceInfos, bufferCount, filledCount);
///                                 ^     or     ^  is NULL
///          
TY_CAPI TYGetInterfaceList        (TY_INTERFACE_INFO* pIfaceInfos, uint32_t bufferCount, uint32_t* filledCount);

///@brief  Check if has interface.
///@param  [in]  ifaceID       Interface ID string, can be get from TY_INTERFACE_INFO.
///@param  [out] value         True if the interface exists.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_NULL_POINTER           TYHasInterface called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYHasInterface(ifaceID, value);
///                                      ^ is NULL
///          
///@see TYGetInterfaceList
TY_CAPI TYHasInterface            (const char* ifaceID, bool* value);

///@brief  Open specified interface.
///@param  [in]  ifaceID       Interface ID string, can be get from TY_INTERFACE_INFO.
///@param  [out] outHandle     Handle of opened interface.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_NULL_POINTER           TYOpenInterface called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYOpenInterface(ifaceID, outHandle);
///                              ^   or   ^ is NULL
///          
///@retval TY_STATUS_INVALID_INTERFACE      TYOpenInterface called with invalid interface ID
///          
///          Suggestions:
///            Please check ifaceID parameter
///            Like this:
///              TYOpenInterface(ifaceID, outHandle);
///                              ^ is invalid
///            Usually you get interface information by calling TYUpdateInterfaceList, TYGetInterfaceList
///            and then open interface by calling TYOpenInterface
///            When your host interface (network or USB) changes);
///            you may need to update interface list again
///          
///@see TYGetInterfaceList
TY_CAPI TYOpenInterface           (const char* ifaceID, TY_INTERFACE_HANDLE* outHandle);

///@brief  Close interface.
///@param  [in]  ifaceHandle   Interface to be closed.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYCloseInterface called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYCloseInterface(ifaceHandle);
///                               ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
TY_CAPI TYCloseInterface          (TY_INTERFACE_HANDLE ifaceHandle);

///@brief  Update current connected devices.
///@param  [in]  ifaceHandle   Interface handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYUpdateDeviceList called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYUpdateDeviceList(ifaceHandle);
///                                 ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
TY_CAPI TYUpdateDeviceList        (TY_INTERFACE_HANDLE ifaceHandle);

///@brief  Update current connected devices.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
TY_CAPI TYUpdateAllDeviceList     (void);

///@brief  Get number of current connected devices.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [out] deviceNumber  Number of connected devices.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYGetDeviceNumber called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYGetDeviceNumber(ifaceHandle, pDeviceNumber);
///                                ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceNumber called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceNumber(ifaceHandle, deviceNumber);
///                                             ^ is NULL
///          
TY_CAPI TYGetDeviceNumber         (TY_INTERFACE_HANDLE ifaceHandle, uint32_t* deviceNumber);

///@brief  Get device info list.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [out] deviceInfos   Device info array to be filled.
///@param  [in]  bufferCount   Array size of deviceInfos.
///@param  [out] filledDeviceCount     Number of filled TY_DEVICE_BASE_INFO.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYGetDeviceList called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYGetDeviceList(ifaceHandle, pDeviceInfos, bufferCount, pFilledDeviceCount);
///                              ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceList called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceList(ifaceHandle, pDeviceInfos, bufferCount, pFilledCount);
///                                           ^ is NULL or  ^ is 0    or ^ is NULL
///          
TY_CAPI TYGetDeviceList           (TY_INTERFACE_HANDLE ifaceHandle, TY_DEVICE_BASE_INFO* deviceInfos, uint32_t bufferCount, uint32_t* filledDeviceCount);

///@brief  Check whether the interface has the specified device.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [in]  deviceID      Device ID string, can be get from TY_DEVICE_BASE_INFO.
///@param  [out] value         True if the device exists.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYHasDevice called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYHasDevice(ifaceHandle, deviceID, value);
///                          ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_NULL_POINTER           TYHasDevice called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYHasDevice(ifaceHandle, deviceID, value);
///                                       ^    or   ^ is NULL
///          
TY_CAPI TYHasDevice               (TY_INTERFACE_HANDLE ifaceHandle, const char* deviceID, bool* value);

///@brief  Open device by device ID.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [in]  deviceID      Device ID string, can be get from TY_DEVICE_BASE_INFO.
///@param  [out] outDeviceHandle Handle of opened device. Valid only if TY_STATUS_OK or TY_FW_ERRORCODE returned.
///@param  [out] outFwErrorcode  Firmware errorcode. Valid only if TY_FW_ERRORCODE returned.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYOpenDevice called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYOpenDevice(ifaceHandle, deviceID, outDeviceHandle, outFwErrorcode);
///                           ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_NULL_POINTER           TYOpenDevice called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYOpenDevice(ifaceHandle, deviceID, outDeviceHandle, outFwErrorcode);
///                                        ^    or   ^ is NULL
///          
///@retval TY_STATUS_INVALID_PARAMETER      TYOpenDevice called with invalid device ID: %s
///          
///          Suggestions:
///            Please check deviceID parameter
///            Like this:
///              TYOpenDevice(ifaceHandle, deviceID, outDeviceHandle, outFwErrorcode);
///                                        ^ is invalid
///            Usually you get device information by calling TYUpdateDeviceList, TYGetDeviceList
///            and then open device by calling TYOpenDevice
///            When your device online status changes);
///            you may need to update device list again
///          
///@retval TY_STATUS_BUSY                   Failed to open device
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera is occupied, please check if other processes on this machine (such as Percipio Viewer tool)
///                or other host machines are occupying the camera. If the camera is occupied, release the occupation.
///              2.A third-party program is written into the camera, please contact Percipio after-sales support.
///          
///@retval TY_STATUS_FIRMWARE_ERROR         Device opened successfully, but firmware error code is not 0
///          
///          Suggestions:
///            Some functions of the device may have exceptions, please check the firmware error code for details
///            TY_FW_ERRORCODE outFwErrorcode;
///            TYOpenDevice(ifaceHandle, deviceID, outDeviceHandle, &outFwErrorcode);
///            if(outFwErrorcode != 0) {
///              parse_firmware_errcode(outFwErrorcode);
///            }
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to open device
///          
///          Suggestions:
///            Possible reasons:
///              1.A third-party program is written into the camera, please contact Percipio after-sales support.
///              2.The camera IP address is not in the same network segment as the host IP address.
///                If the camera IP address is not in the same network segment as the host IP address, the host can discover the camera across network segments, but may not be able to open it.
///                If there is a routing connection between your host and the camera, you can try to open the camera with TYOpenDeviceWithIP.
///                Otherwise, you can modify the camera IP address or the host IP address.
///                If you need to modify the camera IP address, please refer to Setting the IP address of the network depth camera.
///              3.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
TY_CAPI TYOpenDevice              (TY_INTERFACE_HANDLE ifaceHandle, const char* deviceID, TY_DEV_HANDLE* outDeviceHandle, TY_FW_ERRORCODE* outFwErrorcode=NULL);

///@brief  Open device by device IP, useful when a device is not listed.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [in]  IP            Device IP.
///@param  [out] deviceHandle  Handle of opened device.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYOpenDeviceWithIP called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYOpenDeviceWithIP(ifaceHandle, IP, outDeviceHandle);
///                                 ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_NULL_POINTER           TYOpenDeviceWithIP called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYOpenDeviceWithIP(ifaceHandle, IP, outDeviceHandle);
///                                              ^ or^ is NULL
///          
///@retval TY_STATUS_INVALID_PARAMETER      TYOpenDeviceWithIP called with invalid IP address
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYOpenDeviceWithIP(ifaceHandle, IP, outDeviceHandle);
///                                              ^ is invalid
///            A valid IP address should be like:
///              192.168.31.1
///            Usually you get device information by calling TYUpdateDeviceList, TYGetDeviceList
///            and then open device by calling TYOpenDevice
///            When your device online status changes,
///            you may need to update device list again
///          
///@retval TY_STATUS_BUSY                   Failed to open device
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera is occupied, please check if other processes on this machine (such as Percipio Viewer tool)
///                or other host machines are occupying the camera. If the camera is occupied, release the occupation.
///              2.A third-party program is written into the camera, please contact Percipio after-sales support.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to open device
///          
///          Suggestions:
///            Possible reasons:
///              1.A third-party program is written into the camera, please contact Percipio after-sales support.
///              2.The camera IP address cannot communicate with the host IP address through routing.
///              3.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
TY_CAPI TYOpenDeviceWithIP        (TY_INTERFACE_HANDLE ifaceHandle, const char* IP, TY_DEV_HANDLE* deviceHandle);

///@brief  Get interface handle by device handle.
///@param  [in]  hDevice       Device handle.
///@param  [out] pIface        Interface handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceInterface called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceInterface(hDevice, pIface);
///                                   ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceInterface called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceInterface(hDevice, pIface);
///                                            ^ is NULL
///          
TY_CAPI TYGetDeviceInterface      (TY_DEV_HANDLE hDevice, TY_INTERFACE_HANDLE* pIface);

///@brief  Force a ethernet device to use new IP address, useful when device use persistent IP and cannot be found.
///@param  [in]  ifaceHandle   Interface handle.
///@param  [in]  MAC           Device MAC, should be "xx:xx:xx:xx:xx:xx".
///@param  [in]  newIP         New IP.
///@param  [in]  newNetMask    New subnet mask.
///@param  [in]  newGateway    New gateway.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             TYInitLib not called.
///@retval TY_STATUS_INVALID_INTERFACE      TYForceDeviceIP called with invalid interface handle
///          
///          Suggestions:
///            Please check interface handle
///            Like this:
///              TYForceDeviceIP(ifaceHandle, MAC, newIP, newNetMask, newGateway);
///                              ^ is invalid
///            The ifaceHandle parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenInterface failed to open interface and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated interface list by calling TYUpdateInterfaceList
///          
///@retval TY_STATUS_WRONG_TYPE             TYForceDeviceIP called with invalid interface type
///          
///          Suggestions:
///            Please check interface type
///            Usually you can get interface information by calling TYGetInterfaceList
///            You can use TYIsNetworkInterface to check the interface type
///            Only network interfaces can call TYForceDeviceIP
///            Like this:
///              TY_INTERFACE_INFO info; uint32_t num;
///              TYGetInterfaceList(&info, 1, &num);
///              if(TYIsNetworkInterface(info[0].type)) {
///                TY_INTERFACE_HANDLE hIface;
///                TYOpenInterface(info[0].id, &hIface);
///                TYForceDeviceIP(hIface, MAC, newIP, newNetMask, newGateway);
///              }
///          
///@retval TY_STATUS_NULL_POINTER           TYForceDeviceIP called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYForceDeviceIP(ifaceHandle, MAC, newIP, newNetMask, newGateway);
///                                           ^ or ^  or  ^     or    ^ is NULL
///          
///@retval TY_STATUS_INVALID_PARAMETER      Invalid MAC address:
///          
///          Suggestions:
///            Please check MAC parameter
///            Like this:
///              TYForceDeviceIP(ifaceHandle, MAC, newIP, newNetMask, newGateway);
///                                           ^ is invalid
///            MAC address should be six bytes of hexadecimal separated by colons
///            For example: 00:11:22:aa:bb:cc
///          
///@retval TY_STATUS_TIMEOUT                Failed to force set IP
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///              2.There is no camera with a matching target MAC address in the network.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to force set IP
///          
///          Suggestions:
///            Possible reasons:
///              1.New IP, NetMask, Gateway are incorrect, camera device refuses to set, or camera device is abnormal.
///          
TY_CAPI TYForceDeviceIP           (TY_INTERFACE_HANDLE ifaceHandle, const char* MAC, const char* newIP, const char* newNetMask, const char* newGateway);

///@brief  Close device by device handle.
///@param  [in]  hDevice       Device handle.
///@param  [in]  reboot        Reboot device after close.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYCloseDevice called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYCloseDevice(hDevice, reboot);
///                            ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_TIMEOUT                Failed to close device
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to close device
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera device is abnormal and cannot be closed normally.
///          
///@retval TY_STATUS_IDLE                   Device has been closed.
TY_CAPI TYCloseDevice             (TY_DEV_HANDLE hDevice, bool reboot=false);

///@brief  Get base info of the open device.
///@param  [in]  hDevice       Device handle.
///@param  [out] info          Base info out.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceInfo called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceInfo(hDevice, info);
///                              ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceInfo called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceInfo(hDevice, info);
///                                       ^ is NULL
///          
TY_CAPI TYGetDeviceInfo           (TY_DEV_HANDLE hDevice, TY_DEVICE_BASE_INFO* info);

///@brief  Get all components IDs.
///@param  [in]  hDevice       Device handle.
///@param  [out] componentIDs  All component IDs this device has. (bit flag).
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetComponentIDs called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetComponentIDs(hDevice, outComponentIDs);
///                                ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetComponentIDs called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetComponentIDs(hDevice, outComponentIDs);
///                                         ^ is NULL
///          
///@see TY_DEVICE_COMPONENT_LIST
TY_CAPI TYGetComponentIDs         (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID* componentIDs);

///@brief  Get all enabled components IDs.
///@param  [in]  hDevice       Device handle.
///@param  [out] componentIDs  Enabled component IDs.(bit flag)
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetEnabledComponents called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetEnabledComponents(hDevice, componentIDs);
///                                     ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           componentIDs is NULL.
///@see TY_DEVICE_COMPONENT_LIST
TY_CAPI TYGetEnabledComponents    (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID* componentIDs);

///@brief  Enable components.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentIDs  Components to be enabled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYEnableComponents called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYEnableComponents(hDevice, componentIDs);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_PARAMETER      Invalid component IDs
///          
///          Suggestions:
///            Please check componentIDs parameter
///            Like this:
///              TYEnableComponents(hDevice, componentIDs);
///                                          ^ is invalid
///            componentIDs should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_COMPONENT      Some components specified by componentIDs are invalid.
///@retval TY_STATUS_BUSY                   Camera device is capturing
///          
///          Suggestions:
///            Please call TYEnableComponents when the camera device is stopped
///            Like this:
///              TYStopCapture(hDevice);
///              TYEnableComponents(hDevice, componentIDs);
///          
///@see TY_DEVICE_COMPONENT_LIST
TY_CAPI TYEnableComponents        (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentIDs);

///@brief  Disable components.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentIDs  Components to be disabled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYDisableComponents called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYDisableComponents(hDevice, componentIDs);
///                                  ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_PARAMETER      Invalid component IDs
///          
///          Suggestions:
///            Please check componentIDs parameter
///            Like this:
///              TYDisableComponents(hDevice, componentIDs);
///                                            ^ is invalid
///            componentIDs should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_COMPONENT      Some components specified by componentIDs are invalid.
///@retval TY_STATUS_BUSY                   Camera device is capturing
///          
///          Suggestions:
///            Please call TYEnableComponents when the camera device is stopped
///            Like this:
///              TYStopCapture(hDevice);
///              TYDisableComponents(hDevice, componentIDs);
///          
///@see TY_DEVICE_COMPONENT_LIST
TY_CAPI TYDisableComponents       (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentIDs);

///@brief  Get total buffer size of one frame in current configuration.
///@param  [in]  hDevice       Device handle.
///@param  [out] bufferSize    Buffer size per frame.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetFrameBufferSize called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetFrameBufferSize(hDevice, outSize);
///                                   ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetFrameBufferSize called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetFrameBufferSize(hDevice, outSize);
///                                            ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get frame buffer size
///          
///          Suggestions:
///            Possible reasons:
///             1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get frame buffer size
///          
///          Suggestions:
///            Possible reasons:
///             1.Camera device is abnormal and cannot get the frame buffer size.
///          
TY_CAPI TYGetFrameBufferSize      (TY_DEV_HANDLE hDevice, uint32_t* bufferSize);

///@brief  Enqueue a user allocated buffer.
///@param  [in]  hDevice       Device handle.
///@param  [in]  buffer        Buffer to be enqueued.
///@param  [in]  bufferSize    Size of the input buffer.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYEnqueueBuffer called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYEnqueueBuffer(hDevice, buffer, bufferSize);
///                              ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYEnqueueBuffer called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYEnqueueBuffer(hDevice, buffer, bufferSize);
///                                       ^ is NULL
///          
///@retval TY_STATUS_WRONG_SIZE             TYEnqueueBuffer called with wrong size
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYEnqueueBuffer(hDevice, buffer, bufferSize);
///                                               ^ is 0 or negative value
///          
///@retval TY_STATUS_TIMEOUT                Failed to enqueue frame buffer
///          
///          Suggestions:
///            Possible reasons:
///             1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to enqueue frame buffer
///          
///          Suggestions:
///            Possible reasons:
///             1.Camera device is abnormal and cannot get the frame buffer size.
///          
TY_CAPI TYEnqueueBuffer           (TY_DEV_HANDLE hDevice, void* buffer, uint32_t bufferSize);

///@brief  Clear the internal buffer queue, so that user can release all the buffer.
///@param  [in]  hDevice       Device handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYClearBufferQueue called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYClearBufferQueue(hDevice);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_BUSY                   Device is capturing.
TY_CAPI TYClearBufferQueue        (TY_DEV_HANDLE hDevice);

///@brief  Start capture.
///@param  [in]  hDevice       Device handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYStartCapture called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYStartCapture(hDevice);
///                             ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      No components are enabled
///          
///          Suggestions:
///            Please enable the components of the camera device first
///            Like this:
///              TYEnableComponents(hDevice, componentIDs);
///              TYStartCapture(hDevice);
///          
///@retval TY_STATUS_BUSY                   Camera device has been started.
///          
///          Suggestions:
///            Please stop the camera device first
///            Like this:
///              TYStopCapture(hDevice);
///              TYStartCapture(hDevice);
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to start camera
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera device is abnormal and cannot start the camera.
///              2.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///              3.Camera is busy, please try again
///          
TY_CAPI TYStartCapture            (TY_DEV_HANDLE hDevice);

///@brief  Stop capture.
///@param  [in]  hDevice       Device handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYStopCapture called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYStopCapture(hDevice);
///                            ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_IDLE                   Camera device has been stopped
///          
///          Suggestions:
///            The camera device has stopped, usually after starting
///            Like this:
///              TYStartCapture(hDevice);
///              TYStopCapture(hDevice);
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to stop camera
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera device is abnormal and cannot stop the camera.
///          
TY_CAPI TYStopCapture             (TY_DEV_HANDLE hDevice);

///@brief  Send a software trigger to capture a frame when device works in trigger mode.
///@param  [in]  hDevice       Device handle.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSendSoftTrigger called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSendSoftTrigger(hDevice);
///                                ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_FEATURE        Not support soft trigger.
///@retval TY_STATUS_IDLE                   Camera device is not started
///          
///          Suggestions:
///            Please start the camera device first
///            Like this:
///              TYStartCapture(hDevice);
///              TYSendSoftTrigger(hDevice);
///          
///@retval TY_STATUS_WRONG_MODE             Not in trigger mode.
///@retval TY_STATUS_DEVICE_ERROR           Failed to send soft trigger
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera device is abnormal and cannot send soft trigger.
///              2.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_BUSY                   Failed to send soft trigger
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera is busy, the last soft trigger is not completed, please try again.
///          
TY_CAPI TYSendSoftTrigger         (TY_DEV_HANDLE hDevice);

///@brief  Register device status callback. Register NULL to clean callback.
///@param  [in]  hDevice       Device handle.
///@param  [in]  callback      Callback function.
///@param  [in]  userdata      User private data.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYRegisterEventCallback called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYRegisterEventCallback(hDevice, callback, userdata);
///                                      ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_BUSY                   Device is capturing.
TY_CAPI TYRegisterEventCallback   (TY_DEV_HANDLE hDevice, TY_EVENT_CALLBACK callback, void* userdata);

///@brief  Register imu callback. Register NULL to clean callback.
///@param  [in]  hDevice       Device handle.
///@param  [in]  callback      Callback function.
///@param  [in]  userdata      User private data.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYRegisterImuCallback called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYRegisterImuCallback(hDevice, callback, userdata);
///                                    ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_BUSY                   Device is capturing.
TY_CAPI TYRegisterImuCallback     (TY_DEV_HANDLE hDevice, TY_IMU_CALLBACK callback, void* userdata);

///@brief  Fetch one frame.
///@param  [in]  hDevice       Device handle.
///@param  [out] frame         Frame data to be filled.
///@param  [in]  timeout       Timeout in milliseconds. <0 for infinite.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYFetchFrame called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYFetchFrame(hDevice, pFrame, timeout);
///                           ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYFetchFrame called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYFetchFrame(hDevice, pFrame, timeout);
///                                    ^ is NULL
///          
///@retval TY_STATUS_IDLE                   Camera device is not started
///          
///          Suggestions:
///            Please start the camera device first
///            Like this:
///              TYStartCapture(hDevice);
///              TYFetchFrame(hDevice, pFrame, timeout);
///          
///@retval TY_STATUS_WRONG_MODE             Callback has been registered, this function is disabled.
///@retval TY_STATUS_TIMEOUT                Failed to get frame
///          
///          Suggestions:
///            Possible reasons:
///              1.Camera device is abnormal and cannot get frame.
///              2.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///              3.Timeout, frame acquisition timeout
///          
TY_CAPI TYFetchFrame              (TY_DEV_HANDLE hDevice, TY_FRAME_DATA* frame, int32_t timeout);

///@brief  Check whether a component has a specific feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] value         Whether has feature.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYHasFeature called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYHasFeature(hDevice, componentID, featureID, value);
///                           ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYHasFeature(hDevice, componentID, featureID, value);
///                                    ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_NULL_POINTER           TYHasFeature called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYHasFeature(hDevice, componentID, featureID, value);
///                                                            ^ is NULL
///          
TY_CAPI TYHasFeature              (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, bool* value);

///@brief  Get feature info.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] featureInfo   Feature info.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetFeatureInfo called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetFeatureInfo(hDevice, componentID, featureID, pFeatureInfo);
///                               ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetFeatureInfo(hDevice, componentID, featureID, pFeatureInfo);
///                                        ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetFeatureInfo(hDevice, componentID, featureID, pFeatureInfo);
///                                                     ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NULL_POINTER           TYGetFeatureInfo called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetFeatureInfo(hDevice, componentID, featureID, pFeatureInfo);
///                                                                ^ is NULL
///          
TY_CAPI TYGetFeatureInfo          (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, TY_FEATURE_INFO* featureInfo);

///@brief  Get value range of integer feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] intRange      Integer range to be filled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetIntRange called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetIntRange(hDevice, componentID, featureID, pIntRange);
///                            ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetIntRange(hDevice, componentID, featureID, pIntRange);
///                                     ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetIntRange(hDevice, componentID, featureID, pIntRange);
///                                                  ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetIntRange(hDevice, componentID, featureID, pIntRange);
///                                                  ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetIntRange called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetIntRange(hDevice, componentID, featureID, pIntRange);
///                                                             ^ is NULL
///          
TY_CAPI TYGetIntRange             (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, TY_INT_RANGE* intRange);

///@brief  Get value of integer feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] value         Integer value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetInt called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetInt(hDevice, componentID, featureID, pValue);
///                       ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetInt(hDevice, componentID, featureID, pValue);
///                                ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetInt(hDevice, componentID, featureID, pValue);
///                                             ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetInt(hDevice, componentID, featureID, pValue);
///                                             ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetInt called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetInt(hDevice, componentID, featureID, pValue);
///                                                        ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get int feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get int feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get int feature
///          
TY_CAPI TYGetInt                  (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, int32_t* value);

///@brief  Set value of integer feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  value         Integer value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetInt called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetInt(hDevice, componentID, featureID, value);
///                       ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetInt(hDevice, componentID, featureID, value);
///                                ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetInt(hDevice, componentID, featureID, value);
///                                             ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetInt(hDevice, componentID, featureID, value);
///                                             ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_OUT_OF_RANGE           Out of range
///          
///          Suggestions:
///            Please check the value
///            Like this:
///              TYSetInt(hDevice, componentID, featureID, value);
///                                                        ^ is out of range
///            The value is out of range, please use TYGetIntRange to get the range or check the camera xml description file
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set int feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set int feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set int feature
///          
TY_CAPI TYSetInt                  (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, int32_t value);

///@brief  Get value range of float feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] floatRange    Float range to be filled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetFloatRange called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetFloatRange(hDevice, componentID, featureID, pFloatRange);
///                              ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetFloatRange(hDevice, componentID, featureID, pFloatRange);
///                                       ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetFloatRange(hDevice, componentID, featureID, pFloatRange);
///                                                    ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetFloatRange(hDevice, componentID, featureID, pFloatRange);
///                                                    ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetFloatRange called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetFloatRange(hDevice, componentID, featureID, pFloatRange);
///                                                               ^ is NULL
///          
TY_CAPI TYGetFloatRange           (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, TY_FLOAT_RANGE* floatRange);

///@brief  Get value of float feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] value         Float value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetFloat called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetFloat(hDevice, componentID, featureID, pValue);
///                         ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetFloat(hDevice, componentID, featureID, pValue);
///                                  ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetFloat(hDevice, componentID, featureID, pValue);
///                                               ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetFloat(hDevice, componentID, featureID, pValue);
///                                               ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetFloat called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetFloat(hDevice, componentID, featureID, pValue);
///                                                          ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get float feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get float feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get float feature
///          
TY_CAPI TYGetFloat                (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, float* value);

///@brief  Set value of float feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  value         Float value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetFloat called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetFloat(hDevice, componentID, featureID, value);
///                         ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetFloat(hDevice, componentID, featureID, value);
///                                  ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetFloat(hDevice, componentID, featureID, value);
///                                               ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetFloat(hDevice, componentID, featureID, value);
///                                               ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_OUT_OF_RANGE           Out of range
///          
///          Suggestions:
///            Please check the value
///            Like this:
///              TYSetFloat(hDevice, componentID, featureID, value);
///                                                          ^ is out of range
///            The value is out of range, please use TYGetFloatRange to get the range or check the camera xml description file
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set float feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set float feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set float feature
///          
TY_CAPI TYSetFloat                (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, float value);

///@brief  Get number of enum entries.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] entryCount    Entry count.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetEnumEntryCount called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetEnumEntryCount(hDevice, componentID, featureID, pEntryCount);
///                                  ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetEnumEntryCount(hDevice, componentID, featureID, pEntryCount);
///                                           ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetEnumEntryCount(hDevice, componentID, featureID, pEntryCount);
///                                                         ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetEnumEntryCount(hDevice, componentID, featureID, pEntryCount);
///                                                        ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetEnumEntryCount called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetEnumEntryCount(hDevice, componentID, featureID, pEntryCount);
///                                                                   ^ is NULL
///          
TY_CAPI TYGetEnumEntryCount       (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint32_t* entryCount);

///@brief  Get list of enum entries.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] entries       Output entries.
///@param  [in]  entryCount    Array size of input parameter "entries".
///@param  [out] filledEntryCount      Number of filled entries.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetEnumEntryInfo called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetEnumEntryInfo(hDevice, componentID, featureID, entries, entryCount, filledEntryCount);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID: %d
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetEnumEntryInfo(hDevice, componentID, featureID, entries, entryCount, filledEntryCount);
///                                          ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID: %d
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetEnumEntryInfo(hDevice, componentID, featureID, entries, entryCount, filledEntryCount);
///                                                       ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetEnumEntryInfo(hDevice, componentID, featureID, entries, entryCount, filledEntryCount);
///                                                       ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetEnumEntryInfo called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetEnumEntryInfo(hDevice, componentID, featureID, pEnumDescription, entryCount, pFilledEntryCount);
///                                                                  ^              or             ^ is NULL
///          
TY_CAPI TYGetEnumEntryInfo        (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, TY_ENUM_ENTRY* entries, uint32_t entryCount, uint32_t* filledEntryCount);

///@brief  Get current value of enum feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] value         Enum value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetEnum called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetEnum(hDevice, componentID, featureID, pValue);
///                        ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID: %d
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetEnum(hDevice, componentID, featureID, pValue);
///                                 ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID: %d
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetEnum(hDevice, componentID, featureID, pValue);
///                                              ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetEnum(hDevice, componentID, featureID, pValue);
///                                              ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetEnum called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetEnum(hDevice, componentID, featureID, pValue);
///                                                         ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get enum feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get enum feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get enum feature
///          
TY_CAPI TYGetEnum                 (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint32_t* value);

///@brief  Set value of enum feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  value         Enum value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetEnum called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetEnum(hDevice, componentID, featureID, value);
///                        ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetEnum(hDevice, componentID, featureID, value);
///                                 ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetEnum(hDevice, componentID, featureID, value);
///                                              ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetEnum(hDevice, componentID, featureID, value);
///                                              ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_INVALID_PARAMETER      Out of range
///          
///          Suggestions:
///            Please check the value
///            Like this:
///              TYSetEnum(hDevice, componentID, featureID, value);
///                                                         ^ is out of range
///            The value is out of range, please use TYGetEnumEntryInfo to get the range or check the camera xml description file
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set enum feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set enum feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set enum feature
///          
TY_CAPI TYSetEnum                 (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint32_t value);

///@brief  Get value of bool feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] value         Bool value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetBool called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetBool(hDevice, componentID, featureID, pValue);
///                        ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetBool(hDevice, componentID, featureID, pValue);
///                                 ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetBool(hDevice, componentID, featureID, pValue);
///                                              ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetBool(hDevice, componentID, featureID, pValue);
///                                              ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetBool called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetBool(hDevice, componentID, featureID, pValue);
///                                                         ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get bool feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get bool feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get bool feature
///          
TY_CAPI TYGetBool                 (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, bool* value);

///@brief  Set value of bool feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  value         Bool value.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetBool called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetBool(hDevice, componentID, featureID, value);
///                        ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetBool(hDevice, componentID, featureID, value);
///                                 ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetBool(hDevice, componentID, featureID, value);
///                                              ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetBool(hDevice, componentID, featureID, value);
///                                              ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set bool feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set bool feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set bool feature
///          
TY_CAPI TYSetBool                 (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, bool value);

///@brief  Get internal buffer size of string feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] size          String length including '\0'.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetString called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                          ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                   ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                                ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                                ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetStringLength called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetStringLength(hDevice, componentID, featureID, pLength);
///                                                                 ^ is NULL
///          
///@see TYGetString
TY_CAPI TYGetStringLength         (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint32_t* size);

///@brief  Get value of string feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] buffer        String buffer.
///@param  [in]  bufferSize    Size of buffer.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetString called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                          ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                   ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                                ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetString(hDevice, componentID, featureID, buffer, bufferSize);
///                                                ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetString called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetString(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                           ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get string feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get string feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get string feature
///          
///@see TYGetStringLength
TY_CAPI TYGetString               (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, char* buffer, uint32_t bufferSize);

///@brief  Set value of string feature.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  buffer        String buffer.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetString called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetString(hDevice, componentID, featureID, pBuffer);
///                          ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetString(hDevice, componentID, featureID, pBuffer);
///                                   ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetString(hDevice, componentID, featureID, pBuffer);
///                                                ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetString(hDevice, componentID, featureID, pBuffer);
///                                                ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYSetString called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYSetString(hDevice, componentID, featureID, pBuffer);
///                                                           ^ is NULL
///          
///@retval TY_STATUS_OUT_OF_RANGE           Input string is too long.
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set string feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set string feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set string feature
///          
TY_CAPI TYSetString               (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, const char* buffer);

///@brief  Get value of struct.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] pStruct       Pointer of struct.
///@param  [in]  structSize    Size of input buffer pStruct.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetStruct called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                          ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                   ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetStruct called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                           ^ is NULL
///          
///@retval TY_STATUS_WRONG_SIZE             Struct size mismatch
///          
///          Suggestions:
///            Please check the struct size
///            Like this:
///              TYGetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                                    ^ is invalid
///            The struct size you entered does not match
///          
///@retval TY_STATUS_TIMEOUT                Failed to get struct feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get struct feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get struct feature
///          
TY_CAPI TYGetStruct               (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, void* pStruct, uint32_t structSize);

///@brief  Set value of struct.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [in]  pStruct       Pointer of struct.
///@param  [in]  structSize    Size of struct.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetStruct called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                          ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                   ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYSetStruct called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                           ^ is NULL
///          
///@retval TY_STATUS_WRONG_SIZE             Struct size mismatch
///          
///          Suggestions:
///            Please check the struct size
///            Like this:
///              TYSetStruct(hDevice, componentID, featureID, pStruct, structSize);
///                                                                    ^ is invalid
///            The struct size you entered does not match
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set struct feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set struct feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set struct feature
///          
TY_CAPI TYSetStruct               (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, void* pStruct, uint32_t structSize);

///@brief  Get the size of specified byte array zone.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] pSize         Size of specified byte array zone.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetByteArraySize called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetByteArraySize(hDevice, componentID, featureID, pSize);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetByteArraySize(hDevice, componentID, featureID, pSize);
///                                          ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetByteArraySize(hDevice, componentID, featureID, pSize);
///                                                       ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetByteArraySize(hDevice, componentID, featureID, pSize);
///                                                       ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetByteArraySize called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetByteArraySize(hDevice, componentID, featureID, pSize);
///                                                                  ^ is NULL
///          
TY_CAPI TYGetByteArraySize        (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint32_t* pSize);

///@brief  Read byte array from device.
///@param  [in]  hDevice       Device handle.
///@param  [in] componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] pBuffer       Byte buffer.
///@param  [in]  bufferSize    Size of buffer.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetByteArray called with invalid device handle
///          
///          Suggestions: 
///            Please check device handle
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, buffer, bufferSize);
///                             ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, buffer, bufferSize);
///                                      ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, buffer, bufferSize);
///                                                   ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, buffer, bufferSize);
///                                                   ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetByteArray called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                              ^ is NULL
///          
///@retval TY_STATUS_WRONG_SIZE             Array size mismatch
///          
///          Suggestions:
///            Please check the array size
///            Like this:
///              TYGetByteArray(hDevice, componentID, featureID, buffer, bufferSize);
///                                                                      ^ is invalid
///            The array size you entered does not match
///          
///@retval TY_STATUS_TIMEOUT                Failed to get byte array feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get byte array feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get byte array feature
///          
TY_CAPI TYGetByteArray            (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, uint8_t* pBuffer, uint32_t bufferSize);

///@brief  Write byte array to device.
///@param  [in]  hDevice       Device handle.
///@param  [in] componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] pBuffer       Byte buffer.
///@param  [in]  bufferSize    Size of buffer.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYSetByteArray called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                             ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                      ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                   ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                   ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYSetByteArray called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                              ^ is NULL
///          
///@retval TY_STATUS_WRONG_SIZE             Array size mismatch
///          
///          Suggestions:
///            Please check the array size
///            Like this:
///              TYSetByteArray(hDevice, componentID, featureID, pBuffer, bufferSize);
///                                                                       ^ is invalid
///            The array size you entered does not match
///          
///@retval TY_STATUS_BUSY                   Device is capturing, the feature is locked.
///@retval TY_STATUS_TIMEOUT                Failed to set byte array feature
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to set byte array feature
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot set byte array feature
///          
TY_CAPI TYSetByteArray            (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, const uint8_t* pBuffer, uint32_t bufferSize);

///@brief  Write byte array to device.
///@param  [in]  hDevice       Device handle.
///@param  [in] componentID   Component ID.
///@param  [in]  featureID     Feature ID.
///@param  [out] pAttr         Byte array attribute to be filled.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetByteArrayAttr called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetByteArrayAttr(hDevice, componentID, featureID, pAttr);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetByteArrayAttr(hDevice, componentID, featureID, pAttr);
///                                          ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_INVALID_FEATURE        Invalid feature ID
///          
///          Suggestions:
///            Please check featureID parameter
///            Like this:
///              TYGetByteArrayAttr(hDevice, componentID, featureID, pAttr);
///                                                       ^ is invalid
///            You entered an invalid featureID parameter
///            You can get a list of features of the camera device through TYGetFeatureList
///            You can also view the features of the camera device by obtaining the xml description file of the camera
///          
///@retval TY_STATUS_NOT_PERMITTED          The feature is not writable.
///@retval TY_STATUS_WRONG_TYPE             Feature type mismatch
///          
///          Suggestions:
///            Please check the feature type
///            Like this:
///              TYGetByteArrayAttr(hDevice, componentID, featureID, pAttr);
///                                                       ^ type mismatch
///            The feature type you entered does not match. You can use TYFeatureType to check the feature type
///          
///@retval TY_STATUS_NULL_POINTER           TYGetByteArrayAttr called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetByteArrayAttr(hDevice, componentID, featureID, pAttr);
///                                                                  ^ is NULL
///          
TY_CAPI TYGetByteArrayAttr        (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_ID featureID, TY_BYTEARRAY_ATTR* pAttr);

///@brief  Get the size of device features.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [out] size          Size of all feature cnt.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceFeatureNumber called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceFeatureNumber(hDevice, componentID, size);
///                                       ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetDeviceFeatureNumber(hDevice, componentID, size);
///                                                ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceFeatureNumber called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceFeatureNumber(hDevice, componentID, size);
///                                                             ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get feature number
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get feature number
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get feature number
///          
TY_CAPI TYGetDeviceFeatureNumber  (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, uint32_t* size);

///@brief  Get the all features by comp id.
///@param  [in]  hDevice       Device handle.
///@param  [in]  componentID   Component ID.
///@param  [out] featureInfo   Output feature info.
///@param  [in]  entryCount    Array size of input parameter "featureInfo".
///@param  [out] filledEntryCount      Number of filled featureInfo.
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceFeatureInfo called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceFeatureInfo(hDevice, componentID, featureInfo, entryCount, filledEntryCount);
///                                     ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_INVALID_COMPONENT      Invalid component ID
///          
///          Suggestions:
///            Please check componentID parameter
///            Like this:
///              TYGetDeviceFeatureInfo(hDevice, componentID, featureInfo, entryCount, filledEntryCount);
///                                              ^ is invalid
///            componentID should be the value returned by TYGetComponentIDs
///            You can also view the components of the camera by obtaining the xml description file of the camera device
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceFeatureInfo called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceFeatureInfo(hDevice, componentID, featureInfo, entryCount, filledEntryCount);
///                                                           ^           or           ^ is NULL
///          
///@retval TY_STATUS_TIMEOUT                Failed to get feature info
///          
///          Suggestions:
///            Possible reasons:
///              1.Network communication is abnormal, please check whether the network connection is normal, whether firewall and other software block the communication, and whether the packet loss rate is too high.
///          
///@retval TY_STATUS_DEVICE_ERROR           Failed to get feature info
///          
///          Suggestions:
///            Possible reasons:
///              1.The feature of the camera device is not available or not implemented
///              2.Camera device is abnormal and cannot get feature info
///          
TY_CAPI TYGetDeviceFeatureInfo    (TY_DEV_HANDLE hDevice, TY_COMPONENT_ID componentID, TY_FEATURE_INFO* featureInfo, uint32_t entryCount, uint32_t* filledEntryCount);

///@brief  Get the Device xml size.
///@param  [in]  hDevice       Device handle.
///@param  [out] size          The size of device xml string
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             Not call TYInitLib
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceXMLSize called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceXMLSize(hDevice, size);
///                                 ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceXMLSize called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceXMLSize(hDevice, size);
///                                          ^ is NULL
///          
TY_CAPI TYGetDeviceXMLSize        (TY_DEV_HANDLE hDevice, uint32_t* size);

///@brief  Get the Device xml string.
///@param  [in]  hDevice       Device handle.
///@param  [in]  xml           The buffer to store xml
///@param  [in]  in_size       The size buffer
///@param  [out] out_size      The actual size write in buffer
///@retval TY_STATUS_OK                     Succeed.
///@retval TY_STATUS_NOT_INITED             Not call TYInitLib
///@retval TY_STATUS_WRONG_SIZE             XML buffer size is not enough
///          
///          Suggestions:
///            XML buffer size is not enough
///            Like this:
///              TYGetDeviceXML(hDevice, xml, in_size, out_size);
///                                           ^ is invalid
///            XML buffer size is not enough, please use TYGetDeviceXMLSize to get the xml size
///          
///@retval TY_STATUS_INVALID_HANDLE         TYGetDeviceXML called with invalid device handle
///          
///          Suggestions:
///            Please check device handle
///            Like this:
///              TYGetDeviceXML(hDevice, xml, in_size, out_size);
///                             ^ is invalid
///            The hDevice parameter you input is not recorded
///            Possible reasons:
///              1.TYOpenDevice failed to open device and get correct handle
///              2.Memory in stack to store handle data is corrupted
///              3.After getting handle, you updated device list by calling TYUpdateDeviceList
///          
///@retval TY_STATUS_NULL_POINTER           TYGetDeviceXML called with NULL pointer
///          
///          Suggestions:
///            Please check your code
///            Like this:
///              TYGetDeviceXML(hDevice, xml, in_size, out_size);
///                                      ^      or     ^ is NULL
///          
TY_CAPI TYGetDeviceXML            (TY_DEV_HANDLE hDevice, char *xml, const uint32_t in_size, uint32_t* out_size);

#endif //TY_API_H_
