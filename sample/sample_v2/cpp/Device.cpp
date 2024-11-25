#include "Device.hpp"

struct to_string
{
    std::ostringstream ss;
    template<class T> to_string & operator << (const T & val) { ss << val; return *this; }
    operator std::string() const { return ss.str(); }
};

static std::string TY_ERROR(TY_STATUS status)
{
    return to_string() << status << "(" << TYErrorString(status) << ").";
}

static inline TY_STATUS searchDevice(std::vector<TY_DEVICE_BASE_INFO>& out, const char *inf_id = nullptr, TY_INTERFACE_TYPE type = TY_INTERFACE_ALL)
{
    out.clear();
    ASSERT_OK( TYUpdateInterfaceList() );

    uint32_t n = 0;
    ASSERT_OK( TYGetInterfaceNumber(&n) );
    if(n == 0) return TY_STATUS_ERROR;

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    ASSERT_OK( TYGetInterfaceList(&ifaces[0], n, &n) );

    bool found = false;
    std::vector<TY_INTERFACE_HANDLE> hIfaces;
    for(uint32_t i = 0; i < ifaces.size(); i++){
        TY_INTERFACE_HANDLE hIface;
        if(type & ifaces[i].type) {
                //Interface Not setted
            if (nullptr == inf_id ||
                //Interface been setted and matched
                strcmp(inf_id, ifaces[i].id) == 0) {
                ASSERT_OK( TYOpenInterface(ifaces[i].id, &hIface) );
                hIfaces.push_back(hIface);
                found = true;
                //Interface been setted, found and just break
                if(nullptr != inf_id) {
                    break;
                }
            }
        }

    }
    if(!found) return TY_STATUS_ERROR;
    updateDevicesParallel(hIfaces);

    for (uint32_t i = 0; i < hIfaces.size(); i++) {
        TY_INTERFACE_HANDLE hIface = hIfaces[i];
        uint32_t n = 0;
        TYGetDeviceNumber(hIface, &n);
        if(n > 0){
            std::vector<TY_DEVICE_BASE_INFO> devs(n);
            TYGetDeviceList(hIface, &devs[0], n, &n);
            for(uint32_t j = 0; j < n; j++) {
                out.push_back(devs[j]);
            }
        }
        TYCloseInterface(hIface);
    }

    if(out.size() == 0){
      std::cout << "not found any device" << std::endl;
      return TY_STATUS_ERROR;
    }

    return TY_STATUS_OK;
}

namespace percipio_layer {

TYDeviceInfo::TYDeviceInfo(const TY_DEVICE_BASE_INFO& info)
{
    _info = info;
}

TYDeviceInfo::~TYDeviceInfo()
{

}

const char* TYDeviceInfo::mac()
{
    if(!TYIsNetworkInterface(_info.iface.type)) {
        return nullptr;
    }
    return _info.netInfo.mac;
}

const char* TYDeviceInfo::ip()
{
    if(!TYIsNetworkInterface(_info.iface.type))
        return nullptr;
    return _info.netInfo.ip;
}

const char* TYDeviceInfo::netmask()
{
    if(!TYIsNetworkInterface(_info.iface.type))
        return nullptr;
    return _info.netInfo.netmask;
}

const char* TYDeviceInfo::gateway()
{
    if(!TYIsNetworkInterface(_info.iface.type))
        return nullptr;
    return _info.netInfo.gateway;
}

const char* TYDeviceInfo::broadcast()
{
    if(!TYIsNetworkInterface(_info.iface.type))
        return nullptr;
    return _info.netInfo.broadcast;
}

static void eventCallback(TY_EVENT_INFO *event_info, void *userdata) {
    TYDevice* handle = (TYDevice*)userdata;
    handle->_event_callback(event_info);
}

