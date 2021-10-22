/**@file TyIsp.h
 * @breif this file Include interface declare for  raw color image (bayer format)
 * process functions
 *
 * Copyright(C)2016-2019 Percipio All Rights Reserved
 *  
 */

#ifndef TY_COLOR_ISP_H_
#define TY_COLOR_ISP_H_
#include "TYApi.h"

#define TYISP_CAPI  TY_CAPI

typedef void* TY_ISP_HANDLE;

typedef enum{
    TY_ISP_FEATURE_CAM_MODEL                        = 0x000000,
    TY_ISP_FEATURE_CAM_DEV_HANDLE                   = 0x000001, ///<device handle for device control
    TY_ISP_FEATURE_CAM_DEV_COMPONENT                = 0x000002, ///<the component to control
    TY_ISP_FEATURE_IMAGE_SIZE                       = 0x000100, ///<image size width&height
    TY_ISP_FEATURE_WHITEBALANCE_GAIN                = 0x000200,
    TY_ISP_FEATURE_ENABLE_AUTO_WHITEBALANCE         = 0x000300,
    TY_ISP_FEATURE_SHADING                          = 0x000400,
    TY_ISP_FEATURE_SHADING_CENTER                   = 0x000500,
    TY_ISP_FEATURE_BLACK_LEVEL                      = 0x000600, ///<global black level
    TY_ISP_FEATURE_BLACK_LEVEL_COLUMN               = 0x000610, ///<to set different black level for each image column
    TY_ISP_FEATURE_BLACK_LEVEL_GAIN                 = 0x000700, ///<global pixel gain
    TY_ISP_FEATURE_BLACK_LEVEL_GAIN_COLUMN          = 0x000710, ///<to set different gain for each image column
    TY_ISP_FEATURE_BAYER_PATTERN                    = 0x000800,
    TY_ISP_FEATURE_DEMOSAIC_METHOD                  = 0x000900,
    TY_ISP_FEATURE_GAMMA                            = 0x000A00,
    TY_ISP_FEATURE_DEFECT_PIXEL_LIST                = 0x000B00,
    TY_ISP_FEATURE_CCM                              = 0x000C00,
    TY_ISP_FEATURE_CCM_ENABLE                       = 0x000C10, ///<ENABLE CCM
    TY_ISP_FEATURE_BRIGHT                           = 0x000D00,
    TY_ISP_FEATURE_CONTRAST                         = 0x000E00,
    TY_ISP_FEATURE_AUTOBRIGHT                       = 0x000F00,
    TY_ISP_FEATURE_INPUT_RESAMPLE_SCALE             = 0x001000,  //<set this if bayer image resampled before softisp process.
    TY_ISP_FEATURE_ENABLE_AUTO_EXPOSURE_GAIN        = 0x001100,
    TY_ISP_FEATURE_AUTO_EXPOSURE_RANGE              = 0x001200, ///<exposure range ,default no limit
    TY_ISP_FEATURE_AUTO_GAIN_RANGE                  = 0x001300, ///<gain range ,default no limit
    TY_ISP_FEATURE_AUTO_EXPOSURE_UPDATE_INTERVAL    = 0x001400, ///<update device exposure interval , default 5 frame 
    TY_ISP_FEATURE_DEBUG_LOG                        = 0xff000000, ///<display detail log information

} TY_ISP_FEATURE_ID;

typedef enum{
    TY_ISP_BAYER_GB     = 0,
    TY_ISP_BAYER_BG     = 1,
    TY_ISP_BAYER_RG     = 2,
    TY_ISP_BAYER_GR     = 3,
    TY_ISP_BAYER_AUTO   = 0xff,
}TY_ISP_BAYER_PATTERN;

typedef enum{
    TY_DEMOSAIC_METHOD_SIMPLE       = 0,
    TY_DEMOSAIC_METHOD_BILINEAR     = 1,
    TY_DEMOSAIC_METHOD_HQLINEAR     = 2,
    TY_DEMOSAIC_METHOD_EDGESENSE    = 3,
} TY_DEMOSAIC_METHOD;

typedef struct{
    TY_ISP_FEATURE_ID id;
    int32_t size;
    const char * name;
    const char * value_type;
    TY_ACCESS_MODE mode;
} TY_ISP_FEATURE_INFO;

TYISP_CAPI TYISPCreate(TY_ISP_HANDLE *handle);
TYISP_CAPI TYISPRelease(TY_ISP_HANDLE *handle);
TYISP_CAPI TYISPLoadConfig(TY_ISP_HANDLE handle,const uint8_t *config, uint32_t config_size);
///@breif called by main thread to update & control device status for ISP
TYISP_CAPI TYISPUpdateDevice(TY_ISP_HANDLE handle);

TYISP_CAPI TYISPSetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, const uint8_t *data, int32_t size);
TYISP_CAPI TYISPGetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, uint8_t *data_buff, int32_t buff_size);
TYISP_CAPI TYISPGetFeatureSize(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, int32_t *size);

TYISP_CAPI TYISPHasFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id);
TYISP_CAPI TYISPGetFeatureInfoList(TY_ISP_HANDLE handle, TY_ISP_FEATURE_INFO *info_buffer, int buffer_size);
TYISP_CAPI TYISPGetFeatureInfoListSize(TY_ISP_HANDLE handle, int32_t *buffer_size);
///@breif  convert bayer raw image to rgb image,output buffer is allocated by invoker
TYISP_CAPI TYISPProcessImage(TY_ISP_HANDLE handle,const TY_IMAGE_DATA *image_bayer, TY_IMAGE_DATA *image_out);

#ifdef __cplusplus
static inline TY_STATUS TYISPSetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, int value){
    return TYISPSetFeature(handle, feature_id, (uint8_t*)&(value), sizeof(int));
}


static inline TY_STATUS TYISPGetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, int *value){
    return TYISPGetFeature(handle, feature_id, (uint8_t*)value, sizeof(int));
}

static inline TY_STATUS TYISPSetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, float value){
    return TYISPSetFeature(handle, feature_id, (uint8_t*)&(value), sizeof(float));
}


static inline TY_STATUS TYISPGetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, float *value){
    return TYISPGetFeature(handle, feature_id, (uint8_t*)value, sizeof(float));
}

#endif

#endif

