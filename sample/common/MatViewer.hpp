#ifndef XYZ_MAT_VIEWER_HPP_
#define XYZ_MAT_VIEWER_HPP_

#include <opencv2/opencv.hpp>
#include <string>
#include "DepthRender.hpp"


class GraphicItem
{
public:
    GraphicItem(
            const cv::Scalar& color = cv::Scalar(255,255,255)
            )
        : _id(++globalID), _color(color) {}
    virtual ~GraphicItem() {}

    int id() const { return _id; }

    cv::Scalar color() const { return _color; }
    void setColor(const cv::Scalar& color) { _color = color; }

    virtual void draw(cv::Mat& img) = 0;

protected:
    int         _id;
    cv::Scalar  _color;

private:
    static int globalID;

};

class GraphicRectangleItem : public GraphicItem
{
public:
    cv::Rect _rect;

    GraphicRectangleItem(
            const cv::Scalar& color = cv::Scalar(255,255,255),
            const cv::Rect& rect = cv::Rect()
            )
        : GraphicItem(color), _rect(rect) {}
    virtual ~GraphicRectangleItem() {}
    void set(const cv::Rect& rect) { _rect = rect; }
    virtual void draw(cv::Mat& img){ cv::rectangle(img, _rect, color()); }
};

class GraphicStringItem : public GraphicItem
{
public:
    std::string _str;
    cv::Point   _loc;
    double      _scale;
    int         _thick;

    GraphicStringItem(
            const std::string& str = std::string(),
            const cv::Point loc = cv::Point(),
            double scale = 0,
            const cv::Scalar& color = cv::Scalar(),
            int thick = 0
            )
        : GraphicItem(color), _str(str), _loc(loc), _scale(scale), _thick(thick) {}
    virtual ~GraphicStringItem() {}
    void set(const std::string& str) { _str = str; }
    virtual void draw(cv::Mat& img){
        cv::putText(img, _str, _loc, cv::FONT_HERSHEY_SIMPLEX, _scale, _color, _thick);
    }
};


class OpencvViewer
{
public:
    OpencvViewer(const std::string& win)
        : _win(win)
      {
        _has_win = 0;
        //cv::namedWindow(_win);
        //cv::setMouseCallback(_win, _onMouseCallback, this);
      }
    ~OpencvViewer()
      {
        if (_has_win)
          {
            //cv::setMouseCallback(_win, NULL, NULL);
            cv::destroyWindow(_win);
          }
      }

    const std::string& name() const {return _win;}

    virtual void show(const cv::Mat& img)
      {
        _has_win = 1;
        _orgImg = img.clone();
        showImage();
      }
    virtual void onMouseCallback(cv::Mat& /*img*/, int /*event*/, const cv::Point /*pnt*/
                    , bool& repaint) {repaint = false;}

    void addGraphicItem(GraphicItem* item) {
                    _items.insert(std::make_pair(item->id(), item));}
    void delGraphicItem(GraphicItem* item) { _items.erase(item->id()); }

private:
    static void _onMouseCallback(int event, int x, int y, int flags, void* ustc);

    void showImage();

    cv::Mat _orgImg;
    cv::Mat _showImg;
    int _has_win;
    std::string _win;
    std::map<int, GraphicItem*> _items;
};

//////////////////////////////////////////////////////////////////////////////////

class DepthViewer : public OpencvViewer
{
public:
    DepthViewer(const std::string& win);
    virtual void show(const cv::Mat& depthImage);
    virtual void onMouseCallback(cv::Mat& img, int event, const cv::Point pnt
                    , bool& repaint);


    float depth_scale_unit;
private:
    cv::Mat _depth;
    cv::Mat _renderedDepth;
    DepthRender _render;
    GraphicStringItem  _centerDepthItem;
    GraphicStringItem  _pickedDepthItem;
    cv::Point   _fixLoc;
};


#endif
