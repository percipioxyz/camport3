#include "../common/common.hpp"
#include <fstream>

int main(int argc, char* argv[])
{
    bool save = false; 
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-s") == 0){
            save = true;
        }
    }

    // Init lib
    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK(TYLibVersion(&ver));
    LOGD("=== lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    LOGD("Update interface list");
    ASSERT_OK(TYUpdateInterfaceList());

    uint32_t n = 0;
    ASSERT_OK(TYGetInterfaceNumber(&n));
    LOGD("Got %u interfaces", n);
    if (n == 0) {
        LOGE("interface number incorrect");
        return TY_STATUS_ERROR;
    }

    std::string device_list = "device_list";
    std::ofstream* output;
    if (save){
        output = new std::ofstream(device_list.data());
    }

    std::vector<TY_INTERFACE_INFO> ifaces(n);
    ASSERT_OK(TYGetInterfaceList(&ifaces[0], n, &n));
    ASSERT(n == ifaces.size());
    std::vector<TY_INTERFACE_HANDLE> hIfaces;
    for (uint32_t i = 0; i < n; i++) {
        TY_INTERFACE_HANDLE hIface;
        ASSERT_OK(TYOpenInterface(ifaces[i].id, &hIface));
        hIfaces.push_back(hIface);
    }
    updateDevicesParallel(hIfaces);
    for (uint32_t i = 0; i < n; i++) {
        if (save) {
            *output << "Interface: " << i <<std::endl;
        }

        LOGI("Found interface %u:", i);
        LOGI("    name: %s", ifaces[i].name);
        LOGI("    id:   %s", ifaces[i].id);
        LOGI("    type: 0x%x", ifaces[i].type);
        if (TYIsNetworkInterface(ifaces[i].type)) {
            LOGI("    MAC: %s", ifaces[i].netInfo.mac);
            LOGI("    ip: %s", ifaces[i].netInfo.ip);
            LOGI("    netmask: %s", ifaces[i].netInfo.netmask);
            LOGI("    gateway: %s", ifaces[i].netInfo.gateway);
            LOGI("    broadcast: %s", ifaces[i].netInfo.broadcast);
        }
        TY_INTERFACE_HANDLE hIface = hIfaces[i];

        uint32_t n = 0;
        TYGetDeviceNumber(hIface, &n);
        if (n == 0) continue;

        std::vector<TY_DEVICE_BASE_INFO> devs(n);
        TYGetDeviceList(hIface, &devs[0], n, &n);
        ASSERT(n == devs.size());
        for (uint32_t j = 0; j < n; j++) {
            if (TYIsNetworkInterface(devs[j].iface.type)) {
                LOGD("    - device %s:", devs[j].id);
                if (strlen(devs[j].userDefinedName) != 0) {
                    LOGD("          vendor     : %s", devs[j].userDefinedName);
                } else {
                    LOGD("          vendor     : %s", devs[j].vendorName);
                }
                LOGD("          model      : %s", devs[j].modelName);

                LOGD("          device MAC : %s", devs[j].netInfo.mac);
                LOGD("          device IP  : %s", devs[j].netInfo.ip);

                if (save) {
                    *output << devs[j].id <<" ";
                    *output << devs[j].netInfo.ip <<" "<<std::endl;
                }
            } else {
                TY_DEV_HANDLE handle;
                int32_t ret = TYOpenDevice(hIface, devs[j].id, &handle);
                if (ret == 0) {
                    TYGetDeviceInfo(handle, &devs[j]);
                    TYCloseDevice(handle);
                    LOGD("    - device %s:", devs[j].id);
                } else {
                    LOGD("    - device %s(open failed, error: %d)", devs[j].id, ret);
                }
                if (strlen(devs[j].userDefinedName) != 0) {
                    LOGD("          vendor     : %s", devs[j].userDefinedName);
                } else {
                    LOGD("          vendor     : %s", devs[j].vendorName);
                }
                LOGD("          model      : %s", devs[j].modelName);

                if (save) {
                    *output << devs[j].id <<" "<<std::endl;
                }
            }
        }
        TYCloseInterface(hIface);
        if (save) {
            *output << " " <<std::endl;
        }
    }
    ASSERT_OK(TYDeinitLib());
    LOGD("Done!");
    return 0;
}
