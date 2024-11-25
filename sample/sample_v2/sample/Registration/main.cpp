#include "Device.hpp"
#include "TYCoordinateMapper.h"

#define MAP_DEPTH_TO_COLOR  1

using namespace percipio_layer;

class RGBDRegistrationCamera : public FastCamera
{
    public:
        RGBDRegistrationCamera() : FastCamera() {};
        ~RGBDRegistrationCamera() {}; 

        TY_STATUS StreamInit();
        int StreamRegistreation(TYFrameParser& parser, std::shared_ptr<TYImage>& depth, std::shared_ptr<TYImage>& color, bool map_depth_to_color, std::shared_ptr<TYImage>& dst);

    private:
        float f_depth_scale_unit = 1.f;
        bool depth_needUndistort = false;
        bool color_needUndistort = false;
        TY_CAMERA_CALIB_INFO depth_calib, color_calib;
};

TY_STATUS RGBDRegistrationCamera::StreamInit()
{
    TY_STATUS status;
    status = stream_enable(stream_color);
    if(status != TY_STATUS_OK) return status;

    status = stream_enable(stream_depth);
    if(status != TY_STATUS_OK) return status;
    
    TYGetFloat(handle(), TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &f_depth_scale_unit);
    
    TYHasFeature(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &depth_needUndistort);
    TYHasFeature(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_DISTORTION, &color_needUndistort);

    TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &depth_calib, sizeof(depth_calib));
    TYGetStruct(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &color_calib, sizeof(color_calib));

    return TY_STATUS_OK;
}

int RGBDRegistrationCamera::StreamRegistreation(TYFrameParser& parser, std::shared_ptr<TYImage>& depth, std::shared_ptr<TYImage>& color, bool map_depth_to_color, std::shared_ptr<TYImage>& dst)
{
    std::shared_ptr<ImageProcesser> depth_process = std::shared_ptr<ImageProcesser>(new ImageProcesser("depth", depth, &depth_calib));
    std::shared_ptr<ImageProcesser> color_process = std::shared_ptr<ImageProcesser>(new ImageProcesser("undistortion RGB", color, &color_calib));

    if(depth_needUndistort) depth_process->doUndistortion();
    if(color_needUndistort) color_process->doUndistortion();

    depth = depth_process->image();
    color = color_process->image();

    if(map_depth_to_color) {
        dst = std::shared_ptr<TYImage>(new TYImage(color->width(), color->height(), depth->componentID(), TY_PIXEL_FORMAT_DEPTH16, sizeof(uint16_t) * color->width() * color->height()));
        TYMapDepthImageToColorCoordinate(
                  &depth_calib,
                  depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                  &color_calib,
                  color->width(), color->height(), static_cast<uint16_t*>(dst->buffer()),
                  f_depth_scale_unit);

        parser.update(color_process);

        std::shared_ptr<ImageProcesser> remap_depth_process = std::shared_ptr<ImageProcesser>(new ImageProcesser("remap_depth", dst, &color_calib));
        parser.update(remap_depth_process);
    } else {
        TY_PIXEL_FORMAT color_fmt = color->pixelFormat();
        switch(color_fmt)
        {
            case TY_PIXEL_FORMAT_RGB:
            case TY_PIXEL_FORMAT_BGR:
                dst = std::shared_ptr<TYImage>(new TYImage(depth->width(), depth->height(), color->componentID(), color_fmt, 3 * depth->width() * depth->height()));
                TYMapRGBImageToDepthCoordinate(
                    &depth_calib,
                    depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                    &color_calib,
                    color->width(), color->height(), static_cast<const uint8_t*>(color->buffer()),
                    static_cast<uint8_t*>(dst->buffer()),
                    f_depth_scale_unit);
                break;
            case TY_PIXEL_FORMAT_RGB48:
                dst = std::shared_ptr<TYImage>(new TYImage(depth->width(), depth->height(), color->componentID(), color_fmt, 6 * depth->width() * depth->height()));
                TYMapRGB48ImageToDepthCoordinate(
                    &depth_calib,
                    depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                    &color_calib,
                    color->width(), color->height(), static_cast<const uint16_t*>(color->buffer()),
                    static_cast<uint16_t*>(dst->buffer()),
                    f_depth_scale_unit);
                break;
            case TY_PIXEL_FORMAT_MONO:
                dst = std::shared_ptr<TYImage>(new TYImage(depth->width(), depth->height(), color->componentID(), color_fmt, depth->width() * depth->height()));
                TYMapMono8ImageToDepthCoordinate(
                    &depth_calib,
                    depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                    &color_calib,
                    color->width(), color->height(), static_cast<const uint8_t*>(color->buffer()),
                    static_cast<uint8_t*>(dst->buffer()),
                    f_depth_scale_unit);
                break;
            case TY_PIXEL_FORMAT_MONO16:
                dst = std::shared_ptr<TYImage>(new TYImage(depth->width(), depth->height(), color->componentID(), color_fmt, 2 * depth->width() * depth->height()));
                TYMapMono16ImageToDepthCoordinate(
                    &depth_calib,
                    depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                    &color_calib,
                    color->width(), color->height(), static_cast<const uint16_t*>(color->buffer()),
                    static_cast<uint16_t*>(dst->buffer()),
                    f_depth_scale_unit);
                break;
            default:
                break;
        }

        parser.update(depth_process);

        std::shared_ptr<ImageProcesser> remap_color_process = std::shared_ptr<ImageProcesser>(new ImageProcesser("remap_color", dst, &depth_calib));
        parser.update(remap_color_process);
    }
    return 0;
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

    RGBDRegistrationCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    if(TY_STATUS_OK != camera.StreamInit()) {
        std::cout << "Camera stream init failed!" << std::endl;
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

    if(TY_STATUS_OK != camera.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }
    
    std::cout << "stream start" << std::endl;

    while(!process_exit) {
        auto frame = camera.tryGetFrames(2000);
        if(frame) {
            auto color = frame->colorImage();
            auto depth = frame->depthImage();
            if(color && depth) {
                std::shared_ptr<TYImage> remap_image;
                if(camera.StreamRegistreation(parser, depth, color, MAP_DEPTH_TO_COLOR, remap_image) < 0) {
                    process_exit = true;
                }
            }
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}