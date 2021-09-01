///////////////////////////////////////////////
/// A simple opengl cloud viewer
/// Copyright(C)2016-2018 Percipio All Rights Reserved
///////////////////////////////////////////////
#ifndef CLOUD_VIWER_HPP__
#define CLOUD_VIWER_HPP__
#include <TYApi.h>
#include <vector>



class GLPointCloudViewer{
public:

    static int GlInit();
    static int EnterMainLoop();
    static int LeaveMainLoop();
    static int Update(int point_num, const TY_VECT_3F* points, const uint8_t* color);
    static int ResetViewTranslate();
    static int RegisterKeyCallback(bool(*callback)(int));
    static int Deinit();//destroy all & exit

private:
    GLPointCloudViewer(){}
};




#endif
