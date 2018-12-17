#ifndef TY_COLOR_ISP_H_
#define TY_COLOR_ISP_H_
#include "TYApi.h"

#define TYISP_CAPI  TY_CAPI

typedef void* TY_ISP_HANDLE;

typedef enum{
    TY_ISP_FEATURE_CAM_MODEL         = 0x0001,
    TY_ISP_FEATURE_IMAGE_SIZE        = 0x0002, ///<image size width&height
    TY_ISP_FEATURE_WHITEBALANCE_GAIN = 0x0003,
    TY_ISP_FEATURE_AUTO_WHITEBALANCE     ,
    TY_ISP_FEATURE_SHADING               ,
    TY_ISP_FEATURE_SHADING_CENTER        ,
    TY_ISP_FEATURE_INPUT_RESAMPLE_SCALE  , 
    TY_ISP_FEATURE_BLACK_LEVEL           ,
    TY_ISP_FEATURE_BLACK_LEVEL_GAIN      ,
    TY_ISP_FEATURE_BAYER_PATTERN         ,
    TY_ISP_FEATURE_DEMOSAIC_METHOD       ,
    TY_ISP_FEATURE_GAMMA                 ,
    TY_ISP_FEATURE_DEFECT_PIXEL_LIST     ,
    TY_ISP_FEATURE_CCM                   ,
    TY_ISP_FEATURE_BRIGHT                ,
    TY_ISP_FEATURE_CONTRAST              ,
    TY_ISP_FEATURE_AUTOBRIGHT            ,
    TY_ISP_FEATURE_DEBUG_LOG             = 0xff000000, ///<display detail log information

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
    TY_ACCESS_MODE mode;
} TY_ISP_FEATURE_INFO;

TYISP_CAPI TYISPCreate(TY_ISP_HANDLE *handle);
TYISP_CAPI TYISPRelease(TY_ISP_HANDLE *handle);

TYISP_CAPI TYISPSetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, const uint8_t *data, int32_t size);
TYISP_CAPI TYISPGetFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, uint8_t *data_buff, int32_t buff_size);
TYISP_CAPI TYISPGetFeatureSize(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id, int32_t *size);

TYISP_CAPI TYISPHasFeature(TY_ISP_HANDLE handle, TY_ISP_FEATURE_ID feature_id);
TYISP_CAPI TYISPGetFeatureInfoList(TY_ISP_HANDLE handle, TY_ISP_FEATURE_INFO *info_buffer, int buffer_size);
TYISP_CAPI TYISPGetFeatureInfoListSize(TY_ISP_HANDLE handle, int32_t *buffer_size);

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

