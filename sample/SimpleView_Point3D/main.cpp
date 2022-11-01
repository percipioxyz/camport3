#include <limits>
#include <cassert>
#include <cmath>
#include "../common/common.hpp"
#include "../common/cloud_viewer/cloud_viewer.hpp"
#include "TYImageProc.h"

#define MAP_DEPTH_TO_COLOR      1

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    TY_ISP_HANDLE   isp_handle;
    TY_CAMERA_CALIB_INFO depth_calib; 
    TY_CAMERA_CALIB_INFO color_calib;
    
    float f_depth_scale;

    bool saveOneFramePoint3d;
    bool exit_main;
    int  fileIndex;
    bool map_depth_to_color;
};

CallbackData cb_data;
TY_ISP_HANDLE isp_handle = NULL;

bool isTof = false;
cv::Mat tofundis_mapx, tofundis_mapy;

//////////////////////////////////////////////////

static void doRegister(const TY_CAMERA_CALIB_INFO& depth_calib
    , const TY_CAMERA_CALIB_INFO& color_calib
    , const cv::Mat& depth
    , const float f_scale_unit
    , const cv::Mat& color
    , cv::Mat& undistort_color
    , cv::Mat& out
    , bool map_depth_to_color
)
{
    // do undistortion
    TY_IMAGE_DATA src;
    src.width = color.cols;
    src.height = color.rows;
    src.size = color.size().area() * 3;
    src.pixelFormat = TY_PIXEL_FORMAT_RGB;
    src.buffer = color.data;

    undistort_color = cv::Mat(color.size(), CV_8UC3);
    TY_IMAGE_DATA dst;
    dst.width = color.cols;
    dst.height = color.rows;
    dst.size = undistort_color.size().area() * 3;
    dst.buffer = undistort_color.data;
    dst.pixelFormat = TY_PIXEL_FORMAT_RGB;
    ASSERT_OK(TYUndistortImage(&color_calib, &src, NULL, &dst));

    // do register
    if (map_depth_to_color) {
        out = cv::Mat::zeros(undistort_color.size(), CV_16U);
        ASSERT_OK(
            TYMapDepthImageToColorCoordinate(
                &depth_calib,
                depth.cols, depth.rows, depth.ptr<uint16_t>(),
                &color_calib,
                out.cols, out.rows, out.ptr<uint16_t>(), f_scale_unit
            )
        );
        cv::Mat temp;
        //you may want to use median filter to fill holes in projected depth image
        //or do something else here
        cv::medianBlur(out, temp, 5);
        out = temp;
    }
    else {
        out = cv::Mat::zeros(depth.size(), CV_8UC3);
        ASSERT_OK(
            TYMapRGBImageToDepthCoordinate(
                &depth_calib,
                depth.cols, depth.rows, depth.ptr<uint16_t>(),
                &color_calib,
                undistort_color.cols, undistort_color.rows, undistort_color.ptr<uint8_t>(),
                out.ptr<uint8_t>(), f_scale_unit
            )
        );
        undistort_color = out.clone();
    }
}


static void handleFrame(TY_FRAME_DATA* frame, void* userdata) {
    //we only using Opencv Mat as data container.
    //you can allocate memory by yourself.
    CallbackData* pData = (CallbackData*) userdata;
    LOGD("=== Get frame %d", ++pData->index);

    cv::Mat depth, color;
    parseFrame(*frame, &depth, NULL, NULL, &color, isp_handle);
    if(!depth.empty()){
        if (isTof)
        {
            TY_IMAGE_DATA src;
            src.width = depth.cols;
            src.height = depth.rows;
            src.size = depth.size().area() * 2;
            src.pixelFormat = TY_PIXEL_FORMAT_DEPTH16;
            src.buffer = depth.data;

            cv::Mat undistort_depth = cv::Mat(depth.size(), CV_16U);
            TY_IMAGE_DATA dst;
            dst.width = depth.cols;
            dst.height = depth.rows;
            dst.size = undistort_depth.size().area() * 2;
            dst.buffer = undistort_depth.data;
            dst.pixelFormat = TY_PIXEL_FORMAT_DEPTH16;
            ASSERT_OK(TYUndistortImage(&cb_data.depth_calib, &src, NULL, &dst));

            depth = undistort_depth.clone();
        }

        std::vector<TY_VECT_3F> p3d;
        
        uint8_t *color_data = NULL;
        cv::Mat color_data_mat, out;
        if (!color.empty()){
            bool hasColorCalib = false;
            ASSERT_OK(TYHasFeature(pData->hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &hasColorCalib));
            if (hasColorCalib)
            {
                doRegister(pData->depth_calib, pData->color_calib, depth, pData->f_depth_scale, color, color_data_mat, out, pData->map_depth_to_color);
                cv::cvtColor(color_data_mat, color_data_mat, cv::COLOR_BGR2RGB);
                color_data = color_data_mat.ptr<uint8_t>();
            }
        }

        if (pData->map_depth_to_color) {
            depth = out.clone();
            p3d.resize(depth.size().area());
            ASSERT_OK(TYMapDepthImageToPoint3d(&pData->color_calib, depth.cols, depth.rows
                , (uint16_t*)depth.data, &p3d[0]));
        }
        else
        {
            p3d.resize(depth.size().area());
            ASSERT_OK(TYMapDepthImageToPoint3d(&pData->depth_calib, depth.cols, depth.rows
                , (uint16_t*)depth.data, &p3d[0], pData->f_depth_scale));
        }
        if (pData->saveOneFramePoint3d){
            char file[32];
            sprintf(file, "points-%d.xyz", pData->fileIndex++);
            writePointCloud((cv::Point3f*)&p3d[0], (const cv::Vec3b*)color_data_mat.data, p3d.size(), file, PC_FILE_FORMAT_XYZ);
            pData->saveOneFramePoint3d = false;
        }
        for (int idx = 0; idx < p3d.size(); idx++){//we adjust coordinate for display
            p3d[idx].y = -p3d[idx].y;
            p3d[idx].z = -p3d[idx].z;
        }
        GLPointCloudViewer::Update(p3d.size(), &p3d[0], color_data);
    }
}

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

