#include <thread>

#include "Frame.hpp"
#include "TYImageProc.h"

namespace percipio_layer {


TYImage::TYImage()
{
    memset(&image_data, 0, sizeof(image_data));
}

TYImage::TYImage(const TY_IMAGE_DATA& image) :
    m_isOwner(false)
{
    memcpy(&image_data, &image, sizeof(TY_IMAGE_DATA));
}

TYImage::TYImage(const TYImage& src)
{
    image_data.timestamp = src.timestamp();
    image_data.imageIndex = src.imageIndex();
    image_data.status = src.status();
    image_data.componentID = src.componentID();
    image_data.size = src.size();
    image_data.width = src.width();
    image_data.height = src.height();
    image_data.pixelFormat = src.pixelFormat();
    if(image_data.size) {
        m_isOwner = true;
        image_data.buffer = malloc(image_data.size);
        memcpy(image_data.buffer, src.buffer(), image_data.size);
    }
}

TYImage::TYImage(int32_t width, int32_t height, TY_COMPONENT_ID compID, TY_PIXEL_FORMAT format, int32_t size)
{
    image_data.size = size;
    image_data.width = width;
    image_data.height = height;
    image_data.componentID = compID;
    image_data.pixelFormat = format;
     if(image_data.size) {
        m_isOwner = true;
        image_data.buffer = calloc(image_data.size, 1);
    }
}

bool TYImage::resize(int w, int h)
{
#ifdef OPENCV_DEPENDENCIES
    cv::Mat src, dst;
    switch(image_data.pixelFormat)
    {
        case TY_PIXEL_FORMAT_BGR:
        case TY_PIXEL_FORMAT_RGB:
            src = cv::Mat(cv::Size(width(), height()), CV_8UC3, buffer());
            break;
        case TY_PIXEL_FORMAT_MONO:
            src = cv::Mat(cv::Size(width(), height()), CV_8U, buffer());
            break;
        case TY_PIXEL_FORMAT_MONO16:
            src = cv::Mat(cv::Size(width(), height()), CV_16U, buffer());
            break;
        case TY_PIXEL_FORMAT_BGR48:
            src = cv::Mat(cv::Size(width(), height()), CV_16UC3, buffer());
            break;
        case TY_PIXEL_FORMAT_RGB48:
            src = cv::Mat(cv::Size(width(), height()), CV_16UC3, buffer());
            break;
        case TY_PIXEL_FORMAT_DEPTH16:
            src = cv::Mat(cv::Size(width(), height()), CV_16U, buffer());
            break;
        default:
            return false;
    }

    if(image_data.pixelFormat == TY_PIXEL_FORMAT_DEPTH16)
        cv::resize(src, dst, cv::Size(w, h), 0, 0, cv::INTER_NEAREST);
    else
        cv::resize(src, dst, cv::Size(w, h));
    image_data.size = dst.cols * dst.rows * dst.elemSize() * dst.channels();
    image_data.width = dst.cols;
    image_data.height = dst.rows;
    if(m_isOwner) free(image_data.buffer);
    image_data.buffer = malloc(image_data.size);
    memcpy(image_data.buffer, dst.data, image_data.size);
    return true;
#else
    std::cout << "not support!" << std::endl;
    return false;
#endif
}

TYImage::~TYImage()
{
    if(m_isOwner) {
        free(image_data.buffer);
    }
}

ImageProcesser::ImageProcesser(const char* win, const TY_CAMERA_CALIB_INFO* calib_data, const TY_ISP_HANDLE isp_handle) 
{
    win_name = win;
    hasWin = false;
    color_isp_handle = isp_handle;
    if(calib_data != nullptr) {
        _calib_data = std::shared_ptr<TY_CAMERA_CALIB_INFO>(new TY_CAMERA_CALIB_INFO(*calib_data));
    } 
}

int ImageProcesser::parse(const std::shared_ptr<TYImage>& image)
{
    if(!image) return -1;
    TY_PIXEL_FORMAT format = image->pixelFormat();
#ifndef OPENCV_DEPENDENCIES
    std::cout << win() << " image size : " << image->width() << " x " << image->height() << std::endl;
#endif
    switch(format) {
        /*
        case TY_PIXEL_FORMAT_BGR:
        case TY_PIXEL_FORMAT_RGB:
        case TY_PIXEL_FORMAT_MONO:
        case TY_PIXEL_FORMAT_MONO16:
        case TY_PIXEL_FORMAT_BGR48:
        case TY_PIXEL_FORMAT_RGB48:
        */
        case TY_PIXEL_FORMAT_DEPTH16:
        {
            _image = std::shared_ptr<TYImage>(new TYImage(*image));
            return 0;
        }
        case TY_PIXEL_FORMAT_XYZ48:
        {
            std::vector<int16_t> depth_data(image->width() * image->height());
            int16_t* src = static_cast<int16_t*>(image->buffer());
            for (int pix = 0; pix < image->width()*image->height(); pix++) {
                depth_data[pix] = *(src + 3*pix + 2);
            }

            _image = std::shared_ptr<TYImage>(new TYImage(image->width(), image->height(), image->componentID(), TY_PIXEL_FORMAT_DEPTH16, depth_data.size() * sizeof(int16_t)));
            memcpy(_image->buffer(), depth_data.data(), image->size());
            return 0;
        }
        default:
        {
#ifdef OPENCV_DEPENDENCIES
            cv::Mat cvImage;
            int32_t         image_size;
            TY_PIXEL_FORMAT image_fmt;
            TY_COMPONENT_ID comp_id;
            comp_id = image->componentID();
            parseImage(image->image(),  &cvImage, color_isp_handle);
            switch(cvImage.type())
            {
            case CV_8U:
                //MONO8
                image_size = cvImage.size().area();
                image_fmt = TY_PIXEL_FORMAT_MONO;
                break;
            case CV_16U:
                //MONO16
                image_size = cvImage.size().area() * 2;
                image_fmt = TY_PIXEL_FORMAT_MONO16;
                break;
            case  CV_16UC3:
                //BGR48
                image_size = cvImage.size().area() * 6;
                image_fmt = TY_PIXEL_FORMAT_BGR48;
                break;
            default:
                //BGR888
                image_size = cvImage.size().area() * 3;
                image_fmt = TY_PIXEL_FORMAT_BGR;
                break;
            }
            _image = std::shared_ptr<TYImage>(new TYImage(cvImage.cols, cvImage.rows, comp_id, image_fmt, image_size));
            memcpy(_image->buffer(), cvImage.data, image_size);
            return 0;
    #else

            //Without the OpenCV library, image decoding is not supported yet.
            return -1;
    #endif
        }
    }
}


int ImageProcesser::DepthImageRender()
{
    if(!_image) return -1;
    TY_PIXEL_FORMAT format = _image->pixelFormat();
    if(format != TY_PIXEL_FORMAT_DEPTH16) return -1;

#ifdef OPENCV_DEPENDENCIES
    static DepthRender render;
    cv::Mat depth = cv::Mat(_image->height(), _image->width(), CV_16U, _image->buffer());
    cv::Mat bgr = render.Compute(depth);

    _image = std::shared_ptr<TYImage>(new TYImage(_image->width(), _image->height(), _image->componentID(), TY_PIXEL_FORMAT_BGR,  bgr.size().area() * 3));
    memcpy(_image->buffer(), bgr.data, _image->size());
    return 0;
#else
    return -1;
#endif
}

TY_STATUS ImageProcesser::doUndistortion()
{
    int ret = 0;
    if(ret == 0) {
        if(!_calib_data) {
            std::cout << "Calib data is empty!" << std::endl;
            return TY_STATUS_ERROR;
        }

        int32_t         image_size = _image->size();
        TY_PIXEL_FORMAT image_fmt = _image->pixelFormat();
        TY_COMPONENT_ID comp_id = _image->componentID();

        std::vector<uint8_t> undistort_image(image_size);

        TY_IMAGE_DATA src;
        src.width = _image->width();
        src.height = _image->height();
        src.size = image_size;
        src.pixelFormat = image_fmt;
        src.buffer = _image->buffer();

        TY_IMAGE_DATA dst;
        dst.width = _image->width();
        dst.height = _image->height();
        dst.size = image_size;
        dst.pixelFormat = image_fmt;
        dst.buffer = undistort_image.data();

        TY_STATUS status = TYUndistortImage(&*_calib_data, &src, NULL, &dst);
        if(status != TY_STATUS_OK) {
            std::cout << "Do image undistortion failed!" << std::endl;
            return status;
        }

        _image = std::shared_ptr<TYImage>(new TYImage(_image->width(), _image->height(), comp_id, image_fmt, image_size));
        memcpy(_image->buffer(), undistort_image.data(), image_size);
        return TY_STATUS_OK;
    } else {
        std::cout << "Image decoding failed." << std::endl;
        return TY_STATUS_ERROR;
    }
}

int ImageProcesser::show()
{
    if(!_image) return -1;
#ifdef OPENCV_DEPENDENCIES
    cv::Mat display;
    switch(_image->pixelFormat())
    {
        case TY_PIXEL_FORMAT_MONO:
        {
            display = cv::Mat(_image->height(), _image->width(), CV_8U, _image->buffer());
            break;
        }
        case TY_PIXEL_FORMAT_MONO16:
        {
            display = cv::Mat(_image->height(), _image->width(), CV_16U, _image->buffer());
            break;
        }
        case TY_PIXEL_FORMAT_BGR:
        {
            display = cv::Mat(_image->height(), _image->width(), CV_8UC3, _image->buffer());
            break;
        }
        case TY_PIXEL_FORMAT_BGR48:
        {
            display = cv::Mat(_image->height(), _image->width(), CV_16UC3, _image->buffer());
            break;
        }
        case TY_PIXEL_FORMAT_DEPTH16:
        {
            DepthImageRender();
            display = cv::Mat(_image->height(), _image->width(), CV_8UC3, _image->buffer());
            break;
        }
        default:
        {
            break;
        }
    }

    if(!display.empty()) {
        hasWin = true;
        cv::imshow(win_name.c_str(), display);
        int key = cv::waitKey(1);
        return key;
    }
    else
        std::cout << "Unknown image encoding format." << std::endl;
 #endif
    return 0;
}

void ImageProcesser::clear()
{
#ifdef OPENCV_DEPENDENCIES
    if (hasWin) {
        cv::destroyWindow(win_name.c_str());   
    }
#endif
}

TYFrame::TYFrame(const TY_FRAME_DATA& frame)
{
    bufferSize = frame.bufferSize;
    userBuffer.resize(bufferSize);
    memcpy(userBuffer.data(), frame.userBuffer, bufferSize);

#define TY_IMAGE_MOVE(src, dst, from, to) do { \
    (to) = (from); \
    (to.buffer) = reinterpret_cast<void*>((std::intptr_t(dst)) + (std::intptr_t(from.buffer) - std::intptr_t(src)));\
}while(0)

