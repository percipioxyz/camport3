#include "../common/common.hpp"
#include <fstream>


void dumpFeature(TY_DEV_HANDLE handle, TY_COMPONENT_ID compID, TY_FEATURE_ID featID, const char* name)
{
    TY_FEATURE_INFO featInfo;
    if (TYGetFeatureInfo(handle, compID, featID, &featInfo) != TY_STATUS_OK) {
        return;
    }

    if(featInfo.isValid && (featInfo.accessMode&TY_ACCESS_READABLE)){
        LOGD("===         %s: comp(0x%x) feat(0x%x) name(%s) access(%d) bindComponent(0x%x) bindFeature(0x%x)"
                , name, featInfo.componentID, featInfo.featureID, featInfo.name
                , featInfo.accessMode, featInfo.bindComponentID, featInfo.bindFeatureID);

        if(TYFeatureType(featID) == TY_FEATURE_INT){
            int32_t  n;
            ASSERT_OK(TYGetInt(handle, compID, featID, &n));
            LOGD("===         %14s: %d", "", n);
        }
        if(TYFeatureType(featID) == TY_FEATURE_FLOAT){
            float v;
            ASSERT_OK(TYGetFloat(handle, compID, featID, &v));
            LOGD("===         %14s: %f", "", v);
        }
        if(TYFeatureType(featID) == TY_FEATURE_ENUM){
            uint32_t n;
            ASSERT_OK(TYGetEnumEntryCount(handle, compID, featID, &n));
            LOGD("===         %14s: entry count %d", "", n);
            if(n > 0){
                std::vector<TY_ENUM_ENTRY> entry(n);
                ASSERT_OK(TYGetEnumEntryInfo(handle, compID, featID, &entry[0], n, &n));
                for(uint32_t i = 0; i < n; i++){
                    LOGD("===         %14s:     value(%d), desc(%s)", "", entry[i].value, entry[i].description);
                }
            }
        }
        if(TYFeatureType(featID) == TY_FEATURE_BOOL){
            bool v;
            ASSERT_OK(TYGetBool(handle, compID, featID, &v));
            LOGD("===         %14s: %d", "", v);
        }
        if(TYFeatureType(featID) == TY_FEATURE_STRING){
            uint32_t n;
            ASSERT_OK(TYGetStringLength(handle, compID, featID, &n));
            LOGD("===         %14s: length(%d)", "", n);
            std::vector<char> str(n);
            ASSERT_OK(TYGetString(handle, compID, featID, &str[0], str.size()));
            LOGD("===         %14s: content(%s)", "", &str[0]);
        }
        if(TYFeatureType(featID) == TY_FEATURE_STRUCT){
            switch(featID){
                case TY_STRUCT_CAM_INTRINSIC:{
                    TY_CAMERA_INTRINSIC intri;
                    ASSERT_OK(TYGetStruct(handle, compID, featID, &intri
                                , sizeof(TY_CAMERA_INTRINSIC)));
                    LOGD("===%23s%f %f %f", "", intri.data[0], intri.data[1], intri.data[2]);
                    LOGD("===%23s%f %f %f", "", intri.data[3], intri.data[4], intri.data[5]);
                    LOGD("===%23s%f %f %f", "", intri.data[6], intri.data[7], intri.data[8]);
                    return;
                }
                case TY_STRUCT_EXTRINSIC_TO_DEPTH: {
                    TY_CAMERA_EXTRINSIC extri;
                    ASSERT_OK(TYGetStruct(handle, compID, featID, &extri
                                , sizeof(TY_CAMERA_EXTRINSIC)));
                    LOGD("===%23s%f %f %f %f", "", extri.data[0], extri.data[1], extri.data[2], extri.data[3]);
                    LOGD("===%23s%f %f %f %f", "", extri.data[4], extri.data[5], extri.data[6], extri.data[7]);
                    LOGD("===%23s%f %f %f %f", "", extri.data[8], extri.data[9], extri.data[10], extri.data[11]);
                    LOGD("===%23s%f %f %f %f", "", extri.data[12], extri.data[13], extri.data[14], extri.data[15]);
                    return;
                }
                case TY_STRUCT_CAM_DISTORTION:{
                    TY_CAMERA_DISTORTION dist;
                    ASSERT_OK(TYGetStruct(handle, compID, featID, &dist
                                , sizeof(TY_CAMERA_DISTORTION)));
                    LOGD("===%23s%f %f %f %f", "", dist.data[0], dist.data[1], dist.data[2], dist.data[3]);
                    LOGD("===%23s%f %f %f %f", "", dist.data[4], dist.data[5], dist.data[6], dist.data[7]);
                    LOGD("===%23s%f %f %f %f", "", dist.data[8], dist.data[9], dist.data[10], dist.data[11]);
                    return;
                }
                default:
                    LOGD("===         %s: Unknown struct", name);
                    return;
            }
        }
    }
}

#define DUMP_FEATURE(handle, compid, feature)  dumpFeature( (handle), (compid), (feature) , #feature );