static int FetchOneFrame(CallbackData &cb){
    TY_FRAME_DATA frame;
    int err = TYFetchFrame(cb.hDevice, &frame, -1);
    if (err != TY_STATUS_OK){
        LOGD("... Drop one frame");
        return -1;
    }
    handleFrame(&frame, &cb);
    LOGD("=== Re-enqueue buffer(%p, %d)", frame.userBuffer, frame.bufferSize);
    TYEnqueueBuffer(cb.hDevice, frame.userBuffer, frame.bufferSize);
    TYISPUpdateDevice(cb.isp_handle);
    return 0;
}

void* FetchFrameThreadFunc(void* d){
    CallbackData &cb = *(CallbackData*)d;
    while(!cb.exit_main){
        if (FetchOneFrame(cb) != 0){
            break;
        }
    }
    return NULL;
}

bool key_pressed(int key){
    if (key == 's'){
        cb_data.saveOneFramePoint3d = true;
        return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    GLPointCloudViewer::GlInit();
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    bool with_color_cam = true;
    bool dep2rgb = false;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-dep2rgb") == 0) {
            dep2rgb = true;
        }else if(strcmp(argv[i], "-h") == 0){
            printf("Usage: SimpleView_Point3D [-h] [-id <ID>] [-color=off] [-dep2rgb]");
            return 0;
        }
        else if (strcmp(argv[i], "-color=off") == 0){
            with_color_cam = false;
        }
    }

    LOGD("=== Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

    LOGD("=== Configure components, open depth cam");
    int32_t componentIDs = TY_COMPONENT_DEPTH_CAM;
    ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

    //try to enable depth map
    LOGD("Configure components, open depth cam");
    if (componentIDs & TY_COMPONENT_DEPTH_CAM) {
        int32_t image_mode;
        ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
        LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
        ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));
        
        float scale_unit = 1.;
        bool hasScaleUint = false;
        //Incase some model Desc has No ScaleUint Now(Tof), Then Suppose it is 1.0f
        TYHasFeature(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &hasScaleUint);
        if(hasScaleUint) {
            TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
        }
        cb_data.f_depth_scale = scale_unit;
    }

    int32_t allComps;
    ASSERT_OK(TYGetComponentIDs(hDevice, &allComps));
    if ((allComps & TY_COMPONENT_RGB_CAM) && (with_color_cam)){
        LOGD("=== Has internal RGB camera, try to open it");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));

        bool hasColorCalib = false;
        ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &hasColorCalib));
        if (hasColorCalib)
        {
            ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA
                , &cb_data.color_calib, sizeof(cb_data.color_calib)));
        }
        ASSERT_OK(TYISPCreate(&isp_handle)); //create a default isp handle for bayer rgb images
        cb_data.isp_handle = isp_handle;
        ASSERT_OK(ColorIspInitSetting(isp_handle, hDevice));
        //You can turn on auto exposure function as follow ,but frame rate may reduce .
        //Device also may be casually stucked  1~2 seconds when software trying to adjust device exposure time value
#if 0
        ASSERT_OK(ColorIspInitAutoExposure(isp_handle, hDevice));
#endif
    }

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[2];
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("=== Read depth intrinsic");
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
        , &cb_data.depth_calib, sizeof(cb_data.depth_calib)));

    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &isTof));

    LOGD("=== Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    bool hasTrigger = false;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &hasTrigger));
    if (hasTrigger) {
        LOGD("=== Disable trigger mode");
        TY_TRIGGER_PARAM trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
    }

    LOGD("=== Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    cb_data.index = 0;
    cb_data.hDevice = hDevice;
    cb_data.saveOneFramePoint3d = false;
    cb_data.fileIndex = 0;
    cb_data.exit_main = false;
    cb_data.map_depth_to_color = dep2rgb;
    //start a thread to fetch image data
    TYThread fetch_thread;
    fetch_thread.create(FetchFrameThreadFunc, &cb_data);
    LOGD("=== While loop to fetch frame");
    FetchOneFrame(cb_data); 
    GLPointCloudViewer::ResetViewTranslate();//init view position by first frame
    GLPointCloudViewer::RegisterKeyCallback(key_pressed);//key pressed callback
    GLPointCloudViewer::EnterMainLoop();//start main window 
    cb_data.exit_main = true;//wait work thread to exit
    fetch_thread.destroy();

    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];
    if (isp_handle){
        TYISPRelease(&isp_handle);
    }
    LOGD("=== Main done!");
    GLPointCloudViewer::Deinit();
    return 0;
}

