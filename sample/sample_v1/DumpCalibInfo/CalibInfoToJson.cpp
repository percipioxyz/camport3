/************************************************
 * @File Name:       sample/DumpCalibInfo/CalibInfoToJson.cpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-16 09:57:08
 * @Modified Time:   2024-07-18 17:40:19
 ***********************************************/
#include "Utils.hpp"
#include "json11.hpp"
#include "CalibInfoToJson.hpp"
using namespace json11;
const static std::string sn_key = "sn";
const static std::string ts_key = "timestamp";
const static std::string dep_key = "depth_calib_info";
const static std::string rgb_key = "color_calib_info";
const static std::string intri_key = "intri";
const static std::string extri_key = "extri";
const static std::string distortion_key = "distortion";
const static std::string width_key = "image_width";
const static std::string height_key = "image_height";
const static std::string scale_unit_key = "scale_unit";

std::string CalibInfoToJson::toJsonString(const CalibInfo &info)
{
  std::string le_ls = "\n\t";
  //header + sn
  std::string jsonStr = "{" + le_ls +
    "\"" + sn_key + "\": \"" + info.getSN()+ "\",";
  //timestamp
  jsonStr += le_ls + "\"" + ts_key +"\": \"";
  jsonStr += info.getTimeStamp()+ "\",";
  jsonStr += "\n";
  //depth
  jsonStr += CalibInfoToJson::DepToJson(info.getDepCalib(), info.hasDepDistortion(), info.getScaleUnit());
  if (info.hasRGB()) {
    //if has rgb, a "," append after depth json str
    jsonStr += ",\n";
    jsonStr += CalibInfoToJson::RGBToJson(info.getRGBCalib());
  }
  jsonStr += "\n}\n";
  return jsonStr;
}

std::string CalibInfoToJson::DepToJson(const TY_CAMERA_CALIB_INFO &info,
    bool hasDistortion, float scaleUnit)
{
  std::string le_ls = "\n\t\t";
  //"depth_calib_info" : {
  std::string str = "\t\"" + dep_key +"\" : {";
  //                        no extri
  str += CalibInfoToJson::commonToJson(info, false, hasDistortion);
  //,
  //"scale_unit":x.xxx
  str += "," + le_ls + "\"" + scale_unit_key + "\": " + std::to_string(scaleUnit);
  //}
  str += "\n\t}";
  return str;
}

std::string CalibInfoToJson::RGBToJson(const TY_CAMERA_CALIB_INFO &info)
{
  std::string le_ls = "\n\t\t";
  //"color_calib_info" : {
  std::string str = "\t\"" + rgb_key +"\" : {";
  //                       extri, distortion
  str += CalibInfoToJson::commonToJson(info, true, true);
  //}
  str += "\n\t}";
  return str;
}
std::string CalibInfoToJson::commonToJson(const TY_CAMERA_CALIB_INFO &info,
    bool needExtri, bool needDist)
{
  int i = 0;
  std::string le_ls = "\n\t\t";
  //"intri": [
  std::string str = le_ls + "\"" + intri_key + "\": [";
  //Last one do not contain ",' at end of line, so use 3*3-1
  for(i = 0; i < sizeof(info.intrinsic.data)/sizeof(info.intrinsic.data[0]) - 1; i++) {
    str += le_ls + std::to_string(info.intrinsic.data[i]) + ",";
  }
  str += le_ls + std::to_string(info.intrinsic.data[i]);
  str += le_ls + "],";
  //extri
  if (needExtri) {
    //"extri": [
    str += le_ls + "\"" + extri_key + "\": [";
    //Last one do not contain ",' at end of line, so use size -1
    for(i = 0; i < sizeof(info.extrinsic.data)/sizeof(info.extrinsic.data[0]); i++) {
      str += le_ls + std::to_string(info.extrinsic.data[i]) + ",";
    }
    str += le_ls + std::to_string(info.extrinsic.data[i]);
    str += le_ls + "],";
  }
  //distortion
  if (needDist) {
    //"distortion": [
    str += le_ls + "\"" + distortion_key + "\": [";
    //Last one do not contain ",' at end of line, so use size-1
    for(i = 0; i < sizeof(info.distortion.data)/sizeof(info.distortion.data[0]) - 1; i++) {
      str += le_ls + std::to_string(info.distortion.data[i]) + ",";
    }
    str += le_ls + std::to_string(info.distortion.data[i]);
    str += le_ls + "],";
  }
  //"image_width":xxx,
  str += le_ls + "\"" + width_key + "\": " + std::to_string(info.intrinsicWidth);
  str += ",";
  //"image_height":xxx,
  str += le_ls + "\"" + height_key + "\": " + std::to_string(info.intrinsicHeight);
  return str;
}

