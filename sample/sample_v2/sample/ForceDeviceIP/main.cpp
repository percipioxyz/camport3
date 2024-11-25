#include "Device.hpp"

using namespace percipio_layer;

void Usage(const char* program) 
{
    std::cout << 
    "Usage: " << program << " <cmd> <MAC> <newIP> <newNetmask> <newGateway>\n"
    "    - <cmd> should be -force/-static/-dynamic\n"
    "    Example: " << program << " -force 12:34:56:78:90:ab 192.168.1.100 255.255.255.0 192.168.1.1\n"
    "\n"
    "    1. Force the device IP temporarily.\n"
    "       - The newIP/newNetmask will be valid only before reboot.\n"
    "       Command: " << program << " -force <MAC> <newIP> <newNetmask> <newGateway>\n"
    "\n"
    "    2. Force the device IP persistently.\n"
    "       - The newIP/newNetmask will be fixed and always vaild.\n"
    "       Command: " << program << " -static <MAC> <newIP> <newNetmask> <newGateway>\n"
    "\n"
    "    3. Set the device IP to dynamic.\n"
    "       - The simplest way, which is more convenient than usage 4.\n"
    "       - Not supported by some old devices. Try usage 4 in this case.\n"
    "       - Take effect immediately. The device will begin to get new IP from DHCP server or Link-local.\n"
    "       Command: " << program << " -dynamic <MAC>\n"
    "\n"
    "    4. Set the device IP to dynamic, for legacy devices.\n"
    "       - Force the newIP to the device first.\n"
    "       - Make sure the tool can access the device properly after newIP is forced.\n"
    "       - Take effect after reboot.\n"
    "       Command: " << program << " -dynamic <MAC> <newIP> <newNetmask> <newGateway>\n"
    << std::endl;
}

int main(int argc, char* argv[])
{
    char * cmd = NULL;
    char * mac = NULL;
    const char * newIP = "0.0.0.0";
    const char * newNetmask = "0.0.0.0";
    const char * newGateway = "0.0.0.0";

    if (argc < 3) {
      Usage(argv[0]);
      return -1;
    }
    cmd = argv[1];
    mac = argv[2];

    ForceIPStyle style = ForceIPStyleDynamic;
    if(strcmp(cmd, "-force") == 0)
      style = ForceIPStyleForce;
    else if(strcmp(cmd, "-static") == 0)
      style = ForceIPStyleStatic;
    else if(strcmp(cmd, "-dynamic") == 0)
      style = ForceIPStyleDynamic;
    else {
      Usage(argv[0]);
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

    TYContext& ctx = TYContext::getInstance();
    bool ret = ctx.ForceNetDeviceIP(style, mac, newIP, newNetmask, newGateway);
    std::cout << "***Force ip...DONE (" << (ret ? "succeed" : "failed") << ")***"  << std::endl;
    return 0;
}
