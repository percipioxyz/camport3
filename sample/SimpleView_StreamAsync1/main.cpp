#include "../common/common.hpp"


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
    int32_t color, ir, depth;
    color = ir = depth = 1;
    int32_t resend = 1;
    const int32_t buf_count = 10;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if (strcmp(argv[i], "-resend=off") == 0) {
            resend = 0;
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_StreamAsync1 [-h]");
            return 0;
        }
    }

    if (!color && !depth && !ir) {
        LOGD("=== At least one component need to be on");
        return -1;
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

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
    if(allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("=== Has RGB camera, open RGB cam");
        ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );
    }

    TY_COMPONENT_ID componentIDs = 0;
    LOGD("=== Configure components, open depth cam");
    if (depth) {
        componentIDs = TY_COMPONENT_DEPTH_CAM;
    }

    if (ir) {
        componentIDs |= TY_COMPONENT_IR_CAM_LEFT;
    }

    ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

    //try to enable depth map
    LOGD("Configure components, open depth cam");
    DepthViewer depthViewer("Depth");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
        TY_IMAGE_MODE image_mode;
        ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
        LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
        ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));

        //depth map pixel format is uint16_t ,which default unit is  1 mm
        //the acutal depth (mm)= PixelValue * ScaleUnit 
        float scale_unit = 1.;
        TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
        depthViewer.depth_scale_unit = scale_unit;
    }

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[buf_count];
    for (int i=0; i<buf_count; i++) {
        frameBuffer[i] = new char[frameSize];
        LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[i], frameSize);
        ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[i], frameSize) );
    }

    LOGD("=== Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("=== Set trigger to continuous mode");
    TY_TRIGGER_PARAM_EX trigger;
    trigger.mode = TY_TRIGGER_MODE_OFF;
    ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));

    //for network only
    LOGD("=== resend: %d", resend);
    if (resend) {
        bool hasResend;
        ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, &hasResend));
        if (hasResend) {
            LOGD("=== Open resend");
            ASSERT_OK(TYSetBool(hDevice, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true));
        } else {
            LOGD("=== Not support feature TY_BOOL_GVSP_RESEND");
        }
    }

    LOGD("=== Set Stream Async");
    ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEVICE, TY_ENUM_STREAM_ASYNC, TY_STREAM_ASYNC_DEPTH));

    LOGD("=== Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("=== While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;

    std::vector<TY_FRAME_DATA> frames;

    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("=== Get frame %d", ++index);

            for (int i=0; i<frame.validCount; i++) {
                if (frame.image[i].status == TY_STATUS_OK) {
                    void *  image_pos  = frame.image[i].buffer;
                    int32_t image_size = frame.image[i].height * TYPixelLineSize(frame.image[i].width, frame.image[i].pixelFormat);
                    if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT) {
                        LOGD("===   Image %d (LEFT_IR , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_IR_CAM_RIGHT) {
                        LOGD("===   Image %d (RIGHT_IR, %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_RGB_CAM) {
                        LOGD("===   Image %d (RGB     , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
                        LOGD("===   Image %d (DEPTH   , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                    }
                    if (frame.image[i].componentID == TY_COMPONENT_BRIGHT_HISTO) {
                        LOGD("===   Image %d (HISTO   , %" PRIu64 ", %p, %d)", i, frame.image[i].timestamp, image_pos, image_size);
                    }
                }
            }
            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            // Insert frame to frame list
            if (frames.size() == buf_count - 1) {
                // frame list full, drop the head
                LOGD("=== Incomplete Group Dropped, timestamp %" PRIu64 "", (unsigned long)frames[0].image[0].timestamp);
                ASSERT_OK( TYEnqueueBuffer(hDevice, frames[0].userBuffer, frames[0].bufferSize) );
                frames.erase(frames.begin());
            }
            if (frames.size() == 0) {
                // frame list empty, insert directly
                frames.insert(frames.begin(), frame);
            } else {
                // frame list normal, insert in timestamp order
                int i = 0;
                for (i=0; i<frames.size(); i++) {
                    if (frame.image[0].timestamp < frames[i].image[0].timestamp) {
                        break;
                    }
                }
                frames.insert(frames.begin() + i, frame);
            }

            // If the first two frames in frame list have the same timestamp, then they belong to the same group
            if (frames.size() >= 2) {
                if (frames[1].image[0].timestamp == frames[0].image[0].timestamp) {
                    LOGD("=== Complete Group Fetched, timestamp %" PRIu64 "", frames[0].image[0].timestamp);
                    ASSERT_OK( TYEnqueueBuffer(hDevice, frames[0].userBuffer, frames[0].bufferSize) );
                    ASSERT_OK( TYEnqueueBuffer(hDevice, frames[1].userBuffer, frames[1].bufferSize) );
                    frames.erase(frames.begin(), frames.begin() + 2);
                }
            }
        }
    }

    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    for (int i=0; i<buf_count; i++) {
        delete frameBuffer[i];
    }
    LOGD("=== Main done!");
    return 0;
}