    for (int i = 0; i < frame.validCount; i++) {
        TY_IMAGE_DATA img;
        if (frame.image[i].status != TY_STATUS_OK) continue;
    
        // get depth image
        if (frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM) {  
            TY_IMAGE_MOVE(frame.userBuffer, userBuffer.data(), frame.image[i], img);
            _images[TY_COMPONENT_DEPTH_CAM] = std::shared_ptr<TYImage>(new TYImage(img));
        }
        // get left ir image
        if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT) {
            TY_IMAGE_MOVE(frame.userBuffer, userBuffer.data(), frame.image[i], img);
            _images[TY_COMPONENT_IR_CAM_LEFT] = std::shared_ptr<TYImage>(new TYImage(img));
        }
        // get right ir image
        if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT) {
            TY_IMAGE_MOVE(frame.userBuffer, userBuffer.data(), frame.image[i], img);
            _images[TY_COMPONENT_IR_CAM_RIGHT] = std::shared_ptr<TYImage>(new TYImage(img));
        }
        // get color image
        if (frame.image[i].componentID == TY_COMPONENT_RGB_CAM) {
            TY_IMAGE_MOVE(frame.userBuffer, userBuffer.data(), frame.image[i], img);
            _images[TY_COMPONENT_RGB_CAM] = std::shared_ptr<TYImage>(new TYImage(img));
        }
    }
}

