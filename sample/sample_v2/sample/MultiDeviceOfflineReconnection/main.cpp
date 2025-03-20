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

        void registerFrameParser(TYFrameParser* _parser) { parser = _parser; }
        void Display();
        bool is_offline = false;
        
    private:
        std::string id;
        CamInitCallback     parameters_init = nullptr;

        TYFrameParser*    parser;
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


static bool process_exit = false;
void OfflineDetectCamera::Display()
{
    std::string win = "depth_" + id;;
    parser->setImageProcesser(TY_COMPONENT_DEPTH_CAM, std::shared_ptr<ImageProcesser>(new ImageProcesser(win.c_str())));
    win = "color_"+ id;
    parser->setImageProcesser(TY_COMPONENT_RGB_CAM, std::shared_ptr<ImageProcesser>(new ImageProcesser(win.c_str())));
    while(!process_exit) {
        auto frame = tryGetFrames(2000);
        if(frame) {
            parser->update(frame);
        }
    }
}

int main(int argc, char* argv[])
{
    std::vector<std::string> list;
    for(int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-list") == 0) {
            if (argc == i+1) {
                LOGI("===== no list input");
                return 0;
            }
            int32_t j;
            for (j = 0;j < argc - (i + 1); j++) {
                list.push_back(argv[i+1+j]);
                LOGI("===== Select device %s", list[j].c_str());
            }
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-list <sn1 sn2 sn3>]" << std::endl;
            return 0;
        }
    }

    if(!list.size()) {
        std::cout << "no device select!" << std::endl;
        return 0;
    }
    
    std::vector<TYFrameParser> FrameParsers(list.size());
    std::vector<OfflineDetectCamera> cams(list.size());

    for(size_t i = 0; i < list.size(); i++) {
        FrameParsers[i].RegisterKeyBoardEventCallback([](int key, void* data) {
            if(key == 'q' || key == 'Q') {
                *(bool*)data = true;
                std::cout << "Exit..." << std::endl; 
            }
        }, &process_exit);

        if(TY_STATUS_OK != cams[i].open(list[i].c_str())) {
            std::cout << "open camera failed!" << std::endl;
            return -1;
        }

        cams[i].registerFrameParser(&FrameParsers[i]);
        cams[i].RegisterDeviceInitCallback([](OfflineDetectCamera* cam) {
            //Device init code
            //The initialization Settings of the camera are written here.
            cam->stream_enable(FastCamera::stream_depth);
            cam->stream_enable(FastCamera::stream_color);

        },  &cams[i]);
    }

    for(int i = 0; i < cams.size(); i++) {
        cams[i].start();
    }

    std::vector<std::thread>  fetchThread(cams.size());
    for(size_t i = 0; i < fetchThread.size(); i++) {
        fetchThread[i] = std::thread(&OfflineDetectCamera::Display, &cams[i]);
    }

    for(size_t i = 0; i < fetchThread.size(); i++) {
        fetchThread[i].join();
    }

    std::cout << "Main done!" << std::endl;
    return 0;
}