int CalibInfoToJson::parseRGBJson(const Json &json, TY_CAMERA_CALIB_INFO &info)
{
  bool hasDist = true;
  if(CalibInfoToJson::parseCommonJson(json, info, hasDist, true) < 0) {
    return -1;
  }
  return 0;
}

int CalibInfoToJson::parseDepJson(const Json &json, TY_CAMERA_CALIB_INFO &info,
     bool &hasDistortion, float &scaleUnit)
{
  //If json do not has scale unit, default init as 1.0f
  scaleUnit = 1.0f;
  if(CalibInfoToJson::parseCommonJson(json, info, hasDistortion, false) < 0) {
    return -1;
  }
  auto su= json[scale_unit_key];
  if (su.is_number()) {
    scaleUnit = su.number_value();
  }
  return 0;
}

int CalibInfoToJson::parseCommonJson(const Json &json, TY_CAMERA_CALIB_INFO &info,
     bool &hasDistortion, bool needExtri)
{
  hasDistortion = false;
  auto intri = json[intri_key];
  if (!intri.is_array())  {
    printf("intri is missing\n");
    return -1;
  }
  size_t size = intri.array_items().size();
  printf("intri size %lu\n", size);
  if (size > sizeof(info.intrinsic.data)) {
    size = sizeof(info.intrinsic.data);
  }
  for(size_t i = 0; i < size; i++) {
    info.intrinsic.data[i] = intri[i].number_value();
  }
  if (needExtri) {
    auto extri = json[extri_key];
    if (!extri.is_array())  {
      printf("extri is missing\n");
      return -1;
    }
    size = extri.array_items().size();
    printf("extri size %lu\n", size);
    if (size > sizeof(info.extrinsic.data)) {
      size = sizeof(info.extrinsic.data);
    }
    for(size_t i = 0; i < size; i++) {
      info.extrinsic.data[i] = extri[i].number_value();
    }
  }
  auto dist = json[distortion_key];
  //Only is array type parse
  if (dist.is_array())  {
    hasDistortion = true;
    size = dist.array_items().size();
    printf("distortion size %lu\n", size);
    if (size > sizeof(info.distortion.data)) {
      size = sizeof(info.distortion.data);
    }
    for(size_t i = 0; i < size; i++) {
      info.distortion.data[i] = dist[i].number_value();
    }
  }
  auto width = json[width_key];
  if(!width.is_number()) {
    printf("width is not number type!\n");
    return -1;
  }
  //is integer, use int_value
  info.intrinsicWidth = width.int_value();
  auto height = json[height_key];
  if(!height.is_number()) {
    printf("height is not number type!\n");
    return -1;
  }
  info.intrinsicHeight = height.int_value();
  return 0;
}

CalibInfoPtr CalibInfoToJson::JsonStringToCalibInfo(const std::string json_str)
{
  std::string err;
  const auto json = Json::parse(json_str, err);
  if (!err.empty()) {
      LOGD("parse err %s", err.c_str());
      return CalibInfoPtr();
  }
  CalibInfoPtr calib = CalibInfoPtr(new CalibInfo());
  auto str_json = json[sn_key];
  if(!str_json.is_string()) {
    printf("missing sn or sn format err!");
  } else {
    calib->setSN(str_json.string_value());
  }
  str_json = json[ts_key];
  if(!str_json.is_string()) {
    printf("missing Timestamp or Timestamp format err!");
  } else {
    calib->setTimeStamp(str_json.string_value());
  }
  auto dep = json[dep_key];
  if (dep.is_null()) {
    printf("Missing depth info!\n");
    return CalibInfoPtr();
  }
  TY_CAMERA_CALIB_INFO info;
  bool hasDist = false;
  float scale = 1.0f;
  if (CalibInfoToJson::parseDepJson(dep, info, hasDist, scale) < 0) {
    printf("depth info parse err!\n");
    return CalibInfoPtr();
  }
  calib->setDepCalib(info);
  calib->setHasDepDistortion(hasDist);
  calib->setScaleUnit(scale);
  auto rgb = json[rgb_key];
  if (rgb.is_null()) {
    printf("No color info!\n");
    calib->setHasRGB(false);
    return calib;
  }
  calib->setHasRGB(true);
  if (CalibInfoToJson::parseRGBJson(rgb, info) < 0) {
    printf("color info parse err!\n");
    return CalibInfoPtr();
  }
  calib->setRGBCalib(info);
  return calib;
}
