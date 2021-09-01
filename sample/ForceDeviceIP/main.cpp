#include "../common/common.hpp"

void Usage() 
{
    printf(
    "Usage: ForceDeviceIP <cmd> <MAC> <newIP> <newNetmask> <newGateway>\n"
    "    - <cmd> should be -force/-static/-dynamic\n"
    "    Example: ForceDeviceIP -force 12:34:56:78:90:ab 192.168.1.100 255.255.255.0 192.168.1.1\n"
    "\n"
    "    1. Force the device IP temporarily.\n"
    "       - The newIP/newNetmask will be valid only before reboot.\n"
    "       Command: ForceDeviceIP -force <MAC> <newIP> <newNetmask> <newGateway>\n"
    "\n"
    "    2. Force the device IP persistently.\n"
    "       - The newIP/newNetmask will be fixed and always vaild.\n"
    "       Command: ForceDeviceIP -static <MAC> <newIP> <newNetmask> <newGateway>\n"
    "\n"
    "    3. Set the device IP to dynamic.\n"
    "       - The simplest way, which is more convenient than usage 4.\n"
    "       - Not supported by some old devices. Try usage 4 in this case.\n"
    "       - Take effect immediately. The device will begin to get new IP from DHCP server or Link-local.\n"
    "       Command: ForceDeviceIP -dynamic <MAC>\n"
    "\n"
    "    4. Set the device IP to dynamic, for legacy devices.\n"
    "       - Force the newIP to the device first.\n"
    "       - Make sure the tool can access the device properly after newIP is forced.\n"
    "       - Take effect after reboot.\n"
    "       Command: ForceDeviceIP -dynamic <MAC> <newIP> <newNetmask> <newGateway>\n"
    );
}

int main(int argc, char* argv[])
{
    char * cmd = NULL;
    char * mac = NULL;
    const char * newIP = "0.0.0.0";
    const char * newNetmask = "0.0.0.0";
    const char * newGateway = "0.0.0.0";

    if (argc < 3) {
      Usage();
      return -1;
    }
    cmd = argv[1];
    mac = argv[2];
    
    if (strcmp(cmd, "-force") != 0
      && strcmp(cmd, "-static") != 0
      && strcmp(cmd, "-dynamic") != 0) {
      Usage();
      return -1;
    }
    if (argc >= 4) {
      newIP      = argv[3];
    }
    if (argc >= 5) {
      newNetmask = argv[4];
    }
    if (argc >= 6) {
      newGateway = argv[5];
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
        if (TYForceDeviceIP(hIface, mac, newIP, newNetmask, newGateway) == TY_STATUS_OK) {
            LOGD("**** Set Temporary IP/Netmask/Gateway ...Done! ****");

            bool   open_needed  = false;
            const char * ip_save      = newIP;
            const char * netmask_save = newNetmask;
            const char * gateway_save = newGateway;
            if(strcmp(cmd, "-static") == 0) {
              open_needed = true;
            }
            if(strcmp(cmd, "-dynamic") == 0 && strcmp(newIP, "0.0.0.0") != 0){
              open_needed  = true;
              ip_save      = "0.0.0.0";
              netmask_save = "0.0.0.0";
              gateway_save = "0.0.0.0";
            } 
            if(open_needed){
              TYUpdateDeviceList(hIface);
              TY_DEV_HANDLE hDev;
              if(TYOpenDeviceWithIP(hIface, newIP, &hDev) == TY_STATUS_OK){
                int32_t ip_i[4];
                uint8_t ip_b[4];
                int32_t ip32;
                sscanf(ip_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent IP 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_IP, ip32) );
                sscanf(netmask_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent Netmask 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_SUBMASK, ip32) );
                sscanf(gateway_save, "%d.%d.%d.%d", &ip_i[0], &ip_i[1], &ip_i[2], &ip_i[3]);
                ip_b[0] = ip_i[0];ip_b[1] = ip_i[1];ip_b[2] = ip_i[2];ip_b[3] = ip_i[3];
                ip32 = TYIPv4ToInt(ip_b);
                LOGI("Set persistent Gateway 0x%x(%d.%d.%d.%d)", ip32, ip_b[0], ip_b[1], ip_b[2], ip_b[3]);
                ASSERT_OK( TYSetInt(hDev, TY_COMPONENT_DEVICE, TY_INT_PERSISTENT_GATEWAY, ip32) );

                LOGD("**** Set Persistent IP/Netmask/Gateway ...Done! ****");
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

