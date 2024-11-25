#include "Utils.hpp"
#define DO_DEV_NUM                (3)
#define DI_DEV_NUM                (3)
static TY_DO_WORKMODE do_wms[DO_DEV_NUM] = {0};
static TY_DI_WORKMODE di_wms[DI_DEV_NUM] = {0};

static void print_do_info(TY_DO_WORKMODE *do_info)
{
    if(do_info->mode != TY_DO_PWM) {
        LOGD("{ mode:%u, volt:%u, mode_supported:%u, volt_supported:%u",\
            do_info->mode, do_info->volt, do_info->mode_supported,
            do_info->volt_supported);
    } else {
        LOGD("{ mode:%u, volt:%u, freq:%u, duty:%u,"\
            "mode_supported:%u, volt_supported:%u",\
            do_info->mode, do_info->volt, do_info->freq,
            do_info->duty, do_info->mode_supported, do_info->volt_supported);
    }
}

static void print_di_info(TY_DI_WORKMODE *di_info)
{
    if(di_info->mode == TY_DI_POLL) {
        LOGD("{ mode:%u, status: %u, mode_supported:%u, int_act_supported:%u}",\
            di_info->mode, di_info->status, di_info->mode_supported,
            di_info->int_act_supported);
    } else {
        LOGD("{ mode:%u, status: %u, int_act:%u, mode_supported:%u, int_act_supported:%u}",\
            di_info->mode,  di_info->status, di_info->int_act, di_info->mode_supported,
            di_info->int_act_supported);
    }
}


static void enum_di_devs(TY_DEV_HANDLE hDevice)
{
    int i = 0;
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DI0_WORKMODE, &di_wms[0], sizeof(di_wms[0]));
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DI1_WORKMODE, &di_wms[1], sizeof(di_wms[1]));
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DI2_WORKMODE, &di_wms[2], sizeof(di_wms[2]));
    for(i = 0; i < DI_DEV_NUM; i++) {
        LOGD("DI[%d]", i);
        print_di_info(&di_wms[i]);
    }

}

static void enum_do_devs(TY_DEV_HANDLE hDevice)
{
    int i = 0;
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DO0_WORKMODE, &do_wms[0], sizeof(do_wms[0]));
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DO1_WORKMODE, &do_wms[1], sizeof(do_wms[1]));
    TYGetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_DO2_WORKMODE, &do_wms[2], sizeof(do_wms[2]));
    for(i = 0; i < DO_DEV_NUM; i++) {
        LOGD("DO[%d]", i);
        print_do_info(&do_wms[i]);
    }
}

void display_help() {
    printf("    -[do|di] idx : select idx do_dev/di_dev\n");
    printf("    -mode <m>    : mode,  [0,2] for di and [0,3] for do\n");
    printf("                   di: 0 TY_DI_POLL        \n");
    printf("                       1 TY_DI_NE_INT      \n");
    printf("                       2 TY_DI_PE_INT      \n");
    printf("                   do: 0 TY_DO_LOW         \n");
    printf("                       1 TY_DO_HIGH        \n");
    printf("                       2 TY_DO_PWM         \n");
    printf("                       3 TY_DO_CAM_TRIG    \n");
    printf("    -volt <v>    : volt(do Only),  [0,2] \n");
    printf("                   0 TY_EXT_SUPP           \n");
    printf("                   1 TY_DO_5V              \n");
    printf("                   2 TY_DO_12V             \n");
    printf("    -freq <f>    : frequency(do mode=2 Only),  [1,1000]\n");
    printf("    -duty <d>    : duty(do mode=2 Only),  [1,100]\n");
    printf("    -int_act <a> : interrupt action(di mode != 0 Only), [0,2]\n");
    printf("                   0 TY_DI_INT_NO_OP       \n");
    printf("                   1 TY_DI_INT_TRIG_CAP    \n");
    printf("                   2 TY_DI_INT_ENVENT      \n");
}

