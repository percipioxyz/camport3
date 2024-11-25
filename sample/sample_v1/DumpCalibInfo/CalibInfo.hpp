/************************************************
 * @File Name:       sample/DumpCalibInfo/CalibInfo.hpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-11 17:12:28
 * @Modified Time:   2024-07-18 16:36:58
 ***********************************************/
#pragma once
#include <string>
#include <memory>
#include "TYApi.h"

class CalibInfo {
public:
  //
  CalibInfo();
  ~CalibInfo();

  void setScaleUnit(float scale_unit);
  void setDepCalib(struct TY_CAMERA_CALIB_INFO &calib);
  void setRGBCalib(struct TY_CAMERA_CALIB_INFO &calib);
  void setHasDepDistortion(bool has);
  void setHasRGB(bool has);
  void setSN(const std::string &sn);
  void setTimeStamp(const std::string &ts);

  float getScaleUnit() const;
  const struct TY_CAMERA_CALIB_INFO &getDepCalib() const;
  const struct TY_CAMERA_CALIB_INFO &getRGBCalib() const;
  bool hasDepDistortion() const;
  bool hasRGB() const;
  const std::string &getSN() const;
  const std::string &getTimeStamp() const;

private:
  TY_CAMERA_CALIB_INFO depCalib;
  TY_CAMERA_CALIB_INFO RGBCalib;
  float depScaleUnit;
  bool _hasRGB;
  bool _hasDepDistortion;
  std::string sn;
  std::string timestamp;
};
typedef std::shared_ptr<CalibInfo>  CalibInfoPtr;
