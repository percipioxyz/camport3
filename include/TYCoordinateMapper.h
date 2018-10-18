#ifndef TY_COORDINATE_MAPPER_H_
#define TY_COORDINATE_MAPPER_H_


#include <stdlib.h>
#include "TYApi.h"


// ------------------------------------------------------------------
//  NOTE: Considering performance, we leave the responsibility of
//        parameters check to users.
// ------------------------------------------------------------------


typedef struct TY_PIXEL_DESC
{
  int16_t x;      // x coordinate in pixels
  int16_t y;      // y coordinate in pixels
  uint16_t depth; // depth value
  uint16_t rsvd;
}TY_PIXEL_DESC;


// ------------------------------
//  base convertion
// ------------------------------

/// @brief Calculate 4x4 extrinsic matrix's inverse matrix.
/// @param  [in]  orgExtrinsic          Input extrinsic matrix.
/// @param  [out] invExtrinsic          Inverse matrix.
/// @retval TY_STATUS_OK        Succeed.
/// @retval TY_STATUS_ERROR     Calculation failed.
TY_CAPI   TYInvertExtrinsic         (const TY_CAMERA_EXTRINSIC* orgExtrinsic,
                                     TY_CAMERA_EXTRINSIC* invExtrinsic);

/// @brief Map pixels on depth image to 3D points.
/// @param  [in]  src_calib             Depth image's calibration data.
/// @param  [in]  depthW                Width of depth image.
/// @param  [in]  depthH                Height of depth image.
/// @param  [in]  depthPixels           Pixels on depth image.
/// @param  [in]  count                 Number of depth pixels.
/// @param  [out] point3d               Output point3D.
/// @retval TY_STATUS_OK        Succeed.
TY_CAPI   TYMapDepthToPoint3d       (const TY_CAMERA_CALIB_INFO* src_calib,
                                     uint32_t depthW, uint32_t depthH,
                                     const TY_PIXEL_DESC* depthPixels, uint32_t count,
                                     TY_VECT_3F* point3d);

/// @brief Map 3D points to pixels on depth image. Reverse operation of TYMapDepthToPoint3d.
/// @param  [in]  dst_calib             Target depth image's calibration data.
/// @param  [in]  point3d               Input 3D points.
/// @param  [in]  count                 Number of points.
/// @param  [in]  depthW                Width of target depth image.
/// @param  [in]  depthH                Height of target depth image.
/// @param  [out] depth                 Output depth pixels.
/// @retval TY_STATUS_OK        Succeed.
TY_CAPI   TYMapPoint3dToDepth       (const TY_CAMERA_CALIB_INFO* dst_calib,
                                     const TY_VECT_3F* point3d, uint32_t count,
                                     uint32_t depthW, uint32_t depthH,
                                     TY_PIXEL_DESC* depth);

/// @brief Map depth image to 3D points. 0 depth pixels maps to (NAN, NAN, NAN).
/// @param  [in]  src_calib             Depth image's calibration data.
/// @param  [in]  depthW                Width of depth image.
/// @param  [in]  depthH                Height of depth image.
/// @param  [in]  depth                 Depth image.
/// @param  [out] point3d               Output point3D image.
/// @retval TY_STATUS_OK        Succeed.
TY_CAPI   TYMapDepthImageToPoint3d  (const TY_CAMERA_CALIB_INFO* src_calib,
                                     uint32_t imageW, uint32_t imageH,
                                     const uint16_t* depth,
                                     TY_VECT_3F* point3d);

/// @brief Map 3D points to depth image. (NAN, NAN, NAN) will be skipped.
/// @param  [in]  dst_calib             Target depth image's calibration data.
/// @param  [in]  point3d               Input 3D points.
/// @param  [in]  count                 Number of points.
/// @param  [in]  depthW                Width of target depth image.
/// @param  [in]  depthH                Height of target depth image.
/// @param  [in,out] depth              Depth image buffer.
/// @retval TY_STATUS_OK        Succeed.
TY_CAPI   TYMapPoint3dToDepthImage  (const TY_CAMERA_CALIB_INFO* dst_calib,
                                     const TY_VECT_3F* point3d, uint32_t count,
                                     uint32_t depthW, uint32_t depthH, uint16_t* depth);

/// @brief Map 3D points to another coordinate.
/// @param  [in]  extrinsic             Extrinsic matrix.
/// @param  [in]  point3dFrom           Source 3D points.
/// @param  [in]  count                 Number of source 3D points.
/// @param  [out] point3dTo             Target 3D points.
/// @retval TY_STATUS_OK        Succeed.
TY_CAPI   TYMapPoint3dToPoint3d     (const TY_CAMERA_EXTRINSIC* extrinsic,
                                     const TY_VECT_3F* point3dFrom, uint32_t count,
                                     TY_VECT_3F* point3dTo);

// ------------------------------
//  inlines
// ------------------------------

