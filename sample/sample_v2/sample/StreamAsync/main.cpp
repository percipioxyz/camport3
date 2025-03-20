#include "Device.hpp"

using namespace percipio_layer;

class StreamAsyncCamera : public FastCamera
{
    public:
        StreamAsyncCamera() : FastCamera() {};
        ~StreamAsyncCamera() {}; 

        TY_STATUS Init(bool color_en, bool depth_en, bool ir_en, bool resend_en);

        uint32_t streams = 0;
};

TY_STATUS StreamAsyncCamera::Init(bool color_en, bool depth_en, bool ir_en, bool resend_en)
{
    TY_COMPONENT_ID allComps;
    streams = 0;
    ASSERT_OK( TYGetComponentIDs(handle(), &allComps) );
    if(allComps & TY_COMPONENT_RGB_CAM  && color_en) {
        streams |= FastCamera::stream_color;
        stream_enable(FastCamera::stream_color);
    }
    
    if (depth_en) {
        streams |= FastCamera::stream_depth;
        stream_enable(FastCamera::stream_depth);
    }

    if (ir_en) {
        streams |= FastCamera::stream_ir_left;
        stream_enable(FastCamera::stream_ir_left);
    }

    TY_TRIGGER_PARAM trigger;
    trigger.mode = TY_TRIGGER_MODE_SLAVE;
    ASSERT_OK(TYSetStruct(handle(), TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));

    //for network only
    if (resend_en) {
        bool hasResend;
        ASSERT_OK(TYHasFeature(handle(), TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, &hasResend));
        if (hasResend) {
            ASSERT_OK(TYSetBool(handle(), TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true));
        }
    }

    std::cout << "=== Set Stream Async" << std::endl;
    ASSERT_OK(TYSetEnum(handle(), TY_COMPONENT_DEVICE, TY_ENUM_STREAM_ASYNC, TY_STREAM_ASYNC_ALL));

    return TY_STATUS_OK;
}

int main(int argc, char* argv[])
{
    std::string ID;
    int32_t color, ir, depth;
    int32_t resend = 1;
    color = ir = depth = 1;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        }  else if(strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        } else if(strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        } else if(strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        } else if (strcmp(argv[i], "-resend=off") == 0) {
            resend = 0;
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }

    if (!color && !depth && !ir) {
        std::cout << "=== At least one component need to be on" <<std::endl;
        return -1;
    }

    StreamAsyncCamera sync_cam;
    if(TY_STATUS_OK != sync_cam.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    bool process_exit = false;
    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    sync_cam.Init(depth, color, ir, resend);

    if(TY_STATUS_OK != sync_cam.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }

    int32_t stream_expect = sync_cam.streams;
    std::cout << "=== Send Soft Trigger" << std::endl;
    int err = TY_STATUS_OK;
    while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(sync_cam.handle())));
    if (err != TY_STATUS_OK) {
        /*
         * Sometime we got other errors
         *     -1005 before 3.6.53 indicate trigger cmd timeout
         *     new sdk will return -1014 when trigger cmd timeout
         * If we got a timeout err, we do not know whether the
         * deivce missed the cmd or the app missied the ack.
         * We'd better restart the capture
         */
        LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
        process_exit = true;
    }

    int index = 0;
    
    while(!process_exit) {
        auto frame = sync_cam.tryGetFrames(2000);
        if(frame) {
            std::cout << "=== Get frame " << ++index << std::endl;
            parser.update(frame);
            auto color = frame->colorImage();
            auto depth = frame->depthImage();
            auto ir = frame->leftIRImage();
            if(color) {
                void *  image_pos  = color->buffer();
                int32_t image_size = color->height() * TYPixelLineSize(color->width(), color->pixelFormat());
                stream_expect &= ~FastCamera::stream_color;
                std::cout << "===   Image (RGB   , " <<  color->timestamp() << ", " << image_pos << ", "<<  image_size << ")" << std::endl;
            }

            if(depth) {
                void *  image_pos  = depth->buffer();
                int32_t image_size = depth->height() * TYPixelLineSize(depth->width(), depth->pixelFormat());
                stream_expect &= ~FastCamera::stream_depth;
                std::cout << "===   Image (DEPTH   , " <<  depth->timestamp() << ", " << image_pos << ", "<<  image_size << ")" << std::endl;
            }

            if(ir) {
                void *  image_pos  = ir->buffer();
                int32_t image_size = ir->height() * TYPixelLineSize(ir->width(), ir->pixelFormat());
                stream_expect &= ~FastCamera::stream_ir_left;
                std::cout << "===   Image (LEFT_IR   , " <<  ir->timestamp() << ", " << image_pos << ", "<<  image_size << ")" << std::endl;
            }

            if (stream_expect == 0) {
                stream_expect = sync_cam.streams;
                std::cout << "=== Send Soft Trigger" << std::endl;
                while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(sync_cam.handle())));
                if (err != TY_STATUS_OK) {
                    /*
                     * Sometime we got other errors
                     *     -1005 before 3.6.53 indicate trigger cmd timeout
                     *     new sdk will return -1014 when trigger cmd timeout
                     * If we got a timeout err, we do not know whether the
                     * deivce missed the cmd or the app missied the ack.
                     * We'd better restart the capture
                     */
                    LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
                    break;
                }
            }
        }
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
