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

    FastCamera camera;
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
