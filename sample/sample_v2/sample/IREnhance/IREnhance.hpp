/************************************************
 * @File Name:       sample/sample_v2/sample/IREnhance/IREnhance.hpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2025-01-16 15:17:14
 * @Modified Time:   2025-03-17 18:17:55
 ***********************************************/
#pragma once
#include "Device.hpp"
using namespace percipio_layer;

class IREnhanceProcesser: public ImageProcesser{
public:
    IREnhanceProcesser():ImageProcesser("Left-EnhanceIR"){}
    int parse(const std::shared_ptr<TYImage>& image){
        ImageProcesser::parse(image);
        return Enhance();
        
    }
    int get_type(TY_PIXEL_FORMAT fmt)
    {
        int type_ir = CV_8UC1;
        if (fmt == TY_PIXEL_FORMAT_MONO16) {
            type_ir = CV_16UC1;
        } else if (fmt != TY_PIXEL_FORMAT_MONO) {
            std::cout << "UnSupportted pixelFormat!" << std::endl;
            type_ir = -1;
        }
        return type_ir;
    }

    virtual int Enhance() = 0;
    std::string name;
    std::string func_desc;
};

class LinearStretchProcesser: public IREnhanceProcesser{
public:
    LinearStretchProcesser() {
        name = "LinearStretchProcesser";
        func_desc  = "result=(src-min(src))* 255.0 / (max(src) - min(src))";
    }
    //result=(grayIr-min(grayIr))* 255.0 / (max(grayIr) - min(grayIr))
    int Enhance() {
        int type_ir = IREnhanceProcesser::get_type(_image->pixelFormat());
        if (type_ir < 0) {
            return -1;
        }
        cv::Mat grayIR = cv::Mat(_image->height(), _image->width(),
            type_ir, _image->buffer());
        TY_COMPONENT_ID comp_id = _image->componentID();
        cv::Mat result;
        if ((type_ir == CV_16UC1) || (type_ir == CV_8UC1)) {
            double minVal, maxVal;
            int rows = grayIR.rows, cols = grayIR.cols;
            double ratiocut = 0.1;
            cv::Rect roi = cv::Rect(int(cols * ratiocut), int(rows * ratiocut), int(cols - cols * ratiocut * 2), int(rows - rows * ratiocut * 2));
            cv::minMaxLoc(grayIR(roi), &minVal, &maxVal);
            grayIR.convertTo(result, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
            _image = std::shared_ptr<TYImage>(new TYImage(grayIR.cols, grayIR.rows, comp_id, TY_PIXEL_FORMAT_MONO, result.size().area()));
            memcpy(_image->buffer(), result.data, result.size().area());
        }
        else {
            LOGD("linearStretch support CV_8UC1 or CV_16UC1 gray,not support others type,please check grayIR type");
            return -1;
        }
        return 0;
    }
};

class LinearStretchMultiProcesser: public IREnhanceProcesser{
public:
    LinearStretchMultiProcesser(){
        name = "LinearStretchMultiProcesser";
        func_desc  = "result=src*multi_expandratio";
    }
    //result=grayIr*multi_expandratio
    int Enhance() {
        if (multi_expandratio <= 0) {
            LOGD("linearStretch_multi multi_expandratio must bigger than 0");
            return -1;
        }
        TY_COMPONENT_ID comp_id = _image->componentID();
        int type_ir = IREnhanceProcesser::get_type(_image->pixelFormat());
        if (type_ir < 0) {
            return -1;
        }
        cv::Mat grayIR = cv::Mat(_image->height(), _image->width(),
            type_ir, _image->buffer());
        cv::Mat result;
        if (type_ir == CV_16UC1) {
            grayIR.convertTo(result, CV_8UC1, multi_expandratio / 255.0);
            _image = std::shared_ptr<TYImage>(new TYImage(grayIR.cols, grayIR.rows, comp_id, TY_PIXEL_FORMAT_MONO, result.size().area()));
            memcpy(_image->buffer(), result.data, result.size().area());
        }
        else if (type_ir == CV_8UC1) {
            grayIR.convertTo(result, CV_8UC1, multi_expandratio);
            //image size not changed, use org buffer and _image
            memcpy(_image->buffer(), result.data, _image->size());
        }
        else {
            LOGD("linearStretch_multi support CV_8UC1 or CV_16UC1 gray,not support others type,please check grayIR type");
            return -1;
        }
        return 0;
    }
    double multi_expandratio = 8;

};

class LinearStretchStdProcesser: public IREnhanceProcesser{
public:
    LinearStretchStdProcesser(){
        name = "LinearStretchStdProcesser";
        func_desc  = "result=src*255.0/(std_expandratio*std(src))";
    }
    //result=grayIr*255.0/(std_expandratio*std(grayIr));
    int Enhance() {
        if (std_expandratio <= 0) {
            LOGD("GrayIR_linearStretch_std multi_expandratio must bigger than 0");
            return -1;
        }
        TY_COMPONENT_ID comp_id = _image->componentID();
        int type_ir = IREnhanceProcesser::get_type(_image->pixelFormat());
        if (type_ir < 0) {
            return -1;
        }
        cv::Mat grayIR = cv::Mat(_image->height(), _image->width(),
            type_ir, _image->buffer());
        cv::Mat result;

        cv::Mat meanvalue, stdvalue;
        cv::meanStdDev(grayIR, meanvalue, stdvalue);

        double use_norm_std = stdvalue.ptr<double>(0)[0];
        double use_norm = use_norm_std * std_expandratio + 1.0;
        if ((type_ir == CV_16UC1) || (type_ir == CV_8UC1)) {
            double minVal, maxVal;
            cv::minMaxLoc(grayIR, &minVal, &maxVal);
            grayIR.convertTo(result, CV_8UC1, 255.0 / use_norm);
            _image = std::shared_ptr<TYImage>(new TYImage(grayIR.cols, grayIR.rows, comp_id, TY_PIXEL_FORMAT_MONO, result.size().area()));
            memcpy(_image->buffer(), result.data, result.size().area());
        }
        else {
            LOGD("GrayIR_linearStretch_std support CV_8UC1 or CV_16UC1 gray,not support others type,please check grayIR type");
            return -1;
        }

        return 0;
    }
    double std_expandratio = 6;
};

class NoLinearStretchLog2Processer: public IREnhanceProcesser{
public:
    NoLinearStretchLog2Processer(){
        name = "NoLinearStretchLog2Processer";
        func_desc  = "result=log_expandratio * log2(src)";
    }
    //result=log_expandratio * log2(grayIr);
    int Enhance() {
        if (log_expandratio <= 0) {
            LOGD("GrayIR_nonlinearStretch_log multi_expandratio must bigger than 0");
            return -1;
        }

        TY_COMPONENT_ID comp_id = _image->componentID();
        int type_ir = IREnhanceProcesser::get_type(_image->pixelFormat());
        if (type_ir < 0) {
            return -1;
        }
        cv::Mat grayIR = cv::Mat(_image->height(), _image->width(),
            type_ir, _image->buffer());
        cv::Mat result;
        
        int rows = grayIR.rows;
        int cols = grayIR.cols;
        result = cv::Mat::zeros(rows, cols, CV_8UC1);
        if (type_ir == CV_16UC1) {

            for (int i = 0; i < rows; i++) {
                uint16_t* in = grayIR.ptr<uint16_t>(i);
                uint8_t* out = result.ptr<uint8_t>(i);
                for (int j = 0; j < cols; j++) {
                    uint16_t inone = in[j];
                    int outone = log_expandratio * log2(inone);
                    outone = outone < 255 ? outone : 255;
                    out[j] = uint8_t(outone);
                }
            }
        }
        else if (type_ir == CV_8UC1) {

            for (int i = 0; i < rows; i++) {
                uint8_t* in = grayIR.ptr<uint8_t>(i);
                uint8_t* out = result.ptr<uint8_t>(i);
                for (int j = 0; j < cols; j++) {
                    int inone = int(in[j]) * 255;
                    int outone = log_expandratio * log2(inone);
                    outone = outone < 255 ? outone : 255;
                    out[j] = uint8_t(outone);
                }
            }
        }
        else {
            LOGD("GrayIR_linearStretch_std support CV_8UC1 or CV_16UC1 gray,not support others type,please check grayIR type");
            return -1;
        }
        _image = std::shared_ptr<TYImage>(new TYImage(grayIR.cols, grayIR.rows, comp_id, TY_PIXEL_FORMAT_MONO, result.size().area()));
        memcpy(_image->buffer(), result.data, result.size().area());
        return 0;
    }
    double log_expandratio = 6;
};

class NoLinearStretchHistProcesser: public LinearStretchProcesser {
public:
    NoLinearStretchHistProcesser(){
        name = "NoLinearStretchHistProcesser";
        func_desc  = "result=equalizeHist(src)";
    }
    //result=equalizeHist(grayIr);
    int Enhance() {
        int type_ir = IREnhanceProcesser::get_type(_image->pixelFormat());
        if (type_ir < 0) {
            return -1;
        }
        cv::Mat grayIR = cv::Mat(_image->height(), _image->width(),
            type_ir, _image->buffer());
        TY_COMPONENT_ID comp_id = _image->componentID();
        cv::Mat result;

        if (type_ir == CV_16UC1) {
            //This process will change type to CV_8UC1
            LinearStretchProcesser::Enhance();
            cv::Mat tempgray = cv::Mat(_image->height(), _image->width(),
            CV_8UC1, _image->buffer());
            cv::equalizeHist(tempgray, result);
        }
        else if (type_ir == CV_8UC1) {
            cv::equalizeHist(grayIR, result);
        }
        else {
            LOGD("GrayIR_linearStretch_std support CV_8UC1 or CV_16UC1 gray,not support others type,please check grayIR type");
            return -1;
        }
        _image = std::shared_ptr<TYImage>(new TYImage(grayIR.cols, grayIR.rows, comp_id, TY_PIXEL_FORMAT_MONO, result.size().area()));
        memcpy(_image->buffer(), result.data, result.size().area());
        return 0;
    }
};

static int GetAllEnhancers(std::vector<std::shared_ptr<IREnhanceProcesser>> &enhancers)
{
    enhancers.push_back(std::shared_ptr<IREnhanceProcesser>(new LinearStretchProcesser()));
    enhancers.push_back(std::shared_ptr<IREnhanceProcesser>(new LinearStretchMultiProcesser()));
    enhancers.push_back(std::shared_ptr<IREnhanceProcesser>(new LinearStretchStdProcesser()));
    enhancers.push_back(std::shared_ptr<IREnhanceProcesser>(new NoLinearStretchLog2Processer()));
    enhancers.push_back(std::shared_ptr<IREnhanceProcesser>(new NoLinearStretchHistProcesser()));
    return 0;
}
