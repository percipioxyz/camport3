#include <iomanip>
#include "Device.hpp"

using namespace percipio_layer;

template<typename T>
static std::string matrix_line(const T* data, int cnt)
{
    std::stringstream ss;
    for(int i = 0; i < cnt; i++)
        ss << std::setw(12) << std::setfill(' ') << *(data + i) << ", ";
    return ss.str();
}

template<typename T>
static bool is_valid_matrix(const T* data, int cnt)
{
    uint32_t size = sizeof(T) * cnt;
    std::shared_ptr<char> zero_block = std::shared_ptr<char>(new char[size]());
    return !!memcmp((void*)data, zero_block.get(), size);
}

static void cailbration_print(const TY_CAMERA_CALIB_INFO& calib_data)
{
    if(is_valid_matrix(calib_data.intrinsic.data, sizeof(calib_data.intrinsic.data) / sizeof(float))) {
        std::cout << "\tintrinsic size :" << calib_data.intrinsicWidth << " x " << calib_data.intrinsicHeight << std::endl;
        std::cout << "\tintrinsic matrix(3x3) :" << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.intrinsic.data + 0, 3) << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.intrinsic.data + 3, 3) << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.intrinsic.data + 6, 3) << std::endl;
    }

    if(is_valid_matrix(calib_data.extrinsic.data, sizeof(calib_data.extrinsic.data) / sizeof(float))) {
        std::cout << "\textrinsic matrix(4x4) :" << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.extrinsic.data + 0, 4) << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.extrinsic.data + 4, 4) << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.extrinsic.data + 8, 4) << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.extrinsic.data + 12, 4) << std::endl;
    }

    if(is_valid_matrix(calib_data.distortion.data, sizeof(calib_data.distortion.data) / sizeof(float))) {
        std::cout << "\tdistortion matrix(12x1) :" << std::endl;
        std::cout << "\t\t\t" << matrix_line(calib_data.distortion.data + 0, 12) << std::endl;
    }
}   

class TYCameraCalibData : public FastCamera
{
    public:
        TYCameraCalibData() : FastCamera() {};
        ~TYCameraCalibData() {}; 

        void calib_data_read();
};

void TYCameraCalibData::calib_data_read()
{
    TY_CAMERA_CALIB_INFO calib_data;
    TY_COMPONENT_ID allComps;
    TY_STATUS status;
    ASSERT_OK(TYGetComponentIDs(handle(), &allComps));

    if(allComps & TY_COMPONENT_DEPTH_CAM) {
        status = TYGetStruct(handle(), TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &calib_data, sizeof(calib_data));
        if(status == TY_STATUS_OK) {
            std::cout << "Depth cam cailbration data :" << std::endl;
            cailbration_print(calib_data);
        }
    }

    if(allComps & TY_COMPONENT_RGB_CAM) {
        status = TYGetStruct(handle(), TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA, &calib_data, sizeof(calib_data));
        if(status == TY_STATUS_OK) {
            std::cout << "RGB cam cailbration data :" << std::endl;
            cailbration_print(calib_data);
        }
    }

    if(allComps & TY_COMPONENT_IR_CAM_LEFT) {
        status = TYGetStruct(handle(), TY_COMPONENT_IR_CAM_LEFT, TY_STRUCT_CAM_CALIB_DATA, &calib_data, sizeof(calib_data));
        if(status == TY_STATUS_OK) {
            std::cout << "Left-IR cam cailbration data :" << std::endl;
            cailbration_print(calib_data);
        }
    }

    if(allComps & TY_COMPONENT_IR_CAM_RIGHT) {
        status = TYGetStruct(handle(), TY_COMPONENT_IR_CAM_RIGHT, TY_STRUCT_CAM_CALIB_DATA, &calib_data, sizeof(calib_data));
        if(status == TY_STATUS_OK) {
            std::cout << "Right-IR cam cailbration data :" << std::endl;
            cailbration_print(calib_data);
        }
    }
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

    TYCameraCalibData camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    camera.calib_data_read();

    std::cout << "Main done!" << std::endl;
    return 0;
}
