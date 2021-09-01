#include <stdint.h>
#include <stdio.h>
#include "MatViewer.hpp"


int GraphicItem::globalID = 0;


void OpencvViewer::_onMouseCallback(int event, int x, int y, int /*flags*/, void* ustc)
{
    OpencvViewer* p = (OpencvViewer*)ustc;

    // NOTE: This callback will be called very frequently while mouse moving,
    // keep it simple

    bool repaint = false;
    p->onMouseCallback(p->_orgImg, event, cv::Point(x,y), repaint);
    if(repaint){
        p->showImage();
    }
}


void OpencvViewer::showImage()
{
    _showImg = _orgImg.clone();
    for(std::map<int, GraphicItem*>::iterator it = _items.begin()
            ; it != _items.end(); it++){
        it->second->draw(_showImg);
    }
    cv::imshow(_win.c_str(), _showImg);
    cv::setMouseCallback(_win, _onMouseCallback, this);
}

///////////////////////////// DepthViewer ///////////////////////////////////////


DepthViewer::DepthViewer(const std::string& win)
    : OpencvViewer(win)
    , _centerDepthItem(std::string(), cv::Point(0,20), 0.5, cv::Scalar(0,255,0), 2)
    , _pickedDepthItem(std::string(), cv::Point(0,40), 0.5, cv::Scalar(0,255,0), 2)
{
    OpencvViewer::addGraphicItem(&_centerDepthItem);
    OpencvViewer::addGraphicItem(&_pickedDepthItem);
    depth_scale_unit = 1.f;
}


void DepthViewer::show(const cv::Mat& img)
{
    if(img.type() != CV_16U || img.total() == 0){
        return;
    }

    char str[128];
    float val = img.at<uint16_t>(img.rows / 2, img.cols / 2)*depth_scale_unit;
    sprintf(str, "Depth at center: %.1f", val);
    _centerDepthItem.set(str);

    val = img.at<uint16_t>(_fixLoc.y, _fixLoc.x)*depth_scale_unit;
    sprintf(str, "Depth at (%d,%d): %.1f", _fixLoc.x, _fixLoc.y , val);
    _pickedDepthItem.set(str);

    _depth = img.clone();
    _renderedDepth = _render.Compute(img);
    OpencvViewer::show(_renderedDepth);
}


void DepthViewer::onMouseCallback(cv::Mat& img, int event, const cv::Point pnt
    , bool& repaint)
{
    repaint = false;
    switch(event){
        case cv::EVENT_LBUTTONDOWN: {
            _fixLoc = pnt;
            char str[64];
            float val = _depth.at<uint16_t>(pnt.y, pnt.x)*depth_scale_unit;
            sprintf(str, "Depth at (%d,%d): %.1f", pnt.x, pnt.y, val);
            printf(">>>>>>>>>>>>>>>> depth(%.1f)\n", val);
            _pickedDepthItem.set(str);
            repaint = true;
            break;
        }
        case cv::EVENT_MOUSEMOVE: 
            // uint16_t val = _img.at<uint16_t>(pnt.y, pnt.x);
            // char str[32];
            // sprintf(str, "Depth at mouse: %d", val);
            // drawText(img, str, cv::Point(0,60), 0.5, cv::Scalar(0,255,0), 2);
            break;
    }
}

