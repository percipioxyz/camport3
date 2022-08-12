/**@file TYImageProc.h
 * @breif Image post-process API
 * @copyright  Copyright(C)2016-2018 Percipio All Rights Reserved
 **/

#ifndef TY_IMAGE_PROC_H_
#define TY_IMAGE_PROC_H_


#include "TYApi.h"
#include "TYCoordinateMapper.h"
#include "TyIsp.h"

/// @brief Image processing acceleration switch
/// @param  [in] en          Enable image process acceleration switch
TY_CAPI TYImageProcesAcceEnable(bool en);

/// @brief Do image undistortion, only support TY_PIXEL_FORMAT_MONO ,TY_PIXEL_FORMAT_RGB,TY_PIXEL_FORMAT_BGR.
/// @param  [in]  srcCalibInfo          Image calibration data.
/// @param  [in]  srcImage              Source image.
/// @param  [in]  cameraNewIntrinsic    Expected new image intrinsic, will use srcCalibInfo for new image intrinsic if set to NULL.
/// @param  [out] dstImage              Output image.
/// @retval TY_STATUS_OK        Succeed.
/// @retval TY_STATUS_NULL_POINTER      Any srcCalibInfo, srcImage, dstImage, srcImage->buffer, dstImage->buffer is NULL.
/// @retval TY_STATUS_INVALID_PARAMETER Invalid srcImage->width, srcImage->height, dstImage->width, dstImage->height or unsupported pixel format.
TY_CAPI TYUndistortImage (const TY_CAMERA_CALIB_INFO *srcCalibInfo
        , const TY_IMAGE_DATA *srcImage
        , const TY_CAMERA_INTRINSIC *cameraNewIntrinsic
        , TY_IMAGE_DATA *dstImage
        );


// -----------------------------------------------------------
struct DepthSpeckleFilterParameters {
    int max_speckle_size; // blob size smaller than this will be removed
    int max_speckle_diff; // Maximum difference between neighbor disparity pixels
};

///<default parameter value definition
#define DepthSpeckleFilterParameters_Initializer {150, 64}

/// @brief Remove speckles on depth image.
/// @param  [in,out]  depthImage        Depth image to be processed.
/// @param  [in]  param                 Algorithm parameters.
/// @retval TY_STATUS_OK        Succeed.
/// @retval TY_STATUS_NULL_POINTER      Any depth, param or depth->buffer is NULL.
/// @retval TY_STATUS_INVALID_PARAMETER param->max_speckle_size <= 0 or param->max_speckle_diff <= 0
TY_CAPI TYDepthSpeckleFilter (TY_IMAGE_DATA* depthImage
        , const DepthSpeckleFilterParameters* param
        );


// -----------------------------------------------------------
struct DepthEnhenceParameters{
    float sigma_s;          ///< filter param on space
    float sigma_r;          ///< filter param on range
    int   outlier_win_sz;   ///< outlier filter windows ize
    float outlier_rate;
};

///<default parameter value definition
#define DepthEnhenceParameters_Initializer {10, 20, 10, 0.1f}

/// @brief Remove speckles on depth image.
/// @param  [in]  depthImage            Pointer to depth image array.
/// @param  [in]  imageNum              Depth image array size.
/// @param  [in,out]  guide             Guide image.
/// @param  [out] output                Output depth image.
/// @param  [in]  param                 Algorithm parameters.
/// @retval TY_STATUS_OK        Succeed.
/// @retval TY_STATUS_NULL_POINTER      Any depthImage, param, output or output->buffer is NULL.
/// @retval TY_STATUS_INVALID_PARAMETER imageNum >= 5 or imageNum <= 0, or any image invalid
/// @retval TY_STATUS_OUT_OF_MEMORY     Output image not suitable.
TY_CAPI TYDepthEnhenceFilter (const TY_IMAGE_DATA* depthImages
        , int imageNum
        , TY_IMAGE_DATA *guide
        , TY_IMAGE_DATA *output
        , const DepthEnhenceParameters* param
        );


#endif
