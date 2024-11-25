#include "Device.hpp"

using namespace percipio_layer;

class TofCamera : public FastCamera
{
    public:
        TofCamera() : FastCamera() {};
        ~TofCamera() {}; 

        TY_STATUS Init();
        void process(TYFrameParser& parser, const std::shared_ptr<TYImage>&  depth);

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

void TofCamera::process(TYFrameParser& parser, const std::shared_ptr<TYImage>&  depth)
{
    std::shared_ptr<ImageProcesser> depth_process = std::shared_ptr<ImageProcesser>(new ImageProcesser("depth", depth, &depth_calib));
    if(depth_needUndistort)
        depth_process->doUndistortion();
    parser.update(depth_process);
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
    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    tof.Init();
    if(TY_STATUS_OK != tof.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }

    while(!process_exit) {
        auto frame = tof.tryGetFrames(2000);
        if(frame) {
            auto depth = frame->depthImage();
            if(depth) tof.process(parser, depth);
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
