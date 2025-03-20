#include "Device.hpp"
#include "TYCoordinateMapper.h"

#define MAP_DEPTH_TO_COLOR  1

using namespace percipio_layer;

class RegistrationParser: public TYFrameParser {
public:
    int setCalibInfo(TY_CAMERA_CALIB_INFO &dep, TY_CAMERA_CALIB_INFO &rgb)
    {
        memcpy(&depth_calib, &dep, sizeof(dep));
        memcpy(&color_calib, &rgb, sizeof(dep));
        return 0;
    }
    int doProcess(const std::shared_ptr<TYFrame>& img)
    {
        auto depth = img->depthImage();
        auto color = img->colorImage();
        auto left_ir = img->leftIRImage();
        auto right_ir = img->rightIRImage();

        if (left_ir) {
            stream[TY_COMPONENT_IR_CAM_LEFT]->parse(left_ir);
        }

        if (right_ir) {
            stream[TY_COMPONENT_IR_CAM_RIGHT]->parse(right_ir);
        }
        
        if (color) {
            stream[TY_COMPONENT_RGB_CAM]->parse(color);
            if (color_needUndistort) {
                stream[TY_COMPONENT_RGB_CAM]->doUndistortion();
            }
        }

        if (depth) {
            stream[TY_COMPONENT_DEPTH_CAM]->parse(depth);
            if (depth_needUndistort) {
                stream[TY_COMPONENT_DEPTH_CAM]->doUndistortion();
            }
        }
        if(color && depth) {
            doRegistration(depth, color);
        }
        return 0;
    }
    virtual int doRegistration(const std::shared_ptr<TYImage>& dep, const std::shared_ptr<TYImage>& rgb) = 0;

    TY_CAMERA_CALIB_INFO depth_calib, color_calib;
    float f_depth_scale_unit = 1.f;
    bool depth_needUndistort = false;
    bool color_needUndistort = false;
};

class RGB2DepParser: public RegistrationParser {
public:
    int doRegistration(const std::shared_ptr<TYImage>& depth, const std::shared_ptr<TYImage>& color)
    {
        TY_PIXEL_FORMAT color_fmt = color->pixelFormat();
        std::shared_ptr<TYImage> dst;
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
        stream[TY_COMPONENT_RGB_CAM]->parse(dst);
        return 0;
    }
};

class Dep2RGBParser: public RegistrationParser {
public:
    int doRegistration(const std::shared_ptr<TYImage>& depth, const std::shared_ptr<TYImage>& color)
    {
        int dstW = depth->width();
        int dstH = depth->width() * color->height() / color->width();
        std::shared_ptr<TYImage> dst;
        dst = std::shared_ptr<TYImage>(new TYImage(dstW, dstH, depth->componentID(), TY_PIXEL_FORMAT_DEPTH16, sizeof(uint16_t) * dstW * dstH));
        TYMapDepthImageToColorCoordinate(
                  &depth_calib,
                  depth->width(), depth->height(), static_cast<const uint16_t*>(depth->buffer()),
                  &color_calib,
                  dstW, dstH, static_cast<uint16_t*>(dst->buffer()),
                  f_depth_scale_unit);
        dst->resize(color->width(), color->height());
        stream[TY_COMPONENT_DEPTH_CAM]->parse(dst);
        return 0;
    }
};

class RGBDRegistrationCamera : public FastCamera
{
    public:
        RGBDRegistrationCamera() : FastCamera() {};
        ~RGBDRegistrationCamera() {}; 

        TY_STATUS StreamInit(RegistrationParser *parser,  bool depth_to_color);
};

TY_STATUS RGBDRegistrationCamera::StreamInit(RegistrationParser *p, bool depth_to_color)
{
    TY_STATUS status;
    status = stream_enable(stream_color);
    if(status != TY_STATUS_OK) return status;

    status = stream_enable(stream_depth);
    if(status != TY_STATUS_OK) return status;
    
    TYGetFloat(handle(), TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &p->f_depth_scale_unit);
    
    TYHasFeature(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &p->depth_needUndistort);
    TYHasFeature(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_DISTORTION, &p->color_needUndistort);

    TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &p->depth_calib, sizeof(p->depth_calib));
    TYGetStruct(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &p->color_calib, sizeof(p->color_calib));
    std::string dep_win_name = "remap_depth";
    std::string rgb_win_name = "undistortion_RGB";
    if (!depth_to_color) { 
        dep_win_name = "depth";
        rgb_win_name = "remap_color";
    } 
    p->setImageProcesser(TY_COMPONENT_DEPTH_CAM,
        std::shared_ptr<ImageProcesser>(new ImageProcesser(dep_win_name.c_str(), &p->depth_calib)));
    p->setImageProcesser(TY_COMPONENT_RGB_CAM,
        std::shared_ptr<ImageProcesser>(new ImageProcesser(rgb_win_name.c_str(), &p->color_calib)));
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

    RGBDRegistrationCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;

    RegistrationParser *parser = nullptr;
    if (MAP_DEPTH_TO_COLOR) {
        parser = new Dep2RGBParser();
    } else {
        parser = new RGB2DepParser();
    }
    parser->RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    if(TY_STATUS_OK != camera.StreamInit(parser, MAP_DEPTH_TO_COLOR)) {
        std::cout << "Camera stream init failed!" << std::endl;
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
            parser->update(frame);
        }
    }
    delete parser;
    parser = nullptr;
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
