/************************************************
 * @File Name:       sample/DumpCalibInfoToJson/CalibInfoToJsonToJson.hpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-16 09:44:45
 * @Modified Time:   2024-07-18 17:40:13
 ***********************************************/
#include "CalibInfo.hpp"
class CalibInfo;
namespace json11{
class Json;
}
struct TY_CAMERA_CALIB_INFO;
class CalibInfoToJson {
public:
  static CalibInfoPtr JsonStringToCalibInfo(const std::string json_str);
  static std::string toJsonString(const CalibInfo &info);

private:
  static std::string DepToJson(const TY_CAMERA_CALIB_INFO &info,
    bool hasDistortion, float scaleUnit);
  static std::string RGBToJson(const TY_CAMERA_CALIB_INFO &info);
  static std::string commonToJson(const TY_CAMERA_CALIB_INFO &info,
    bool needExtri, bool needDist);

  static int parseRGBJson(const json11::Json &json, TY_CAMERA_CALIB_INFO &info);
  static int parseDepJson(const json11::Json &json, TY_CAMERA_CALIB_INFO &info, 
    bool &hasDistortion, float &scaleUnit);
  static int parseCommonJson(const json11::Json &json, TY_CAMERA_CALIB_INFO &info, 
    bool &hasDistortion, bool needExtri);
};
