#include "Device.hpp"

using namespace percipio_layer;
class undistortionProcesser: public ImageProcesser {
public:
    undistortionProcesser(TY_CAMERA_CALIB_INFO &calib):ImageProcesser("depth", &calib) {}
    int parse(const std::shared_ptr<TYImage>& image) {
        ImageProcesser::parse(image);
        return ImageProcesser::doUndistortion();
    }
};

class TofCamera : public FastCamera
{
    public:
        TofCamera() : FastCamera() {};
        ~TofCamera() {}; 

        TY_STATUS Init();
        bool needUndistortion() {return depth_needUndistort;}
        TY_CAMERA_CALIB_INFO &getCalib() { return depth_calib;} 
    private:
        float f_depth_scale_unit = 1.f;
        bool depth_needUndistort = false;
        TY_CAMERA_CALIB_INFO depth_calib;
};

TY_STATUS TofCamera::Init()
{
    TY_STATUS status;
    status = stream_enable(stream_depth);
    if(status != TY_STATUS_OK) return status;
    
    TYGetFloat(handle(), TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &f_depth_scale_unit);
    TYHasFeature(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &depth_needUndistort);
    TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &depth_calib, sizeof(depth_calib));
    return TY_STATUS_OK;
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

    TofCamera tof;
    if(TY_STATUS_OK != tof.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;
    tof.Init();
    TYFrameParser parser;
    if (tof.needUndistortion()) {
        std::shared_ptr<ImageProcesser> depth_process = std::shared_ptr<ImageProcesser>(new undistortionProcesser(tof.getCalib()));
        parser.setImageProcesser(TY_COMPONENT_DEPTH_CAM, depth_process);
    }
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != tof.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }


    while(!process_exit) {
        auto frame = tof.tryGetFrames(2000);
        if(frame) {
            parser.update(frame);
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