 TYCamInterface::TYCamInterface()
 {
    TYContext::getInstance();
    Reset();
 }

TYCamInterface::~TYCamInterface()
{

}

TY_STATUS TYCamInterface::Reset()
{
    TY_STATUS status;
    status = TYUpdateInterfaceList();
    if(status != TY_STATUS_OK) return status;

    uint32_t n = 0;
    status = TYGetInterfaceNumber(&n);
    if(status != TY_STATUS_OK) return status;

    if(n == 0) return TY_STATUS_OK;

    ifaces.resize(n);
    status = TYGetInterfaceList(&ifaces[0], n, &n);
    return status;
}

void TYCamInterface::List(std::vector<std::string>& interfaces)
{
    for(auto& iter : ifaces) {
        std::cout << iter.id << std::endl;
        interfaces.push_back(iter.id);
    }
}

FastCamera::FastCamera()
{

}

FastCamera::FastCamera(const char* sn)
{
    const char *inf = nullptr;
    if (!mIfaceId.empty()) {
        inf = mIfaceId.c_str();
    }
    auto devList = TYContext::getInstance().queryDeviceList(inf);
    if(devList->empty()) {
        return;
    }

    device = (sn && strlen(sn) != 0) ? devList->getDeviceBySN(sn) : devList->getDevice(0);
    if(!device) {
        return;
    }

    TYGetComponentIDs(device->_handle, &components);
}

TY_STATUS FastCamera::open(const char* sn)
{
    const char *inf = nullptr;
    if (!mIfaceId.empty()) {
        inf = mIfaceId.c_str();
    }

    auto devList = TYContext::getInstance().queryDeviceList(inf);
    if(devList->empty()) {
        std::cout << "deivce list is empty!" << std::endl;
        return TY_STATUS_ERROR;
    }

    device = (sn && strlen(sn) != 0) ? devList->getDeviceBySN(sn) : devList->getDevice(0);
    if(!device) {
        return TY_STATUS_ERROR;
    }

    return TYGetComponentIDs(device->_handle, &components);
}

TY_STATUS FastCamera::openByIP(const char* ip)
{
    const char *inf = nullptr;
    if (!mIfaceId.empty()) {
        inf = mIfaceId.c_str();
    }

    std::unique_lock<std::mutex> lock(_dev_lock);
    auto devList = TYContext::getInstance().queryNetDeviceList(inf);
    if(devList->empty()) {
        std::cout << "net deivce list is empty!" << std::endl;
        return TY_STATUS_ERROR;
    }

    device = (ip && strlen(ip) != 0) ? devList->getDeviceByIP(ip) : devList->getDevice(0);
    if(!device) {
        std::cout << "open device failed!" << std::endl;
        return TY_STATUS_ERROR;
    }

    return TYGetComponentIDs(device->_handle, &components);
}

TY_STATUS FastCamera::setIfaceId(const char* inf)
{
    mIfaceId = inf;
    return TY_STATUS_OK;
}

FastCamera::~FastCamera()
{
    if(isRuning) {
        doStop();
    }
}

void FastCamera::close()
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    if(isRuning) {
        doStop();
    }
    
    if(device) device.reset();
}

std::shared_ptr<TYFrame> FastCamera::fetchFrames(uint32_t timeout_ms)
{
    TY_FRAME_DATA tyframe;
    TY_STATUS status = TYFetchFrame(handle(), &tyframe, timeout_ms);
    if(status != TY_STATUS_OK) {
        std::cout << "Frame fetch failed with err code: " << status << "(" << TYErrorString(status) << ")."<< std::endl;
        return std::shared_ptr<TYFrame>();
    }
    
    std::shared_ptr<TYFrame> frame = std::shared_ptr<TYFrame>(new TYFrame(tyframe));
    CHECK_RET(TYEnqueueBuffer(handle(), tyframe.userBuffer, tyframe.bufferSize));
    return frame;
}