static void config_do_dev(TY_DEV_HANDLE hDevice, int idx, int mode,
        int volt, int freq, int duty)
{
    TY_DO_WORKMODE do_wm;
    uint32_t feat_id = TY_STRUCT_DO0_WORKMODE;
    if (!((1 << mode) & do_wms[idx].mode_supported)) {
        printf("mode %d not supported for do_dev[%d].mode_suported %u\n",
            mode, idx, do_wms[idx].mode_supported);
        return;
    }
    if (!((1 << volt) & do_wms[idx].volt_supported)) {
        printf("volt %d not supported for do_dev[%d].volt_suported %u\n",
            volt, idx, do_wms[idx].volt_supported);
        return;
    }
    if (mode == TY_DO_PWM) {
        if (freq < 1 || freq > 1000) {
            printf("freq %d out of range [1, 1000]\n", freq);
        }
        if (duty < 1 || duty > 100) {
            printf("duty %d out of range [1, 100]\n", duty);
        }
    }
    if (idx == 1) {
        feat_id = TY_STRUCT_DO1_WORKMODE;
    } else if (idx == 2) {
        feat_id = TY_STRUCT_DO2_WORKMODE;
    }
    do_wm.mode = mode;
    do_wm.volt = volt;
    do_wm.freq = freq;
    do_wm.duty = duty;
    TYSetStruct(hDevice, TY_COMPONENT_DEVICE, feat_id, &do_wm, sizeof(do_wm));
}

static void config_di_dev(TY_DEV_HANDLE hDevice, int idx, int mode, int int_act)
{
    TY_DI_WORKMODE di_wm;
    uint32_t feat_id = TY_STRUCT_DI0_WORKMODE;
    if (!((1 << mode) & di_wms[idx].mode_supported)) {
        printf("mode %d not supported for di_dev[%d].mode_suported %u\n",
            mode, idx, di_wms[idx].mode_supported);
        return;
    }
    if (mode != TY_DI_POLL && !((1 << int_act) & di_wms[idx].int_act_supported)) {
        printf("int_act %d not supported for di_dev[%d].int_act_suported %u\n",
            int_act, idx, di_wms[idx].int_act_supported);
        return;
    }
    if (idx == 1) {
        feat_id = TY_STRUCT_DI1_WORKMODE;
    } else if (idx == 2) {
        feat_id = TY_STRUCT_DI2_WORKMODE;
    }
    di_wm.mode = mode;
    di_wm.int_act = int_act;
    TYSetStruct(hDevice, TY_COMPONENT_DEVICE, feat_id, &di_wm, sizeof(di_wm));

}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int do_di = -1;
    int do_di_idx = -1;
    int mode = -1;
    int volt = -1, freq = -1, duty = -1;
    int int_act = -1;

    for (int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if (strcmp(argv[i], "-do") == 0) {
            do_di = 0;
            do_di_idx = atoi(argv[++i]);
            if(do_di_idx < 0 || do_di_idx > DO_DEV_NUM - 1) {
                printf("do idx %d out of range\n", do_di_idx);
                return -1;
            }
        } else if (strcmp(argv[i], "-di") == 0) {
            do_di = 1;
            do_di_idx = atoi(argv[++i]);
            if(do_di_idx < 0 || do_di_idx > DI_DEV_NUM - 1) {
                printf("di idx %d out of range\n", do_di_idx);
                return -1;
            }
        } else if (strcmp(argv[i], "-mode") == 0) {
            mode = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-volt") == 0) {
            volt = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-freq") == 0) {
            freq = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-duty") == 0) {
            duty = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-int_act") == 0) {
            int_act = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [-h] [-id <ID>]\n", argv[0]);
            display_help();
            return 0;
        }
    }
    LOGD("Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );
    enum_do_devs(hDevice);
    enum_di_devs(hDevice);
    if (do_di == 0) {
        config_do_dev(hDevice, do_di_idx, mode, volt, freq, duty);
    } else if (do_di == 1) {
        config_di_dev(hDevice, do_di_idx, mode, int_act);
    }
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
