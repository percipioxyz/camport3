#include "Device.hpp"

using namespace percipio_layer;

class OfflineDetectCamera : public FastCamera
{
    public:
        OfflineDetectCamera() : FastCamera() {};
        ~OfflineDetectCamera() {};

        TY_STATUS open(const char* sn);
        
        typedef std::function<void(OfflineDetectCamera* camera)>      CamInitCallback;
        void RegisterDeviceInitCallback(CamInitCallback funcPtr, void* ptr);

        TY_STATUS start();
        std::shared_ptr<TYFrame> tryGetFrames(uint32_t timeout_ms);

        bool is_offline = false;
    private:
        std::string id;
        CamInitCallback     parameters_init = nullptr;
};

TY_STATUS OfflineDetectCamera::open(const char* sn)
{
    TY_STATUS st;
    while(1) {
        st = FastCamera::open(sn);
        if(st == TY_STATUS_OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1500));
    }

    id = sn;
    is_offline = false;
    RegisterOfflineEventCallback([](void* userdata) {
        std::cout << "Device Offline!" << std::endl;
        *(bool*)userdata = true;
    }, &is_offline);

    return TY_STATUS_OK;
}

void OfflineDetectCamera::RegisterDeviceInitCallback(CamInitCallback funcPtr, void* ptr)
{
    parameters_init = funcPtr;
}

TY_STATUS OfflineDetectCamera::start()
{
    if(parameters_init)  
        parameters_init(this);
    return FastCamera::start();
}

std::shared_ptr<TYFrame> OfflineDetectCamera::tryGetFrames(uint32_t timeout_ms)
{
    TY_STATUS status;
    if(is_offline) {
        FastCamera::close();
        //loop device detect
        while(1) {
            status = open(id.c_str());
            if(status == TY_STATUS_OK) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1500));
        }

        is_offline = false;
        status = start();
        if(status != TY_STATUS_OK) {
            std::cout << "camera init failed! " << std::endl;
            return std::shared_ptr<TYFrame>();
        }
    }

    return FastCamera::tryGetFrames(timeout_ms);
}

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

    OfflineDetectCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    camera.RegisterDeviceInitCallback([](OfflineDetectCamera* cam) {
        //Device init code
        //The initialization Settings of the camera are written here.
        cam->stream_enable(FastCamera::stream_depth);
        cam->stream_enable(FastCamera::stream_color);

    },  &camera);
    
    bool process_exit = false;
    TYFrameParser       parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    camera.start();
    while(!process_exit) {
        auto frame = camera.tryGetFrames(2000);
        if(frame) {
            parser.update(frame);
        } else {
        }
    }

    std::cout << "Main done!" << std::endl;
    return 0;
}
