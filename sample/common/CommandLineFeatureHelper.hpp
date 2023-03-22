#ifndef CMDLINE_FEATURE_HELPER__H_
#define CMDLINE_FEATURE_HELPER__H_

#include "CommandLineParser.hpp"
#include "TYApi.h"
#include "Utils.hpp"

/// @brief  command line sgb feature param id
struct ty_fetaure_options
{
    int component_id;
    int feature_id;

    ty_fetaure_options(int comp_id = 0, int _f_id = 0)
    {
        component_id = comp_id;
        feature_id = _f_id;
    }
};

/// @brief command line feature helper for set device feature by command line args
class CommandLineFeatureHelper
{
public:
    TyCommandlineParser<ty_fetaure_options> cmd_parser; ///< command line parser

    /// @brief  add feature param to command line parser
    /// @param param  command line param name
    /// @param comp_id  component id
    /// @param feat_id  feature id
    /// @param val  default value
    /// @param desc  describe
    /// @param is_flag  is a flag only , no value
    void add_feature(const std::string &param, int comp_id, int feat_id, int val, const std::string &desc, bool is_flag = false)
    {
        cmd_parser.addItem(param, desc, is_flag, std::to_string(val), ty_fetaure_options(comp_id, feat_id));
    }

    /// @brief  add feature param to command line parser
    /// @param param  command line param name
    /// @param comp_id   component id
    /// @param feat_id  feature id
    /// @param val  default value
    /// @param desc  describe
    /// @param is_flag  is a flag only , no value
    void add_feature(const std::string &param, int comp_id, int feat_id, std::string val, const std::string &desc, bool is_flag = false)
    {
        cmd_parser.addItem(param, desc, is_flag, val, ty_fetaure_options(comp_id, feat_id));
    }

    /// @brief  add feature param to command line parser
    /// @param name  command line param name
    /// @return  command line item
    const TyCommandlineItem<ty_fetaure_options> *get_feature(const std::string &name) const
    {
        auto res = cmd_parser.get(name);
        return res;
    }

    /// @brief get command line param describe
    /// @return  describe string
    std::string usage_describe() const
    {
        return cmd_parser.getUsage();
    }

    /// @brief parse command line args
    void parse_argv(int argc, char *argv[])
    {
        cmd_parser.parse(argc, argv);
    }

    /// @brief set command line param to device
    /// @param hDevice  device handle
    void set_device_feature(TY_DEV_HANDLE hDevice)
    {
        // loop for all command line argv items and set to device
        for (auto &kv : cmd_parser.cmd_items)
        {
            auto &p = kv.second;
            int res = TY_STATUS_OK;
            if (!p.has_set)
            {
                continue;
            }
            int feature_id = p.ctx.feature_id;
            int comp_id = p.ctx.component_id;
            if (comp_id == 0 && feature_id == 0)
            {
                // param is not a feature setting
                continue;
            }
            // set feature by type
            int type = feature_id & 0xf000;
            if (type == TY_FEATURE_INT)
            {
                int val = p.get_int_val();
                LOGD("set feature %s (compId 0x%x featId 0x%x)  to %d", p.name.c_str(), comp_id, feature_id, val);
                res = TYSetInt(hDevice, comp_id, feature_id, val);
            }
            else if (type == TY_FEATURE_BOOL)
            {
                bool val = p.get_bool_val();
                LOGD("set feature %s (compId 0x%x featId 0x%x)  to %d", p.name.c_str(), comp_id, feature_id, val);
                res = TYSetBool(hDevice, comp_id, feature_id, val);
            }
            else if (type == TY_FEATURE_FLOAT)
            {
                float val = p.get_float_val();
                LOGD("set feature %s (compId 0x%x featId 0x%x)  to %f", p.name.c_str(), comp_id, feature_id, val);
                res = TYSetFloat(hDevice, comp_id, feature_id, val);
            }
            else
            {
                LOGE("unknow feature type %d for %s", type, p.name.c_str());
                continue;
            }
            if (res != TY_STATUS_OK)
            {
                LOGE("set feature %s (%s) FAILED with return status code %d", p.name.c_str(), p.describe.c_str(), res);
            }
        }
    }
};

#endif // CMDLINE_FEATURE_HELPER__H_