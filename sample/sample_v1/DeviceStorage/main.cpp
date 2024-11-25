#include "common.hpp"

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    std::string block;
    std::string input_file;
    std::string output_file;
    
    if (argc < 2) {
       LOGI("Usage: DeviceStorage [-id <ID>]/[-ip <IP>] -b custom/isp -i/-o <filename>"); 
       return 0;
    }
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-b") == 0) {
            block = argv[++i];
        } else if(strcmp(argv[i], "-i") == 0) {
            input_file = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0) {
            output_file = argv[++i];
        }
    }

    LOGD("Init lib");
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
    
    if (block == "custom") {
        uint32_t block_size;
        ASSERT_OK( TYGetByteArraySize(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_CUSTOM_BLOCK, &block_size) );
        LOGD("Custom Block Size: %u", block_size);

        uint8_t * buf = new uint8_t[block_size];
        memset(buf, 0, block_size);
        
        if (!output_file.empty()) {
            ASSERT_OK( TYGetByteArray(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_CUSTOM_BLOCK, buf, block_size) );
            FILE * fp = fopen(output_file.c_str(), "wb");
            ASSERT(fp);
            fwrite(buf, 1, block_size, fp);
            fclose(fp);
            LOGD("Read Custom Block Done!");
        } else if (!input_file.empty()) {
            FILE * fp = fopen(input_file.c_str(), "rb");
            ASSERT(fp);
            if (fread(buf, 1, block_size, fp) != block_size) {
                LOGD("read size %u not enough!\n", block_size);
            }
            fclose(fp);           
            ASSERT_OK( TYSetByteArray(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_CUSTOM_BLOCK, buf, block_size) );
            LOGD("Write Custom Block Done!");
        }
        delete[]buf;
    } else if (block == "isp") {
        uint32_t block_size;
        ASSERT_OK( TYGetByteArraySize(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, &block_size) );
        LOGD("ISP Block Size: %u", block_size);

        uint8_t * buf = new uint8_t[block_size];
        
        if (!output_file.empty()) {
            ASSERT_OK( TYGetByteArray(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, buf, block_size) );
            FILE * fp = fopen(output_file.c_str(), "wb");
            ASSERT(fp);
            fwrite(buf, 1, block_size, fp);
            fclose(fp);
            LOGD("Read ISP Block Done!");
        } else if (!input_file.empty()) {
            FILE * fp = fopen(input_file.c_str(), "rb");
            ASSERT(fp);
            if (fread(buf, 1, block_size, fp) != block_size) {
                LOGD("read size %u not enough!\n", block_size);
            }
            fclose(fp);           
            ASSERT_OK( TYSetByteArray(hDevice, TY_COMPONENT_STORAGE, TY_BYTEARRAY_ISP_BLOCK, buf, block_size) );
            LOGD("Write ISP Block Done!");
        }
        delete[]buf;
    }   

    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
