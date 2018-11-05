#include "../common/common.hpp"

int main(int argc, char* argv[])
{
    if (argc < 5) {
        LOGI("Usage: ForceDeviceIP <MAC> <newIP> <newNetmask> <newGateway> [-set | -clear]");
        return -1;
    }
    const char * MAC        = argv[1];
    const char * newIP      = argv[2];
    const char * newNetmask = argv[3];
    const char * newGateway = argv[4];
    bool forceSet = false;
    bool forceClear = false;
    for(int i = 5; i < argc; i++){
      if(argv[i] == std::string("-set")){
        forceSet = true;
      } else if(argv[i] == std::string("-clear")){
        forceClear = true;
      }
    }

    LOGD("Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

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
        
        TY_INTERFACE_HANDLE hIface;
        ASSERT_OK( TYOpenInterface(ifaces[i].id, &hIface) );
        if (TYForceDeviceIP(hIface, MAC, newIP, newNetmask, newGateway) == TY_STATUS_OK) {
            LOGD("**** Force device IP/Netmask/Gateway ...Done! ****");

            if(forceSet || forceClear){
              TYUpdateDeviceList(hIface);
              TY_DEV_HANDLE hDev;
              if(TYOpenDeviceWithIP(hIface, newIP, &hDev) == TY_STATUS_OK){
                if(forceClear){
                  newIP = "0.0.0.0";
                  newNetmask = "0.0.0.0";
                  newGateway = "0.0.0.0";
                }
                int32_t ip_i[4];
                uint8_t ip_b[4];
                int32_t ip32;
                sscanf(newIP, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent IP 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_IP, ip32) );
                sscanf(newNetmask, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent Netmask 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_SUBMASK, ip32) );
                sscanf(newGateway, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent Gateway 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_GATEWAY, ip32) );
              }
            }

        } else {
            LOGW("Force ip failed on interface %s", ifaces[i].id);
        }
        ASSERT_OK( TYCloseInterface(hIface));        
      }
    }
    ASSERT_OK(TYDeinitLib());
    return 0;
}
