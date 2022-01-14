#ifndef XYZ_IMAGE_SPECKLE_FILTER_HPP_
#define XYZ_IMAGE_SPECKLE_FILTER_HPP_


#include <vector>
#include <opencv2/opencv.hpp>


class ImageSpeckleFilter
{
public:
    void Compute(cv::Mat &image, int newVal = 0, int maxSpeckleSize = 50, int maxDiff = 6);

private:
    std::vector<char>   _labelBuf;
};

extern ImageSpeckleFilter gSpeckleFilter;



#endif
