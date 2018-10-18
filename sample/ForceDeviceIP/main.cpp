#include "../common/common.hpp"

int main(int argc, char* argv[])
{
    if (argc < 5) {
        LOGI("Usage: ForceDeviceIP <MAC> <newIP> <newNetmask> <newGateway>");
        return -1;
    }  
    const char * MAC        = argv[1];
    const char * newIP      = argv[2];
    const char * newNetmask = argv[3];
    const char * newGateway = argv[4];

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
        }
        ASSERT_OK( TYCloseInterface(hIface));        
      }
    }
    ASSERT_OK(TYDeinitLib());
    return 0;
}
