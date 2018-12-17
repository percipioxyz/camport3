#ifndef SAMPLE_COMMON_COMMON_HPP_
#define SAMPLE_COMMON_COMMON_HPP_

#include "Utils.hpp"

#include <opencv2/opencv.hpp>
#include "DepthRender.hpp"
#include "MatViewer.hpp"
#include "TYThread.hpp"
#include "TyIsp.h"


static inline int parseFrame(const TY_FRAME_DATA& frame, cv::Mat* pDepth
        , cv::Mat* pLeftIR, cv::Mat* pRightIR
        , cv::Mat* pColor, TY_ISP_HANDLE color_isp_handle = NULL)
{
    for( int i = 0; i < frame.validCount; i++ ){
        // get depth image
        if(pDepth && frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM){
            *pDepth = cv::Mat(frame.image[i].height, frame.image[i].width
                    , CV_16U, frame.image[i].buffer).clone();
        }
        // get left ir image
        if(pLeftIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT){
            *pLeftIR = cv::Mat(frame.image[i].height, frame.image[i].width
                    , CV_8U, frame.image[i].buffer).clone();
        }
        // get right ir image
        if(pRightIR && frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT){
            *pRightIR = cv::Mat(frame.image[i].height, frame.image[i].width
                    , CV_8U, frame.image[i].buffer).clone();
        }
        // get BGR
        if(pColor && frame.image[i].componentID == TY_COMPONENT_RGB_CAM){
            if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_JPEG){
                cv::Mat jpeg(frame.image[i].height, frame.image[i].width
                    , CV_8UC1, frame.image[i].buffer);
                *pColor = cv::imdecode(jpeg,CV_LOAD_IMAGE_COLOR);
            }
            if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_YVYU){
                cv::Mat yuv(frame.image[i].height, frame.image[i].width
                            , CV_8UC2, frame.image[i].buffer);
                cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YVYU);
            }
            else if (frame.image[i].pixelFormat == TY_PIXEL_FORMAT_YUYV){
                cv::Mat yuv(frame.image[i].height, frame.image[i].width
                            , CV_8UC2, frame.image[i].buffer);
                cv::cvtColor(yuv, *pColor, cv::COLOR_YUV2BGR_YUYV);
            } else if(frame.image[i].pixelFormat == TY_PIXEL_FORMAT_RGB){
                cv::Mat rgb(frame.image[i].height, frame.image[i].width
                        , CV_8UC3, frame.image[i].buffer);
                cv::cvtColor(rgb, *pColor, cv::COLOR_RGB2BGR);
            } else if(frame.image[i].pixelFormat == TY_PIXEL_FORMAT_BGR){
                *pColor = cv::Mat(frame.image[i].height, frame.image[i].width
                        , CV_8UC3, frame.image[i].buffer);
            } else if(frame.image[i].pixelFormat == TY_PIXEL_FORMAT_BAYER8GB){
                if (!color_isp_handle){
                    cv::Mat raw(frame.image[i].height, frame.image[i].width
                                , CV_8U, frame.image[i].buffer);
                    cv::cvtColor(raw, *pColor, cv::COLOR_BayerGB2BGR);
                }
                else{
                    cv::Mat raw(frame.image[i].height, frame.image[i].width
                                , CV_8U, frame.image[i].buffer);
                    TY_IMAGE_DATA _img = frame.image[i];
                    pColor->create(_img.height, _img.width, CV_8UC3);
                    int sz = _img.height* _img.width * 3;
                    TY_IMAGE_DATA out_buff = TYInitImageData(sz, pColor->data, _img.width, _img.height);
                    out_buff.pixelFormat = TY_PIXEL_FORMAT_BGR;
                    int res = TYISPProcessImage(color_isp_handle, &_img, &out_buff);
                    if (res != TY_STATUS_OK){
                        //fall back to  using opencv api
                        cv::Mat raw(frame.image[i].height, frame.image[i].width
                                    , CV_8U, frame.image[i].buffer);
                        cv::cvtColor(raw, *pColor, cv::COLOR_BayerGB2BGR);
                    }
                }
            } else if(frame.image[i].pixelFormat == TY_PIXEL_FORMAT_MONO){
                cv::Mat gray(frame.image[i].height, frame.image[i].width
                        , CV_8U, frame.image[i].buffer);
                cv::cvtColor(gray, *pColor, cv::COLOR_GRAY2BGR);
            }
        }
    }

    return 0;
}

enum{
    PC_FILE_FORMAT_XYZ = 0,
};

static void writePC_XYZ(const cv::Point3f* pnts,const cv::Vec3b *color, size_t n, FILE* fp)
{
    if (color){
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f %d %d %d\n", pnts[i].x, pnts[i].y, pnts[i].z, color[i][0], color[i][1], color[i][2]);
            }
        }
    }
    else{
        for (size_t i = 0; i < n; i++){
            if (!std::isnan(pnts[i].x)){
                fprintf(fp, "%f %f %f 0 0 0\n", pnts[i].x, pnts[i].y, pnts[i].z);
            }
        }
    }
}

static void writePointCloud(const cv::Point3f* pnts,const cv::Vec3b *color, size_t n, const char* file, int format)
{
    FILE* fp = fopen(file, "w");
    if (!fp){
        return;
    }

    switch (format){
    case PC_FILE_FORMAT_XYZ:
        writePC_XYZ(pnts, color, n, fp);
        break;
    default:
        break;
    }

    fclose(fp);
}

#endif
