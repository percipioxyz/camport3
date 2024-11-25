#include "Device.hpp"

using namespace percipio_layer;

class ExposureTimeSettingCamera : public FastCamera
{
    public:
        ExposureTimeSettingCamera() : FastCamera() {};
        ~ExposureTimeSettingCamera() {}; 

        TY_STATUS SetRGBExposureTime(int exposure);
};

TY_STATUS ExposureTimeSettingCamera::SetRGBExposureTime(int exposure)
{
    TY_DEV_HANDLE _handle = handle();
    if(!_handle) {
        std::cout << "Invalid camera handle!" << std::endl;
        return TY_STATUS_INVALID_HANDLE;
    }

    bool hasAutoExp;
    TY_STATUS ret = TYHasFeature(_handle, TY_COMPONENT_RGB_CAM, TY_BOOL_AUTO_EXPOSURE, &hasAutoExp);
    if(ret == TY_STATUS_OK) {
        if(hasAutoExp) {
            bool auto_exp = false;
            TYGetBool(_handle, TY_COMPONENT_RGB_CAM, TY_BOOL_AUTO_EXPOSURE, &auto_exp);
            if(auto_exp) {
                std::cout << "Turn off camera AEC." << std::endl;
                TYSetBool(_handle, TY_COMPONENT_RGB_CAM, TY_BOOL_AUTO_EXPOSURE, false);
            }
        }
    }

    bool hasExp = false;
    ret = TYHasFeature(_handle, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, &hasExp);
    if(ret!= TY_STATUS_OK || !hasExp) {
        std::cout << "The camera does not support exposure time Settings" << std::endl;
        return TY_STATUS_INVALID_FEATURE;
    }

    TY_INT_RANGE exp_range;
    ret = TYGetIntRange(_handle, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, &exp_range);
    if(ret != TY_STATUS_OK) {
        std::cout << "Get camera rgb exposure time range err!" << std::endl;
        return ret;
    }

    if((exposure < exp_range.min || exposure > exp_range.max)) {
        std::cout << "Exposure time(" << exposure << ") is out of range:(" << exp_range.min << ", " << exp_range.max << "), " << std::endl;
        return TY_STATUS_OUT_OF_RANGE;
    }

    return TYSetInt(_handle, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, exposure);
}

int main(int argc, char* argv[])
{
    std::string ID;
    int val = -1;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        }  else if(strcmp(argv[i], "-exp") == 0) {
            val = atoi(argv[++i]);
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]  [-exp <exposure>]" << std::endl;
            return 0;
        }
    }
    
    ExposureTimeSettingCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    TY_STATUS status = camera.SetRGBExposureTime(val);
    std::cout << "set rgb exposure time : " << val << ",  ret = " << status << std::endl;
    
    bool process_exit = false;
    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != camera.stream_enable(FastCamera::stream_color)) {
        std::cout << "color stream enable failed!" << std::endl;
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
