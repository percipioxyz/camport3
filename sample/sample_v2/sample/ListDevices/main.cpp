#include "Device.hpp"

using namespace percipio_layer;

static void printDeviceInfo(std::shared_ptr<TYDeviceInfo> devInfo)
{
    std::cout << "    - device " << devInfo->id() << std::endl;
    std::cout << "          vendor     : " << devInfo->vendorName() << std::endl;
    std::cout << "          model      : " << devInfo->modelName() << std::endl;
    if(TYIsNetworkInterface(devInfo->Interface().type)) {
        std::cout << "          device MAC : " << devInfo->mac() << std::endl;
        std::cout << "          device IP  : " << devInfo->ip() << std::endl;
    }
}

static void printInterfaceInfo(const TY_INTERFACE_INFO& infInfo)
{
    std::cout << "- Interface:" << std::endl;
    std::cout << "  name:" << infInfo.name <<  std::endl;
    std::cout << "  id:" << infInfo.id <<  std::endl;
    std::cout << "  type:" << infInfo.type<<  std::endl;
    if (TYIsNetworkInterface(infInfo.type)) {
        std::cout << "  Mac:" << infInfo.netInfo.mac <<  std::endl;
        std::cout << "  ip:" << infInfo.netInfo.ip<<  std::endl;
        std::cout << "  netmask:" << infInfo.netInfo.netmask <<  std::endl;
        std::cout << "  gateway:" << infInfo.netInfo.gateway <<  std::endl;
        std::cout << "  broadcast:" << infInfo.netInfo.broadcast<<  std::endl;
    }
}

int main(int argc, char* argv[])
{
    //This Func will list all devices interface by interface, and stored in order
    auto devList = TYContext::getInstance().queryDeviceList();
    if(devList->empty()) {
        return TY_STATUS_ERROR;
    }
    std::string ifaceid;
    for(int i = 0; i < devList->devCount(); i++) {
        auto devInfo =  devList->getDeviceInfo(i);
        const TY_INTERFACE_INFO &infInfo = devInfo->Interface();
        //deviceinfos support store interface by interface
        if(ifaceid != infInfo.id) {
            printInterfaceInfo(infInfo);
            ifaceid = infInfo.id;
        }
        if(TY_INTERFACE_USB == (devInfo->Interface().type)) {
            //Usb Device need to open device to get the real Deviceinfo
            //Here will Cause an open-close operation as the result of
            //getDevice released immediately
            auto dev = devList->getDevice(i);
            //If open Failed, returned dev will be nullptr
            if (nullptr != dev.get()) {
                devInfo = dev->getDeviceInfo();
            }
        }
        printDeviceInfo(devInfo);
    }

    return 0;
}
