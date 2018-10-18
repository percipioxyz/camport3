#include <limits>
#include <cassert>
#include <cmath>
#include "../common/common.hpp"
#include "TYImageProc.h"


struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    DepthViewer*    depthViewer;
    PointCloudViewer* pcviewer;
    TY_CAMERA_CALIB_INFO* pDepthIntri; 

    bool saveOneFramePoint3d;
    bool exit_main;
    int  fileIndex;
};

static void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
    CallbackData* pData = (CallbackData*) userdata;
    LOGD("=== Get frame %d", ++pData->index);

    cv::Mat depth, color;
    parseFrame(*frame, &depth, 0, 0, &color);

    if(!depth.empty()){
        pData->depthViewer->show(depth);

        cv::Mat p3d(depth.size(), CV_32FC3);
        ASSERT_OK( TYMapDepthImageToPoint3d(pData->pDepthIntri, depth.cols, depth.rows
                  , depth.ptr<uint16_t>(), p3d.ptr<TY_VECT_3F>()) );

        pData->pcviewer->show(p3d, "Point3D");
        if(pData->pcviewer->isStopped("Point3D")){
            pData->exit_main = true;
            return;
        }

        if(pData->saveOneFramePoint3d){
            char file[32];
            sprintf(file, "points-%d.xyz", pData->fileIndex++);
            writePointCloud((cv::Point3f*)p3d.data, p3d.total(), file, PC_FILE_FORMAT_XYZ);
            pData->saveOneFramePoint3d = false;
        }
    }

    if(!color.empty()){
        imshow("Color", color);
    }

    int key = cv::waitKey(100);
    switch(key & 0xff){
        case 0xff:
            break;
        case 'q':
            pData->exit_main = true;
            break;
        case 's':
            pData->saveOneFramePoint3d = true;
            break;
        default:
            LOGD("Pressed key %d", key);
    }

    LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
    ASSERT_OK( TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize) );
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

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        }else if(strcmp(argv[i], "-h") == 0){
            LOGI("Usage: SimpleView_Point3D [-h] [-id <ID>]");
            return 0;
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

    LOGD("=== Configure feature, set resolution to 640x480.");
    int err = TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_DEPTH16_640x480);
    ASSERT(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);
    ASSERT( frameSize >= 640*480*2 );

    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[2];
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("=== Read depth intrinsic");
    TY_CAMERA_CALIB_INFO depth_calib; 
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
                          , &depth_calib, sizeof(depth_calib)) );

    LOGD("=== Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("=== Disable trigger mode");
    TY_TRIGGER_PARAM trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));

    LOGD("=== Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    DepthViewer depthViewer("depth");
    PointCloudViewer pcviewer;
    CallbackData cb_data;
    cb_data.index = 0;
    cb_data.hDevice = hDevice;
    cb_data.depthViewer = &depthViewer;
    cb_data.pcviewer = &pcviewer;
    cb_data.pDepthIntri = &depth_calib;
    cb_data.saveOneFramePoint3d = false;
    cb_data.fileIndex = 0;
    cb_data.exit_main = false;

    LOGD("=== While loop to fetch frame");
    TY_FRAME_DATA frame;

    while(!cb_data.exit_main){
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err != TY_STATUS_OK ){
            LOGD("... Drop one frame");
            break;
        }

        handleFrame(&frame, &cb_data);
    }

    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("=== Main done!");
    return 0;
}