/// @brief Map depth pixels to color coordinate pixels.
/// @param  [in]  depth_calib           Depth image's calibration data.
/// @param  [in]  depthW                Width of current depth image.
/// @param  [in]  depthH                Height of current depth image.
/// @param  [in]  depth                 Depth image pixels.
/// @param  [in]  count                 Number of depth image pixels.
/// @param  [in]  color_calib           Color image's calibration data.
/// @param  [in]  mappedW               Width of target depth image.
/// @param  [in]  mappedH               Height of target depth image.
/// @param  [out] mappedDepth           Output pixels.
/// @retval TY_STATUS_OK        Succeed.
static inline TY_STATUS TYMapDepthToColorCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH,
                  const TY_PIXEL_DESC* depth, uint32_t count,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH,
                  TY_PIXEL_DESC* mappedDepth);

/// @brief Map original depth image to color coordinate depth image.
/// @param  [in]  depth_calib           Depth image's calibration data.
/// @param  [in]  depthW                Width of current depth image.
/// @param  [in]  depthH                Height of current depth image.
/// @param  [in]  depth                 Depth image.
/// @param  [in]  color_calib           Color image's calibration data.
/// @param  [in]  mappedW               Width of target depth image.
/// @param  [in]  mappedH               Height of target depth image.
/// @param  [out] mappedDepth           Output pixels.
/// @retval TY_STATUS_OK        Succeed.
static inline TY_STATUS TYMapDepthImageToColorCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH, uint16_t* mappedDepth);

/// @brief Create depth image to color coordinate lookup table.
/// @param  [in]  depth_calib           Depth image's calibration data.
/// @param  [in]  depthW                Width of current depth image.
/// @param  [in]  depthH                Height of current depth image.
/// @param  [in]  depth                 Depth image.
/// @param  [in]  color_calib           Color image's calibration data.
/// @param  [in]  mappedW               Width of target depth image.
/// @param  [in]  mappedH               Height of target depth image.
/// @param  [out] lut                   Output lookup table.
/// @retval TY_STATUS_OK        Succeed.
static inline TY_STATUS TYCreateDepthToColorCoordinateLookupTable(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH,
                  TY_PIXEL_DESC* lut);

/// @brief Map original RGB image to depth coordinate RGB image.
/// @param  [in]  depth_calib           Depth image's calibration data.
/// @param  [in]  depthW                Width of current depth image.
/// @param  [in]  depthH                Height of current depth image.
/// @param  [in]  depth                 Current depth image.
/// @param  [in]  color_calib           Color image's calibration data.
/// @param  [in]  rgbW                  Width of RGB image.
/// @param  [in]  rgbH                  Height of RGB image.
/// @param  [in]  inRgb                 Current RGB image.
/// @param  [out] mappedRgb             Output RGB image.
/// @retval TY_STATUS_OK        Succeed.
static inline TY_STATUS TYMapRGBImageToDepthCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t rgbW, uint32_t rgbH, const uint8_t* inRgb,
                  uint8_t* mappedRgb);

/// @brief Map original MONO8 image to depth coordinate MONO8 image.
/// @param  [in]  depth_calib           Depth image's calibration data.
/// @param  [in]  depthW                Width of current depth image.
/// @param  [in]  depthH                Height of current depth image.
/// @param  [in]  depth                 Current depth image.
/// @param  [in]  color_calib           Color image's calibration data.
/// @param  [in]  monoW                 Width of MONO8 image.
/// @param  [in]  monoH                 Height of MONO8 image.
/// @param  [in]  inMono                Current MONO8 image.
/// @param  [out] mappedMono            Output MONO8 image.
/// @retval TY_STATUS_OK        Succeed.
static inline TY_STATUS TYMapMono8ImageToDepthCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t monoW, uint32_t monoH, const uint8_t* inMono,
                  uint8_t* mappedMono);


#define TYMAP_CHECKRET(f, bufToFree) \
  do{ \
    TY_STATUS err = (f); \
    if(err){ \
      if(bufToFree) \
        free(bufToFree); \
      return err; \
    } \
  } while(0)
  

static inline TY_STATUS TYMapDepthToColorCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH,
                  const TY_PIXEL_DESC* depth, uint32_t count,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH,
                  TY_PIXEL_DESC* mappedDepth)
{
  TY_VECT_3F* p3d = (TY_VECT_3F*)malloc(sizeof(TY_VECT_3F) * count);
  TYMAP_CHECKRET(TYMapDepthToPoint3d(depth_calib, depthW, depthH, depth, count, p3d), p3d );
  TY_CAMERA_EXTRINSIC extri_inv;
  TYMAP_CHECKRET(TYInvertExtrinsic(&color_calib->extrinsic, &extri_inv), p3d);
  TYMAP_CHECKRET(TYMapPoint3dToPoint3d(&extri_inv, p3d, count, p3d), p3d );
  TYMAP_CHECKRET(TYMapPoint3dToDepth(color_calib, p3d, count, mappedW, mappedH, mappedDepth), p3d );
  free(p3d);
  return TY_STATUS_OK;
}


