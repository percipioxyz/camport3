#include <cmath>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "Device.hpp"
#include "TYImageProc.h"

#if _WIN32
#include <conio.h>
#elif __linux__
#include <termio.h>
#define TTY_PATH    "/dev/tty"
#define STTY_DEF    "stty -raw echo -F"
#endif

using namespace percipio_layer;

static char GetChar(void)
{
    char ch = -1;
#if _WIN32
    if (kbhit()) //check fifo
    {
        ch = getche(); //read fifo
    }
#elif __linux__
    fd_set rfds;
    struct timeval tv;
    system(STTY_DEF TTY_PATH);
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 10;
    if (select(1, &rfds, NULL, NULL, &tv) > 0)
    {
        ch = getchar();
    }
#endif
    return ch;
}

class P3DCamera : public FastCamera
{
    public:
        P3DCamera() : FastCamera() {};
        ~P3DCamera() {}; 

        TY_STATUS Init();
        int process(const std::shared_ptr<TYImage>&  depth);
        int process(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color);

    private:
        float f_depth_scale_unit = 1.f;
        bool depth_needUndistort = false;
        bool b_points_with_color = false;
        TY_CAMERA_CALIB_INFO depth_calib, color_calib;
        void savePointsToPly(const std::vector<TY_VECT_3F>& p3d, const std::shared_ptr<TYImage>& color, const char* fileName);
        void processDepth16(const std::shared_ptr<TYImage>&  depth, std::vector<TY_VECT_3F>& p3d);
        void processXYZ48(const std::shared_ptr<TYImage>&  depth, std::vector<TY_VECT_3F>& p3d);
        void processDepth16(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color, std::vector<TY_VECT_3F>& p3d,  std::shared_ptr<TYImage>& registration_color);
        void processXYZ48(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color, std::vector<TY_VECT_3F>& p3d,  std::shared_ptr<TYImage>& registration_color);
};

TY_STATUS P3DCamera::Init()
{
    TY_STATUS status;
    
    status = stream_enable(stream_depth);
    if(status != TY_STATUS_OK) return status;

    if(has_stream(stream_color)) {
        status = stream_enable(stream_color);
        if(status != TY_STATUS_OK) return status;
        bool hasColorCalib = false;
        ASSERT_OK(TYHasFeature(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &hasColorCalib));
        if (hasColorCalib)
        {
            ASSERT_OK(TYGetStruct(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &color_calib, sizeof(color_calib)));
        }
    }
    
    ASSERT_OK(TYGetFloat(handle(), TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &f_depth_scale_unit));
    ASSERT_OK(TYHasFeature(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &depth_needUndistort));
    ASSERT_OK(TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &depth_calib, sizeof(depth_calib)));

    return TY_STATUS_OK;
}