void dumpComponentFeatures(TY_DEV_HANDLE handle, TY_COMPONENT_ID compID)
{
    DUMP_FEATURE(handle, compID, TY_STRUCT_CAM_INTRINSIC );
    DUMP_FEATURE(handle, compID, TY_STRUCT_EXTRINSIC_TO_DEPTH);
    DUMP_FEATURE(handle, compID, TY_STRUCT_CAM_DISTORTION);

    DUMP_FEATURE(handle, compID, TY_INT_PERSISTENT_IP);
    DUMP_FEATURE(handle, compID, TY_INT_PERSISTENT_SUBMASK);
    DUMP_FEATURE(handle, compID, TY_INT_PERSISTENT_GATEWAY);
    DUMP_FEATURE(handle, compID, TY_BOOL_GVSP_RESEND);
    DUMP_FEATURE(handle, compID, TY_INT_PACKET_DELAY);

    DUMP_FEATURE(handle, compID, TY_INT_WIDTH_MAX);
    DUMP_FEATURE(handle, compID, TY_INT_HEIGHT_MAX);
    DUMP_FEATURE(handle, compID, TY_INT_OFFSET_X);
    DUMP_FEATURE(handle, compID, TY_INT_OFFSET_Y);
    DUMP_FEATURE(handle, compID, TY_INT_WIDTH);
    DUMP_FEATURE(handle, compID, TY_INT_HEIGHT);
    DUMP_FEATURE(handle, compID, TY_ENUM_IMAGE_MODE);
                                                                
    DUMP_FEATURE(handle, compID, TY_ENUM_TRIGGER_POL);
    DUMP_FEATURE(handle, compID, TY_INT_FRAME_PER_TRIGGER);
    DUMP_FEATURE(handle, compID, TY_BOOL_KEEP_ALIVE_ONOFF);
    DUMP_FEATURE(handle, compID, TY_INT_KEEP_ALIVE_TIMEOUT);
                                                                
    DUMP_FEATURE(handle, compID, TY_BOOL_AUTO_EXPOSURE);
    DUMP_FEATURE(handle, compID, TY_INT_EXPOSURE_TIME);
    DUMP_FEATURE(handle, compID, TY_BOOL_AUTO_GAIN);
    DUMP_FEATURE(handle, compID, TY_INT_GAIN);
    DUMP_FEATURE(handle, compID, TY_BOOL_AUTO_AWB);
                                                                
    DUMP_FEATURE(handle, compID, TY_INT_LASER_POWER);
    DUMP_FEATURE(handle, compID, TY_BOOL_LASER_AUTO_CTRL);

    DUMP_FEATURE(handle, compID, TY_BOOL_UNDISTORTION);
    DUMP_FEATURE(handle, compID, TY_BOOL_BRIGHTNESS_HISTOGRAM);
    DUMP_FEATURE(handle, compID, TY_BOOL_DEPTH_POSTPROC);

    DUMP_FEATURE(handle, compID, TY_INT_R_GAIN);
    DUMP_FEATURE(handle, compID, TY_INT_G_GAIN);
    DUMP_FEATURE(handle, compID, TY_INT_B_GAIN);

    DUMP_FEATURE(handle, compID, TY_INT_ANALOG_GAIN);

    DUMP_FEATURE(handle, compID, TY_INT_ACCEPTABLE_PERCENT);
    DUMP_FEATURE(handle, compID, TY_INT_NTP_SERVER_IP);
}

#define DUMP_COMPONENT(handle,compIds,id) \
  do{\
    if(compIds & id){\
      LOGD("===  %s:",#id);\
      dumpComponentFeatures(handle, id);\
    }\
  }while(0)

void dumpAllComponentFeatures(TY_DEV_HANDLE handle, int32_t compIDs)
{
    LOGD("=== Dump all components and features:");
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_DEVICE);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_DEPTH_CAM);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_IR_CAM_LEFT);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_IR_CAM_RIGHT);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_RGB_CAM_LEFT);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_RGB_CAM_RIGHT);
    DUMP_COMPONENT(handle, compIDs, TY_COMPONENT_LASER);
}

int main(int argc, char* argv[])
{
    bool dumpConfig = false;
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface;
    TY_DEV_HANDLE handle;
    int i = 0;

    for(i = 1; i < argc; i++){
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        }else if(strcmp(argv[i], "-d") == 0){
            dumpConfig = true;
        }else if(strcmp(argv[i], "-h") == 0){
            LOGI("Usage: DumpAllFeatures [-h] [-id <ID>]");
            return 0;
        }
    }

    if (dumpConfig) {
        std::ofstream fout("fetch_config.xml");  
        if (fout) {
            LOGD("Create fetch_config.xml successfully");
        } else {
            LOGD("Create fetch_config.xml failed");
        }
    }

    // Init lib
    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("=== lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &handle) );
    ASSERT_OK( TYGetDeviceInfo(handle, &selectedDev) );
    {
        LOGD("===   selected interface %s:", selectedDev.iface.id);
        LOGD("===   selected device %s:", selectedDev.id);
        LOGD("===       vendor     : %s", selectedDev.vendorName);
        LOGD("===       model      : %s", selectedDev.modelName);
        LOGD("===       HW version : %d.%d.%d"
                , selectedDev.hardwareVersion.major
                , selectedDev.hardwareVersion.minor
                , selectedDev.hardwareVersion.patch
                );
        LOGD("===       FW version : %d.%d.%d"
                , selectedDev.firmwareVersion.major
                , selectedDev.firmwareVersion.minor
                , selectedDev.firmwareVersion.patch
                );
        if (TYIsNetworkInterface(selectedDev.iface.type)) {
            LOGD("===       device MAC : %s", selectedDev.netInfo.mac);
            LOGD("===       device IP  : %s", selectedDev.netInfo.ip);
        }
    }

    {
        // List all components
        int32_t compIDs;
        std::string compNames;
        ASSERT_OK(TYGetComponentIDs(handle, &compIDs));
        dumpAllComponentFeatures(handle, compIDs);

    }
    LOGD("=== Close device");
    ASSERT_OK(TYCloseDevice(handle));
    ASSERT_OK(TYCloseInterface(hIface));
    ASSERT_OK(TYDeinitLib());
    LOGD("Done!");
    return 0;
}
