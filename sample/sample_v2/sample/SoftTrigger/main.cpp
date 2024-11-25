#include "Device.hpp"

using namespace percipio_layer;


class SoftTriggerCamera : public FastCamera
{
    public:
        SoftTriggerCamera() : FastCamera() {};
        ~SoftTriggerCamera() {}; 

        TY_STATUS SetSoftTriggerMode();
};


TY_STATUS SoftTriggerCamera::SetSoftTriggerMode()
{
    TY_DEV_HANDLE _handle = handle();
    if(!_handle) {
        std::cout << "Invalid camera handle!" << std::endl;
        return TY_STATUS_INVALID_HANDLE;
    }

    TY_STATUS status = TYSetBool(_handle, TY_COMPONENT_DEVICE, TY_BOOL_GVSP_RESEND, true);
    if(status != TY_STATUS_OK) {
        std::cout << "gvsp resend configuration failed!" << std::endl;
    }

    TY_TRIGGER_PARAM_EX triger_param;
    triger_param.mode = TY_TRIGGER_MODE_SLAVE;
    return TYSetStruct(_handle, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &triger_param, sizeof(triger_param));
}


int main(int argc, char* argv[])
{
    std::string ID;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }

    SoftTriggerCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    TY_STATUS status = camera.SetSoftTriggerMode();
    std::cout << "set soft trigger mode,  ret = " << status << std::endl;

    bool process_exit = false;
    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    status = camera.stream_enable(FastCamera::stream_depth);
    std::cout << "enable depth stream,  ret = " << status << std::endl;

    if(TY_STATUS_OK != camera.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }
    
    while(!process_exit) {
        int err = TY_STATUS_OK;
        while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(camera.handle())));
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

        auto frame = camera.tryGetFrames(2000);
        if(frame) parser.update(frame);
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