TYFrame::~TYFrame()
{

}


TYFrameParser::TYFrameParser(uint32_t max_queue_size, const TY_ISP_HANDLE isp_handle)
{
    _max_queue_size = max_queue_size;
    isRuning = true;

    setImageProcesser(TY_COMPONENT_DEPTH_CAM, std::shared_ptr<ImageProcesser>(new ImageProcesser("depth")));
    setImageProcesser(TY_COMPONENT_IR_CAM_LEFT, std::shared_ptr<ImageProcesser>(new ImageProcesser("Left-IR")));
    setImageProcesser(TY_COMPONENT_IR_CAM_RIGHT, std::shared_ptr<ImageProcesser>(new ImageProcesser("Right-IR")));
    setImageProcesser(TY_COMPONENT_RGB_CAM, std::shared_ptr<ImageProcesser>(new ImageProcesser("color", nullptr, isp_handle)));

    processThread_ = std::thread(&TYFrameParser::display, this);
}

TYFrameParser::~TYFrameParser()
{
    isRuning = false;
    processThread_.join();
}

int TYFrameParser::setImageProcesser(TY_COMPONENT_ID id, std::shared_ptr<ImageProcesser> proc)
{
    stream[id] = proc;
    return 0;
}

int TYFrameParser::doProcess(const std::shared_ptr<TYFrame>& img)
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
    }

    if (depth) {
        stream[TY_COMPONENT_DEPTH_CAM]->parse(depth);
    }
    return 0;
}