void P3DCamera::savePointsToPly(const std::vector<TY_VECT_3F>& p3d, const std::shared_ptr<TYImage>& color, const char* fileName)
{
    int   pointsCnt = 0;
    const TY_VECT_3F *point = p3d.data();
    std::stringstream ss;
    if(color) {
        int32_t bpp = TYBitsPerPixel(color->pixelFormat());
        uint8_t* pixels = (uint8_t*)color->buffer();
        switch(bpp) {
            case 8://mono8
            {
                uint8_t* mono8 = pixels;
                for(int i = 0; i < p3d.size(); i++) {
                    if(!std::isnan(point->z)) {
                        ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << " " << 
                            (uint32_t)mono8[i] << " " << (uint32_t)mono8[i] << " " << (uint32_t)mono8[i] << std::endl;
                        pointsCnt++;
                    }
                    point++;
                }
                break;
            }
            case 16://mono16
            {
                uint16_t* mono16 = (uint16_t*)pixels;
                for(int i = 0; i < p3d.size(); i++) {
                    if(!std::isnan(point->z)) {
                        ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << " " << 
                            (uint32_t)(mono16[i] >> 8) << " " << (uint32_t)(mono16[i] >> 8) << " " << (uint32_t)(mono16[i] >> 8) << std::endl;
                        pointsCnt++;
                    }
                    point++;
                }
                break;
            }
            case 24://bgr888
            {   
                uint8_t* bgr = pixels;
                for(int i = 0; i < p3d.size(); i++) {
                    if(!std::isnan(point->z)) {
                        ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << " " << 
                            (uint32_t)bgr[3*i] << " " << (uint32_t)bgr[3*i + 1] << " " << (uint32_t)bgr[3*i + 2] << std::endl;
                        pointsCnt++;
                    }
                    point++;
                }
                break;
            }
            case 48://bgr16
            {
                uint16_t* bgr16 = (uint16_t*)pixels;
                for(int i = 0; i < p3d.size(); i++) {
                    if(!std::isnan(point->z)) {
                        ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << " " << 
                            (uint32_t)(bgr16[3*i] >> 8) << " " << (uint32_t)(bgr16[3*i + 1] >> 8) << " " << (uint32_t)(bgr16[3*i + 2] >> 8) << std::endl;
                        pointsCnt++;
                    }
                    point++;
                }
                break;
            }
            default:
                std::cout << "Unsupported RGB format!" << std::endl;
                for(int i = 0; i < p3d.size(); i++) {
                    if(!std::isnan(point->z)) {
                        ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << std::endl;
                        pointsCnt++;
                    }
                    point++;
                }
                break;
        }
    } else {
        for(int i = 0; i < p3d.size(); i++) {
            if(!std::isnan(point->z)) {
                ss << (point->x)/1000 << " " << (point->y)/1000 << " " << (point->z)/1000 << std::endl;
                pointsCnt++;
            }
            point++;
        }
    }

    FILE *fp         = fopen(fileName, "wb+");
    fprintf(fp, "ply\n");
    fprintf(fp, "format ascii 1.0\n");
    fprintf(fp, "element vertex %d\n", pointsCnt);
    fprintf(fp, "property float x\n");
    fprintf(fp, "property float y\n");
    fprintf(fp, "property float z\n");
    if(color) {
        fprintf(fp, "property uchar blue\n");
        fprintf(fp, "property uchar green\n");
        fprintf(fp, "property uchar red\n");
    }
    fprintf(fp, "end_header\n");
    fprintf(fp, "%s", ss.str().c_str());
    fflush(fp);
    fclose(fp);
}

void P3DCamera::processDepth16(const std::shared_ptr<TYImage>&  depth, std::vector<TY_VECT_3F>& p3d)
{
    if(!depth) return;

    if(depth->pixelFormat() == TY_PIXEL_FORMAT_DEPTH16) {    
        p3d.resize(depth->width() * depth->height());
        TYMapDepthImageToPoint3d(&depth_calib, depth->width(), depth->height(), (uint16_t*)depth->buffer(), &p3d[0], f_depth_scale_unit);
    }
}

void P3DCamera::processXYZ48(const std::shared_ptr<TYImage>&  depth, std::vector<TY_VECT_3F>& p3d)
{
    if(!depth) return;

    if(depth->pixelFormat() == TY_PIXEL_FORMAT_XYZ48) {
        int16_t* src = static_cast<int16_t*>(depth->buffer());
        p3d.resize(depth->width() * depth->height());
        for (int pix = 0; pix < p3d.size(); pix++) {
            p3d[pix].x = *(src + 3*pix + 0) * f_depth_scale_unit;
            p3d[pix].y = *(src + 3*pix + 1) * f_depth_scale_unit;
            p3d[pix].z = *(src + 3*pix + 2) * f_depth_scale_unit;
        }
    }
}

void P3DCamera::processDepth16(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color, std::vector<TY_VECT_3F>& p3d, std::shared_ptr<TYImage>& registration_color)
{
    if(!depth) return;

    ImageProcesser depth_processer = ImageProcesser("depth", depth, &depth_calib);
    ImageProcesser color_processer = ImageProcesser("color", color, &color_calib);
    if(depth_needUndistort)
        depth_processer.doUndistortion();

    if(TY_STATUS_OK == color_processer.doUndistortion()) {
        //do rgbd registration
        const std::shared_ptr<TYImage>& depth_image = depth_processer.image();
        const std::shared_ptr<TYImage>& color_image = color_processer.image();
        std::shared_ptr<TYImage> registration_depth = 
                std::shared_ptr<TYImage>(new TYImage(color_image->width(), 
                                            color_image->height(), 
                                            depth_image->componentID(), 
                                            TY_PIXEL_FORMAT_DEPTH16, 
                                            sizeof(uint16_t) * color_image->width() * color_image->height()));

        TYMapDepthImageToColorCoordinate(
            &depth_calib,
            depth_image->width(), depth_image->height(), static_cast<const uint16_t*>(depth_image->buffer()),
            &color_calib,
            registration_depth->width(), registration_depth->height(), static_cast<uint16_t*>(registration_depth->buffer()), f_depth_scale_unit
        );
        registration_color = color_image;
        p3d.resize(registration_depth->width() * registration_depth->height());
        TYMapDepthImageToPoint3d(&color_calib, registration_depth->width(), registration_depth->height()
            , (uint16_t*)registration_depth->buffer(), &p3d[0], f_depth_scale_unit);
    } else {
        processDepth16(depth, p3d);
    }
}

