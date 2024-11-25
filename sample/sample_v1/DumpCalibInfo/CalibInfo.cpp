/************************************************
 * @File Name:       sample/DumpCalibInfo/CalibInfo.cpp
 * @Author:          Leon Zhou
 * @Mail:            <leonzhou@percipio.xyz>
 * @Created Time:    2024-07-15 17:12:26
 * @Modified Time:   2024-07-18 17:39:57
 ***********************************************/
#include "Utils.hpp"
#include "CalibInfo.hpp"
#include <cstring>
CalibInfo::CalibInfo()
{
  //default is 1.0 if camera Do not contain this feature
  depScaleUnit = 1.0;
  _hasRGB = false;
  _hasDepDistortion = false;
  memset(&depCalib, 0, sizeof(depCalib));
  memset(&RGBCalib, 0, sizeof(RGBCalib));
  //default is Unknow, override by setSN
  sn="Unknown SN";
  //default ts set when construct, can modify by setTimestamp
  timestamp = getLocalTime();
}

CalibInfo::~CalibInfo()
{
}

void CalibInfo::setScaleUnit(float scale_unit)
{
  depScaleUnit = scale_unit;
}
void CalibInfo::setDepCalib(TY_CAMERA_CALIB_INFO &calib)
{
  memcpy(&depCalib, &calib, sizeof(depCalib));
}
void CalibInfo::setRGBCalib(TY_CAMERA_CALIB_INFO &calib)
{
  memcpy(&RGBCalib, &calib, sizeof(depCalib));
}
void CalibInfo::setHasDepDistortion(bool has)
{
  _hasDepDistortion = has;
}
void CalibInfo::setHasRGB(bool has)
{
  _hasRGB = has;
}
void CalibInfo::setSN(const std::string &SN)
{
    sn = SN;
}
void CalibInfo::setTimeStamp(const std::string &ts)
{
  timestamp = ts;
}


float CalibInfo::getScaleUnit() const
{
  return depScaleUnit;
}
const TY_CAMERA_CALIB_INFO &CalibInfo::getDepCalib() const
{
  return depCalib;
}
const TY_CAMERA_CALIB_INFO &CalibInfo::getRGBCalib() const
{
  return RGBCalib;
}
bool CalibInfo::hasDepDistortion() const
{
  return _hasDepDistortion;
}
bool CalibInfo::hasRGB() const
{
  return _hasRGB;
}

const std::string &CalibInfo::getSN() const
{
  return sn;
}
const std::string &CalibInfo::getTimeStamp() const
{
  return timestamp;
}
