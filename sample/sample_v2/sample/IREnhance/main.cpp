#include "IREnhance.hpp"
int main(int argc, char* argv[])
{
    std::string ID;
    std::vector<std::shared_ptr<IREnhanceProcesser>> enhancers;
    GetAllEnhancers(enhancers);
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }
    std::shared_ptr<IREnhanceProcesser> enhancer = nullptr;
    std::cout << "select an Enahancer: " << std::endl;
    for(uint32_t i = 0; i < enhancers.size(); i++) {
        std::cout << "\t" << i << ": " << enhancers[i]->name << std::endl;
        std::cout << "\t\t func:" << enhancers[i]->func_desc << std::endl;
    }
    while(1) {
        uint32_t i;
        char ch;
        while ((ch = getchar()) != '\n') {
            i = ch - '0';
        }

        if((i >= 0) && (i < enhancers.size())) {
            std::cout << "select index : " << i << std::endl;
            enhancer = enhancers[i];
            break;
        } else {
            std::cout << "invalid index" << std::endl;
        }
    }

    FastCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;
    TYFrameParser parser;
    parser.setImageProcesser(TY_COMPONENT_IR_CAM_LEFT, std::shared_ptr<ImageProcesser>(enhancer)); 
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != camera.stream_enable(FastCamera::stream_ir_left)) {
        std::cout << "ir left stream enable failed!" << std::endl;
        return -1;
    }
    if(TY_STATUS_OK != camera.stream_enable(FastCamera::stream_depth)) {
        std::cout << "depth stream enable failed!" << std::endl;
        return -1;
    }

    if(TY_STATUS_OK != camera.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }
    
    std::cout << "stream start" << std::endl;

    while(!process_exit) {
        auto frame = camera.tryGetFrames(2000);
        if(frame) {
            parser.update(frame);
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