static TY_COMPONENT_ID StreamIdx2CompID(FastCamera::stream_idx idx)
{
    TY_COMPONENT_ID comp = 0;
    switch (idx)
    {
    case FastCamera::stream_depth:
        comp = TY_COMPONENT_DEPTH_CAM;
        break;
    case FastCamera::stream_color:
        comp = TY_COMPONENT_RGB_CAM;
        break; 
    case FastCamera::stream_ir_left:
        comp = TY_COMPONENT_IR_CAM_LEFT;
        break;
    case FastCamera::stream_ir_right:
        comp = TY_COMPONENT_IR_CAM_RIGHT;
        break;
    default:
        break;
    }

    return comp;
}
bool FastCamera::has_stream(stream_idx idx)
{
    return components & StreamIdx2CompID(idx);
}

TY_STATUS FastCamera::stream_enable(stream_idx idx)
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    return TYEnableComponents(handle(), StreamIdx2CompID(idx));
}

TY_STATUS FastCamera::stream_disable(stream_idx idx)
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    return TYDisableComponents(handle(), StreamIdx2CompID(idx));
}

TY_STATUS FastCamera::start()
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    if(isRuning) {
        std::cout << "Device is busy!" << std::endl;
        return TY_STATUS_BUSY;
    }

    uint32_t stream_buffer_size;
    TY_STATUS status = TYGetFrameBufferSize(handle(), &stream_buffer_size);
    if(status != TY_STATUS_OK) {
        std::cout << "Get frame buffer size failed with error code: " << TY_ERROR(status) << std::endl;
        return status;
    }
    if(stream_buffer_size == 0) {
        std::cout << "Frame buffer size is 0, is the data flow component not enabled?" << std::endl;
        return TY_STATUS_DEVICE_ERROR;
    }

    for(int i = 0; i < BUF_CNT; i++) {
        stream_buffer[i].resize(stream_buffer_size);
        TYEnqueueBuffer(handle(), &stream_buffer[i][0], stream_buffer_size);
    }

    status = TYStartCapture(handle());
    if(TY_STATUS_OK != status) {
        std::cout << "Start capture failed with error code: " << TY_ERROR(status) << std::endl;
        return status;
    }

    isRuning = true;
    return TY_STATUS_OK;
}

TY_STATUS FastCamera::stop()
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    return doStop();
}

TY_STATUS FastCamera::doStop()
{
    if(!isRuning) 
        return TY_STATUS_IDLE;
    
    isRuning = false;
    
    TY_STATUS status = TYStopCapture(handle());
    if(TY_STATUS_OK != status) {
        std::cout << "Stop capture failed with error code: " << TY_ERROR(status) << std::endl;
    }
    //Stop will stop receive, need TYClearBufferQueue any way
    //Ignore TYClearBufferQueue ret val
    TYClearBufferQueue(handle());
    for(int i = 0; i < BUF_CNT; i++) {
        stream_buffer[i].clear();
    }

    return status;
}

std::shared_ptr<TYFrame> FastCamera::tryGetFrames(uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(_dev_lock);
    return fetchFrames(timeout_ms);
}

TYDevice::TYDevice(const TY_DEV_HANDLE handle, const TY_DEVICE_BASE_INFO& info)
{
    _handle = handle;
    _dev_info = info;
    _event_callback = std::bind(&TYDevice::onDeviceEventCallback, this, std::placeholders::_1);
    TYRegisterEventCallback(_handle, eventCallback, this);
}

TYDevice::~TYDevice()
{
    CHECK_RET(TYCloseDevice(_handle));
}

void  TYDevice::registerEventCallback(const TY_EVENT eventID, void* data, EventCallback cb)
{
    _eventCallbackMap[eventID] = {data, cb};
}

void TYDevice::onDeviceEventCallback(const TY_EVENT_INFO *event_info)
{
    if(_eventCallbackMap[event_info->eventId].second != nullptr) {
        _eventCallbackMap[event_info->eventId].second(_eventCallbackMap[event_info->eventId].first);
    }
}

std::shared_ptr<TYDeviceInfo> TYDevice::getDeviceInfo()
{
    return std::shared_ptr<TYDeviceInfo>(new TYDeviceInfo(_dev_info));
}

