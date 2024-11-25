#include "Device.hpp"

using namespace percipio_layer;


class ResolutionSettingCamera : public FastCamera
{
    public:
        ResolutionSettingCamera() : FastCamera() {};
        ~ResolutionSettingCamera() {}; 
        //Image mode not setted, select image mode with an interactive method
        TY_STATUS ResolutionSetting(TY_COMPONENT_ID comp);
        //use a image mode generate with TYImageMode2 API
        TY_STATUS ResolutionSetting(TY_COMPONENT_ID comp, TY_IMAGE_MODE image_mode);
};


TY_STATUS ResolutionSettingCamera::ResolutionSetting(TY_COMPONENT_ID comp)
{
    TY_STATUS ret = TY_STATUS_OK;
    TY_DEV_HANDLE _handle = handle();
    if(!_handle) {
        std::cout << "Invalid camera handle!" << std::endl;
        return TY_STATUS_INVALID_HANDLE;
    }
    std::vector<TY_ENUM_ENTRY> image_mode_list;
    ASSERT_OK(get_feature_enum_list(_handle, comp, TY_ENUM_IMAGE_MODE, image_mode_list));
    if(image_mode_list.size()) {
        std::cout << "select an image mode: " << std::endl;
        for(uint32_t i = 0; i < image_mode_list.size(); i++) {
            std::cout << "\t" << i << ": " << image_mode_list[i].description << std::endl;
        }

        while(1) {
            uint32_t i;
            char ch;
            while ((ch = getchar()) != '\n') {
                i = ch - '0';
            }

            if((i >= 0) && (i < image_mode_list.size())) {
                std::cout << "select index : " << i << std::endl;
                auto profile = image_mode_list[i];
                ret = ResolutionSetting(comp, profile.value);
                break;
            } else {
                std::cout << "invalid index" << std::endl;
            }
        }
    }
    return ret;
}

TY_STATUS ResolutionSettingCamera::ResolutionSetting(TY_COMPONENT_ID comp, TY_IMAGE_MODE image_mode)
{
    TY_DEV_HANDLE _handle = handle();
    if(!_handle) {
        std::cout << "Invalid camera handle!" << std::endl;
        return TY_STATUS_INVALID_HANDLE;
    }
    std::cout << "set comp 0x "<< std::hex << comp << " image mode: "<< std::hex << image_mode << std::endl;
    TY_PIXEL_FORMAT pix = TYPixelFormat(image_mode);
    int32_t w = TYImageWidth(image_mode);
    int32_t h = TYImageHeight(image_mode);
    std::cout << "pixel format: 0x"<< std::hex << pix << std::endl;
    std::cout << "reso:"<< std::dec << w << "X" << h << std::endl;
    TY_STATUS ret = TYSetEnum(_handle, comp, TY_ENUM_IMAGE_MODE, image_mode);
    if(ret != TY_STATUS_OK) {
        std::cout << "TYSetEnum failed with code:(" << ret << ")" << TYErrorString(ret)<< std::endl;
    }
    return ret;
}

int main(int argc, char* argv[])
{
    std::string ID;
    int w = 0, h = 0;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-width") == 0) {
            w = atoi(argv[++i]);
        } else if(strcmp(argv[i], "-height") == 0) {
            h = atoi(argv[++i]);
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>] [-width w] [-height h]" << std::endl;
            return 0;
        }
    }

    ResolutionSettingCamera camera;
    TY_STATUS status = camera.open(ID.c_str());
    if(status != TY_STATUS_OK) {
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

    if(camera.has_stream(FastCamera::stream_depth) && (w*h) != 0) {
        //Use TYImageMode2 generate image_mode
        //pixel format should modify for some camera, XYZ48 .etc
        TY_IMAGE_MODE image_mode = TYImageMode2(TY_PIXEL_FORMAT_DEPTH16, w, h);
        camera.ResolutionSetting(TY_COMPONENT_DEPTH_CAM, image_mode);
        camera.stream_enable(FastCamera::stream_depth);
    }
    if(camera.has_stream(FastCamera::stream_color)) {
        //select Image mode in an interactive mode
        camera.ResolutionSetting(TY_COMPONENT_RGB_CAM);
        camera.stream_enable(FastCamera::stream_color);
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
