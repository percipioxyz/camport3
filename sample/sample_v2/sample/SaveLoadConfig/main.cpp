#include <fstream>
#include <string>
#include "Device.hpp"

using namespace percipio_layer;

class StorageCfgTestCamera : public FastCamera
{
    public:
        StorageCfgTestCamera() : FastCamera() {};
        ~StorageCfgTestCamera() {}; 

        TY_STATUS SaveJsonCfg(const std::string file);
        TY_STATUS DumpJsonCfg(const std::string file);
};

TY_STATUS StorageCfgTestCamera::SaveJsonCfg(const std::string file)
{
    return write_parameters_to_storage(handle(),  file);
}

TY_STATUS StorageCfgTestCamera::DumpJsonCfg(const std::string file)
{
    std::string js_code;
    TY_STATUS ret = load_parameters_from_storage(handle(), js_code);
    if(ret == TY_STATUS_OK) {
        std::ofstream filestream(file.c_str()); // 创建ofstream对象，打开文件
        if (filestream.is_open()) { // 检查文件是否成功打开
            filestream << js_code; // 写入字符串到文件
            filestream.close(); // 关闭文件
        } else {
            // 错误处理
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    std::string ID;
    std::string config_file;
    std::string output_file;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        }else if(strcmp(argv[i], "-s") == 0) {
            config_file = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0) {
            output_file = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>] [-ip <IP>] [-s <config_file>] [-o <dump_file>]" << std::endl;
            std::cout << "\t[-h] Show this help info" << std::endl;
            std::cout << "\t[-id <ID>] select camera sn to work with" << std::endl;
            std::cout << "\t[-s <config_file>] save config_file to camera storage area" << std::endl;
            std::cout << "\t[-o <dump_file>] save configs read from camera storage area to dump_file" << std::endl;
            return 0;
        }
    }

    StorageCfgTestCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    TY_STATUS ret;
    if (!config_file.empty()) {
        ret = camera.SaveJsonCfg(config_file);
        std::cout << "Write Config Done (ret = " << ret << ")" << std::endl;
    } else {
        ret = camera.DumpJsonCfg(output_file);
        std::cout << "Save Config Done (ret = " << ret << ")" << std::endl;
    }

    return 0;
}