void TYFrameParser::display()
{
    int ret = 0;
    while(isRuning) {
        if(images.size()) {
            std::unique_lock<std::mutex> lock(_queue_lock);
            std::shared_ptr<TYFrame> img = images.front();

            if(img) {
                images.pop();
                doProcess(img);
            }
        }

        for(auto& iter : stream) {
            ret = iter.second->show();
            if(ret > 0) {
                if(func_keyboard_event) func_keyboard_event(ret, user_data);
            }
        }
    }
}

inline void TYFrameParser::ImageQueueSizeCheck()
{
    while(images.size() >= _max_queue_size)
        images.pop();
}

void TYFrameParser::update(const std::shared_ptr<TYFrame>& frame)
{
    std::unique_lock<std::mutex> lock(_queue_lock);
    if(frame) {
        ImageQueueSizeCheck();
        images.push(frame);
#ifndef OPENCV_DEPENDENCIES        
        auto depth = frame->depthImage();
        auto color = frame->colorImage();
        auto left_ir = frame->leftIRImage();
        auto right_ir = frame->rightIRImage();

        if (left_ir) {
            auto image = left_ir;
            std::cout << "Left" << " image size : " << image->width() << " x " << image->height() << std::endl;
        }

        if (right_ir) {
            auto image = right_ir;
            std::cout << "Right" << " image size : " << image->width() << " x " << image->height() << std::endl;
        }

        if (color) {
            auto image = color;
            std::cout << "Color" << " image size : " << image->width() << " x " << image->height() << std::endl;
        }

        if (depth) {
            auto image = depth;
            std::cout << "Depth" << " image size : " << image->width() << " x " << image->height() << std::endl;
        }

#endif
    }
}
}//namespace percipio_layer
