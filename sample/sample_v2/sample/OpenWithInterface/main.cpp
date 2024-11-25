#include "Device.hpp"

using namespace percipio_layer;

int main(int argc, char* argv[])
{
    std::string ID;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }

    TYCamInterface interfaces;

    std::vector<std::string>  ifaces;
    interfaces.List(ifaces);
    std::cout << "Select the interface which the camera is located:" << std::endl;
    for(uint32_t i = 0; i < ifaces.size(); i++) {
        std::cout << "\t" << i << ": " << parseInterfaceID(ifaces[i]) << std::endl;
    }

    std::string inf;
     while(1) {
        uint32_t i;
        char ch;
        while ((ch = getchar()) != '\n') {
            i = ch - '0';
        }

        if((i >= 0) && (i < ifaces.size())) {
            std::cout << "select index : " << i << std::endl;
            inf = ifaces[i];
            break;
        } else {
            std::cout << "invalid index" << std::endl;
        }
    }

    FastCamera camera;
    //Call setIfaceId before open/openByIP
    camera.setIfaceId(inf.c_str());
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;

    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != camera.stream_enable(FastCamera::stream_depth)) {
        std::cout << "depth stream enable failed!" << std::endl;
        return -1;
    }

    if(TY_STATUS_OK != camera.stream_enable(FastCamera::stream_color)) {
        std::cout << "color stream enable failed!" << std::endl;
        return -1;
    }

    std::cout << "stream start!" << std::endl;
    if(TY_STATUS_OK != camera.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }
    
    while(!process_exit) {
        auto frame = camera.tryGetFrames(2000);
        if(frame) parser.update(frame);
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