void P3DCamera::processXYZ48(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color, std::vector<TY_VECT_3F>& p3d, std::shared_ptr<TYImage>& registration_color)
{
    if(!depth) return;
    
    ImageProcesser color_processer = ImageProcesser("color", color, &color_calib);
    if(TY_STATUS_OK == color_processer.doUndistortion()) {
        registration_color = color_processer.image();

        processXYZ48(depth, p3d);

        TY_CAMERA_EXTRINSIC extri_inv;
        TYInvertExtrinsic(&color_calib.extrinsic, &extri_inv);
        TYMapPoint3dToPoint3d(&extri_inv, p3d.data(), p3d.size(), p3d.data());

        std::vector<uint16_t> mappedDepth(registration_color->width() * registration_color->height());
        TYMapPoint3dToDepthImage(&color_calib, p3d.data(), depth->width() * depth->height(),  registration_color->width(), registration_color->height(), mappedDepth.data(), f_depth_scale_unit);
        p3d.resize(registration_color->width() * registration_color->height());
        TYMapDepthImageToPoint3d(&color_calib, registration_color->width(), registration_color->height()
            , mappedDepth.data(), &p3d[0], f_depth_scale_unit);
    } else {
        processXYZ48(depth, p3d);
    }
}

int P3DCamera::process(const std::shared_ptr<TYImage>&  depth, const std::shared_ptr<TYImage>&  color)
{
    std::vector<TY_VECT_3F> p3d;
    std::shared_ptr<TYImage> registration_color = nullptr;
    if(!depth) {
        std::cout << "depth image is empty!" << std::endl;
        return -1;
    }

    TY_PIXEL_FORMAT fmt = depth->pixelFormat();
#ifdef OPENCV_DEPENDENCIES
    if(color) {
        if(fmt == TY_PIXEL_FORMAT_DEPTH16)
            processDepth16(depth, color, p3d, registration_color);
        else if(fmt == TY_PIXEL_FORMAT_XYZ48)
            processXYZ48(depth, color, p3d, registration_color);
        else {
            std::cout << "Invalid depth image format!" << std::endl;
            return -1;
        }
    } else 
#endif
    {
        if(fmt == TY_PIXEL_FORMAT_DEPTH16)
            processDepth16(depth, p3d);
        else if(fmt == TY_PIXEL_FORMAT_XYZ48)
            processXYZ48(depth, p3d);
        else {
            std::cout << "Invalid depth image format!" << std::endl;
            return -1;
        }
    }

    static int m_frame_cnt = 0;
    int ch = GetChar();
    switch(ch & 0xff) {
    case 'c': 
    {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        char file[32];
        struct tm* p = std::localtime(&now_time);
        sprintf(file, "%d.%d.%d %02d_%02d_%02d.ply", 1900 + p->tm_year, 1+p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
        std::cout << "Save : " << file << std::endl;
        savePointsToPly(p3d, registration_color,  file);
        std::cout << file << "Saved!" << std::endl;
        break;
    }
    case 'q':
        return -1;
    default:
        break;
    }

    m_frame_cnt++;

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

    P3DCamera _3dcam;
    if(TY_STATUS_OK != _3dcam.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    _3dcam.Init();
    if(TY_STATUS_OK != _3dcam.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }

    std::cout << "Press 'c' to take a picture and 'q' to exit" << std::endl;
    bool process_exit = false;
    while(!process_exit) {
        auto frame = _3dcam.tryGetFrames(2000);
        if(frame) {
            auto depth = frame->depthImage();
            auto color = frame->colorImage();
            if(depth && _3dcam.process(depth, color) < 0) {
                process_exit = true;
            }
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
