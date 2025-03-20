#pragma once

#include <memory>
#include <vector>
#include <set>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <stdint.h>

#include "Frame.hpp"

namespace percipio_layer {

class TYDevice;
class DeviceList;
class TYContext;
class TYFrame;
class FastCamera;

static std::string parseInterfaceID(std::string &ifaceId)
{
    std::string type_s = ifaceId.substr(0, ifaceId.find('-'));
    if ("usb" == type_s) {
        //add usb specific parse if needed
    }
    if ("eth" == type_s || "wifi" == type_s) {
        //eth-2c:f0:5d:ac:5d:6265eea8c0
        //eth-2c:f0:5d:ac:5d:62
        size_t IdLength = 18 + type_s.length();
        std::string new_id = ifaceId.substr(0, IdLength);
        //                     65eea8c0
        std::string ip_s = ifaceId.substr(IdLength, ifaceId.size() - IdLength);
        //base = 16
        uint32_t ip = static_cast<uint32_t>(std::stoul(ip_s, nullptr, 16));
        uint8_t *ip_arr = (uint8_t *)&ip;
        new_id += " ip:";
        for(int i = 0; i <  3; i++) {
            new_id += std::to_string((uint32_t) ip_arr[i]) + ".";
        }
        new_id += std::to_string((uint32_t) ip_arr[3]);
        return new_id;
    }
    return ifaceId;
}

class TYDeviceInfo
{
    public:
        ~TYDeviceInfo();
        TYDeviceInfo(TYDeviceInfo const&) = delete;
        void operator=(TYDeviceInfo const&) = delete;

        friend class TYDevice;
        friend class DeviceList;

        const char* id()                                { return _info.id; }
        const TY_INTERFACE_INFO&  Interface()           { return _info.iface; }
        
        const char*    vendorName()
        {
            //specific Vendor name for some camera
            if (strlen(_info.userDefinedName) != 0) {
                return _info.userDefinedName;
            } else {
                return _info.vendorName;
            }
        }
        const char*    modelName()                      { return _info.modelName; }
        const char*    buildHash()                      { return _info.buildHash; }
        const char*    configVersion()                  { return _info.configVersion; }

        const TY_VERSION_INFO&     hardwareVersion()    { return _info.hardwareVersion; }
        const TY_VERSION_INFO&     firmwareVersion()    { return _info.firmwareVersion; }

        const char*    mac();
        const char*    ip();
        const char*    netmask();
        const char*    gateway();
        const char*    broadcast();
    private:
        TYDeviceInfo(const TY_DEVICE_BASE_INFO& info);
        TY_DEVICE_BASE_INFO _info;
};

typedef std::function<void(void* userdata)>      EventCallback;
typedef std::pair<void*, EventCallback>          event_pair;
static void eventCallback(TY_EVENT_INFO *event_info, void *userdata);
class TYDevice
{
    public:
        ~TYDevice();
        void operator=(TYDevice const&) = delete;

        friend class FastCamera;
        friend class TYStream;
        friend class DeviceList;
        friend class TYPropertyManager;
        friend void eventCallback(TY_EVENT_INFO *event_info, void *userdata);

        std::shared_ptr<TYDeviceInfo>           getDeviceInfo();
        void      registerEventCallback     (const TY_EVENT eventID, void* data, EventCallback cb);
        
    private:
        TYDevice(const TY_DEV_HANDLE handle, const TY_DEVICE_BASE_INFO& info);
        
        TY_DEV_HANDLE _handle;
        TY_DEVICE_BASE_INFO _dev_info;

        std::map<TY_EVENT, event_pair> _eventCallbackMap;

        std::function<void(TY_EVENT_INFO*)> _event_callback;
        void onDeviceEventCallback(const TY_EVENT_INFO *event_info);
};

class DeviceList {
    public:
        ~DeviceList();
        DeviceList(DeviceList const&) = delete;
        void operator=(DeviceList const&) = delete;

        bool empty()    { return devs.size() == 0; }
        int  devCount() { return devs.size(); }

        std::shared_ptr<TYDeviceInfo>   getDeviceInfo(int idx);
        std::shared_ptr<TYDevice>       getDevice(int idx);
        std::shared_ptr<TYDevice>       getDeviceBySN(const char* sn);
        std::shared_ptr<TYDevice>       getDeviceByIP(const char* ip);

        friend class TYContext;
    private:
        std::vector<TY_DEVICE_BASE_INFO> devs;
        static std::set<TY_INTERFACE_HANDLE> gifaces;
        DeviceList(std::vector<TY_DEVICE_BASE_INFO>& devices);
};

enum ForceIPStyle {
    ForceIPStyleDynamic = 0,
    ForceIPStyleForce = 1,
    ForceIPStyleStatic = 2
};

class TYContext {
public:
    static TYContext& getInstance() {
        static TYContext instance;
        return instance;
    }
 
    TYContext(TYContext const&) = delete;
    void operator=(TYContext const&) = delete;
 
    std::shared_ptr<DeviceList> queryDeviceList(const char *iface = nullptr);
    std::shared_ptr<DeviceList> queryNetDeviceList(const char *iface = nullptr);
 
    bool ForceNetDeviceIP(const ForceIPStyle style, const std::string& mac, const std::string& ip, const std::string& mask, const std::string& gateway);

private:
    TYContext() {
        ASSERT_OK(TYInitLib());
        TY_VERSION_INFO ver;
        ASSERT_OK( TYLibVersion(&ver) );
        std::cout << "=== lib version: " << ver.major << "." << ver.minor << "." << ver.patch << std::endl;
    }

    ~TYContext() {
        ASSERT_OK(TYDeinitLib());
    }
};

class TYCamInterface
{
    public:
        TYCamInterface();
        ~TYCamInterface();

        TY_STATUS Reset();
        void List(std::vector<std::string>& );
    private:
        std::vector<TY_INTERFACE_INFO> ifaces;
};

class FastCamera
{
    public:
    enum stream_idx
    {
        stream_depth = 0x1,
        stream_color = 0x2,
        stream_ir_left = 0x4,
        stream_ir_right = 0x8,
        stream_ir = stream_ir_left
    };
        friend class TYFrame;
        FastCamera();
        FastCamera(const char* sn);
        ~FastCamera();

        virtual TY_STATUS open(const char* sn);
        TY_STATUS setIfaceId(const char* inf);
        virtual TY_STATUS openByIP(const char* ip);
        virtual bool has_stream(stream_idx idx);
        virtual TY_STATUS stream_enable(stream_idx idx);
        virtual TY_STATUS stream_disable(stream_idx idx);

        virtual TY_STATUS start();
        virtual TY_STATUS stop();
        virtual void close();

        std::shared_ptr<TYFrame> tryGetFrames(uint32_t timeout_ms);

        TY_DEV_HANDLE handle() {return device->_handle; }

        void RegisterOfflineEventCallback(EventCallback cb, void* data) { device->registerEventCallback(TY_EVENT_DEVICE_OFFLINE, data, cb); }
    
    private:
        std::string     mIfaceId;
        std::mutex      _dev_lock;

        TY_COMPONENT_ID components = 0;
#define BUF_CNT      (3)

        bool isRuning = false;
        std::shared_ptr<TYFrame> fetchFrames(uint32_t timeout_ms);
        TY_STATUS doStop();

        std::shared_ptr<TYDevice> device;
        std::vector<uint8_t> stream_buffer[BUF_CNT];
};

}