static inline TY_STATUS TYMapDepthImageToColorCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH, uint16_t* mappedDepth)
{
  TY_VECT_3F* p3d = (TY_VECT_3F*)malloc(sizeof(TY_VECT_3F) * depthW * depthH);
  TYMAP_CHECKRET(TYMapDepthImageToPoint3d(depth_calib, depthW, depthH, depth, p3d), p3d);
  TY_CAMERA_EXTRINSIC extri_inv;
  TYMAP_CHECKRET(TYInvertExtrinsic(&color_calib->extrinsic, &extri_inv), p3d);
  TYMAP_CHECKRET(TYMapPoint3dToPoint3d(&extri_inv, p3d, depthW * depthH, p3d), p3d);
  TYMAP_CHECKRET(TYMapPoint3dToDepthImage(
        color_calib, p3d, depthW * depthH,  mappedW, mappedH, mappedDepth), p3d);
  free(p3d);
  return TY_STATUS_OK;
}


static inline TY_STATUS TYCreateDepthToColorCoordinateLookupTable(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t mappedW, uint32_t mappedH,
                  TY_PIXEL_DESC* lut)
{
  TY_VECT_3F* p3d = (TY_VECT_3F*)malloc(sizeof(TY_VECT_3F) * depthW * depthH);
  TYMAP_CHECKRET(TYMapDepthImageToPoint3d(depth_calib, depthW, depthH, depth, p3d), p3d);
  TY_CAMERA_EXTRINSIC extri_inv;
  TYMAP_CHECKRET(TYInvertExtrinsic(&color_calib->extrinsic, &extri_inv), p3d);
  TYMAP_CHECKRET(TYMapPoint3dToPoint3d(&extri_inv, p3d, depthW * depthH, p3d), p3d);
  TYMAP_CHECKRET(TYMapPoint3dToDepth(color_calib, p3d, depthW * depthH, mappedW, mappedH, lut), p3d );
  free(p3d);
  return TY_STATUS_OK;
}


static inline TY_STATUS TYMapRGBImageToDepthCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t rgbW, uint32_t rgbH, const uint8_t* inRgb,
                  uint8_t* mappedRgb)
{
  TY_PIXEL_DESC* lut = (TY_PIXEL_DESC*)malloc(sizeof(TY_PIXEL_DESC) * depthW * depthH*4);
  TYMAP_CHECKRET(TYCreateDepthToColorCoordinateLookupTable(
                    depth_calib, depthW, depthH, depth,
                    color_calib, rgbW, rgbH, lut), lut);
  for(uint32_t depthr = 0; depthr < depthH; depthr++)
  for(uint32_t depthc = 0; depthc < depthW; depthc++)
  {
    TY_PIXEL_DESC* plut = &lut[depthr * depthW + depthc];
    uint8_t* outPtr = &mappedRgb[depthW * depthr * 3 + depthc * 3];
    if(plut->x < 0 || plut->x >= (int)rgbW || plut->y < 0 || plut->y >= (int)rgbH){
      outPtr[0] = outPtr[1] = outPtr[2] = 0;
    } else {
      const uint8_t* inPtr = &inRgb[rgbW * plut->y * 3 + plut->x * 3];
      outPtr[0] = inPtr[0];
      outPtr[1] = inPtr[1];
      outPtr[2] = inPtr[2];
    }
  }
  free(lut);
  return TY_STATUS_OK;
}


static inline TY_STATUS TYMapMono8ImageToDepthCoordinate(
                  const TY_CAMERA_CALIB_INFO* depth_calib,
                  uint32_t depthW, uint32_t depthH, const uint16_t* depth,
                  const TY_CAMERA_CALIB_INFO* color_calib,
                  uint32_t monoW, uint32_t monoH, const uint8_t* inMono,
                  uint8_t* mappedMono)
{
  TY_PIXEL_DESC* lut = (TY_PIXEL_DESC*)malloc(sizeof(TY_PIXEL_DESC) * depthW * depthH);
  TYMAP_CHECKRET(TYCreateDepthToColorCoordinateLookupTable(
                    depth_calib, depthW, depthH, depth,
                    color_calib, monoW, monoH, lut), lut);
  for(uint32_t depthr = 0; depthr < depthH; depthr++)
  for(uint32_t depthc = 0; depthc < depthW; depthc++)
  {
    TY_PIXEL_DESC* plut = &lut[depthr * depthW + depthc];
    uint8_t* outPtr = &mappedMono[depthW * depthr + depthc];
    if(plut->x < 0 || plut->x >= (int)monoW || plut->y < 0 || plut->y >= (int)monoH){
      outPtr[0] = 0;
    } else {
      const uint8_t* inPtr = &inMono[monoW * plut->y + plut->x];
      outPtr[0] = inPtr[0];
    }
  }
  free(lut);
  return TY_STATUS_OK;
}


#endif
