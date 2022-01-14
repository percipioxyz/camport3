#ifndef XYZ_INPAINTER_HPP_
#define XYZ_INPAINTER_HPP_


#include <opencv2/opencv.hpp>
#include "ImageSpeckleFilter.hpp"

//#warn("DepthInpainter this design no longer supported by new opencv version, using opencv inpaint api for alternative")


class DepthInpainter
{
public:
    int         _kernelSize;
    int         _maxInternalHoleToBeFilled;
    double      _inpaintRadius;
    bool        _fillAll;


    DepthInpainter()
        : _kernelSize(5)
        , _maxInternalHoleToBeFilled(50)
        , _inpaintRadius(1)
        , _fillAll(true)
    {
    }

    void inpaint(const cv::Mat& inputDepth, cv::Mat& out, const cv::Mat& mask);

private:
    cv::Mat genValidMask(const cv::Mat& depth);
};

#endif