std::set<TY_INTERFACE_HANDLE> DeviceList::gifaces;
DeviceList::DeviceList(std::vector<TY_DEVICE_BASE_INFO>& devices)
{
    devs = devices;
}

DeviceList::~DeviceList()
{
    for (TY_INTERFACE_HANDLE iface : gifaces) {
        TYCloseInterface(iface);
    }
    gifaces.clear();
}

std::shared_ptr<TYDeviceInfo> DeviceList::getDeviceInfo(int idx)
{
    if((idx < 0) || (idx > devCount())) {
        std::cout << "idx out of range" << std::endl;
        return nullptr;
    }
    
    return std::shared_ptr<TYDeviceInfo>(new TYDeviceInfo(devs[idx]));
}

std::shared_ptr<TYDevice> DeviceList::getDevice(int idx)
{
    if((idx < 0) || (idx > devCount())) {
        std::cout << "idx out of range" << std::endl;
        return nullptr;
    }

    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    TY_STATUS status = TY_STATUS_OK;
    status = TYOpenInterface(devs[idx].iface.id, &hIface);
    if(status != TY_STATUS_OK)  {
        std::cout << "Open interface failed with error code: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }

    gifaces.insert(hIface);
    std::string ifaceId = devs[idx].iface.id;
    std::string open_log = std::string("open device ") + devs[idx].id +
        "\non interface " + parseInterfaceID(ifaceId);
    std::cout << open_log << std::endl;
    status = TYOpenDevice(hIface, devs[idx].id, &hDevice);
    if(status != TY_STATUS_OK) {
        std::cout << "Open device < " << devs[idx].id << "> failed with error code: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }

    TY_DEVICE_BASE_INFO info;
    status = TYGetDeviceInfo(hDevice, &info);
    if(status != TY_STATUS_OK) {
        std::cout << "Get device info failed with error code: " << TY_ERROR(status) << std::endl;
        return nullptr;
    }

    return std::shared_ptr<TYDevice>(new TYDevice(hDevice, info));
}

std::shared_ptr<TYDevice> DeviceList::getDeviceBySN(const char* sn)
{
    TY_STATUS status = TY_STATUS_OK;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    
    if(!sn) {
        std::cout << "Invalid parameters" << std::endl;
        return nullptr;
    }

    for(size_t i = 0; i < devs.size(); i++) {
        if(strcmp(devs[i].id, sn) == 0) {
            status = TYOpenInterface(devs[i].iface.id, &hIface);
            if(status != TY_STATUS_OK)  continue;

            gifaces.insert(hIface);
            std::string ifaceId = devs[i].iface.id;
            std::string open_log = std::string("open device ") + devs[i].id +
                "\non interface " + parseInterfaceID(ifaceId);
            std::cout << open_log << std::endl;
            status = TYOpenDevice(hIface, devs[i].id, &hDevice);
            if(status != TY_STATUS_OK) continue;

            TY_DEVICE_BASE_INFO info;
            status = TYGetDeviceInfo(hDevice, &info);
            if(status != TY_STATUS_OK) {
                TYCloseDevice(hDevice);
                continue;
            }
            return std::shared_ptr<TYDevice>(new TYDevice(hDevice, info));
        }
    }

    std::cout << "Device <sn:" << sn << "> not found!" << std::endl;
    return nullptr;
}

std::shared_ptr<TYDevice> DeviceList::getDeviceByIP(const char* ip)
{
    TY_STATUS status = TY_STATUS_OK;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    if(!ip) {
        std::cout << "Invalid parameters" << std::endl;
        return nullptr;
    }

    for(size_t i = 0; i < devs.size(); i++) {        
        if(TYIsNetworkInterface(devs[i].iface.type)) {
            status = TYOpenInterface(devs[i].iface.id, &hIface);
            if(status != TY_STATUS_OK)  continue;
            std::string open_log = "open device ";
            if(ip && strlen(ip)) {
                open_log += ip;
                status = TYOpenDeviceWithIP(hIface, ip, &hDevice);
            } else {
                open_log += devs[i].id;
                status = TYOpenDevice(hIface, devs[i].id, &hDevice);
            }
            std::string ifaceId = devs[i].iface.id;
            open_log += "\non interface " + parseInterfaceID(ifaceId);
            std::cout << open_log << std::endl;

            if(status != TY_STATUS_OK) continue;

            TY_DEVICE_BASE_INFO info;
            status = TYGetDeviceInfo(hDevice, &info);
            if(status != TY_STATUS_OK) {
                TYCloseDevice(hDevice);
                continue;;
            }

            return std::shared_ptr<TYDevice>(new TYDevice(hDevice, info));
        }
    }

    std::cout << "Device <ip:" << ip << "> not found!" << std::endl;
    return nullptr;
}

std::shared_ptr<DeviceList> TYContext::queryDeviceList(const char *iface)
{
    std::vector<TY_DEVICE_BASE_INFO> devs;
    searchDevice(devs, iface);
    return std::shared_ptr<DeviceList>(new DeviceList(devs));
}

std::shared_ptr<DeviceList> TYContext::queryNetDeviceList(const char *iface)
{
    std::vector<TY_DEVICE_BASE_INFO> devs;
    searchDevice(devs, iface, TY_INTERFACE_ETHERNET | TY_INTERFACE_IEEE80211);
    return std::shared_ptr<DeviceList>(new DeviceList(devs));
}

bool TYContext::ForceNetDeviceIP(const ForceIPStyle style, const std::string& mac, const std::string& ip, const std::string& mask, const std::string& gateway)
{
    ASSERT_OK( TYUpdateInterfaceList() );

    uint32_t n = 0;
    ASSERT_OK( TYGetInterfaceNumber(&n) );
    if(n == 0) return false;
    
    std::vector<TY_INTERFACE_INFO> ifaces(n);
    ASSERT_OK( TYGetInterfaceList(&ifaces[0], n, &n) );
    ASSERT( n == ifaces.size() );

    bool   open_needed  = false;
    const char * ip_save      = ip.c_str();
    const char * netmask_save = mask.c_str();
    const char * gateway_save = gateway.c_str();
    switch(style)
    {
        case ForceIPStyleDynamic:
            ip_save      = "0.0.0.0";
            netmask_save = "0.0.0.0";
            gateway_save = "0.0.0.0";
            break;
        case ForceIPStyleStatic:
            open_needed = true;
            break;
        default:
            break;
    }

    bool result = false;
    for(uint32_t i = 0; i < n; i++) {
        if(TYIsNetworkInterface(ifaces[i].type)) {
            TY_INTERFACE_HANDLE hIface;
            ASSERT_OK( TYOpenInterface(ifaces[i].id, &hIface) );
            if (TYForceDeviceIP(hIface, mac.c_str(), ip.c_str(), mask.c_str(), gateway.c_str()) == TY_STATUS_OK) {
                LOGD("**** Set Temporary IP/Netmask/Gateway ...Done! ****");
                if(open_needed) {
                    TYUpdateDeviceList(hIface);
                    TY_DEV_HANDLE hDev;
                    if(TYOpenDeviceWithIP(hIface, ip.c_str(), &hDev) == TY_STATUS_OK){
                        int32_t ip_i[4];
                        uint8_t ip_b[4];
                        int32_t ip32;
                        sscanf(ip_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_IP, ip32) );
                        sscanf(netmask_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_SUBMASK, ip32) );
                        sscanf(gateway_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                        ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                        ip32 = TYIPv4ToInt(ip_b);
                        ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_GATEWAY, ip32) );

                        result = true;
                        std::cout << "**** Set Persistent IP/Netmask/Gateway ...Done! ****" <<std::endl;
                    } else {
                        result = false;
                    }
                } else {
                    result = true;
                }
            }
            ASSERT_OK( TYCloseInterface(hIface));        
        }
    }
    return result;
}
}
