
#include "ParametersParse.h"
#include "json11.hpp"

using namespace json11;

TY_STATUS write_int_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetInt(hDevice, comp, feat, static_cast<int>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_float_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetFloat(hDevice, comp, feat, static_cast<float>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_enum_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_number())
        return TYSetEnum(hDevice, comp, feat, static_cast<uint32_t>(value.number_value()));
    else
        return TY_STATUS_ERROR;
}

TY_STATUS write_bool_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    if(value.is_bool())
        return TYSetBool(hDevice, comp, feat, value.bool_value());
    else
        return TY_STATUS_ERROR;
}

bool json_parse_arrar(const Json& value, std::vector<char>& buff)
{
    buff.clear();
    if(value.is_array()) {
        size_t size = value.array_items().size();
       	buff.resize(size);
        for(size_t i = 0; i < size; i++)
            buff[i] = static_cast<char>(value[i].number_value());
        return true;
    } else {
        return false;
    }
}

TY_STATUS write_string_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        buff.push_back(0);
        return TYSetString(hDevice, comp, feat, &buff[0]);
    } else {
        return TY_STATUS_ERROR;
    }
}

TY_STATUS write_bytearray_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        return TYSetByteArray(hDevice, comp, feat, (uint8_t*)(&buff[0]), buff.size());
    } else {
        return TY_STATUS_ERROR;
    }
}

TY_STATUS write_struct_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    std::vector<char> buff(0);
    if(json_parse_arrar(value, buff)) {
        return TYSetStruct(hDevice, comp, feat, (void*)(&buff[0]), buff.size());
    } else {
        return TY_STATUS_ERROR;
    }
}


TY_STATUS device_write_feature(const TY_DEV_HANDLE hDevice, TY_COMPONENT_ID comp, TY_FEATURE_ID feat, const Json& value)
{
    TY_STATUS status = TY_STATUS_OK;
    TY_FEATURE_TYPE type = TYFeatureType(feat);
    switch (type)
    {
    case TY_FEATURE_INT:
        status = write_int_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_FLOAT:
        status = write_float_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_ENUM:
        status = write_enum_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_BOOL:
        status = write_bool_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_STRING:
        status = write_string_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_BYTEARRAY:
        status = write_bytearray_feature(hDevice, comp, feat, value);
        break;
    case TY_FEATURE_STRUCT:
        status = write_struct_feature(hDevice, comp, feat, value);
        break;
    default:
        status = TY_STATUS_INVALID_FEATURE;
        break;
    }
    return status;
}

struct DevParam
{
    TY_COMPONENT_ID compID;
    TY_FEATURE_ID   featID;
    Json feat_value;
};
bool json_parse(const TY_DEV_HANDLE hDevice, const char* jscode)
{
    std::string err;
    const auto json = Json::parse(jscode, err);

    Json components = json["component"];
    if(components.is_array()) {
        std::vector<DevParam>  param_list(0);   
        for (auto &k : components.array_items()) {
            const Json& comp_id = k["id"];
            const Json& comp_desc = k["desc"];
            const Json& features = k["feature"];

            if(!comp_id.is_string()) continue;
            if(!comp_desc.is_string()) continue;
            if(!features.is_array()) continue;

            const char* comp_desc_str = comp_desc.string_value().c_str();
            const char* comp_id_str   = comp_id.string_value().c_str();

            TY_COMPONENT_ID m_comp_id;
            sscanf(comp_id_str,"%x",&m_comp_id);

            for (auto &f : features.array_items()) {
                const Json& feat_name   = f["name"];
                const Json& feat_id     = f["id"];
                const Json& feat_value  = f["value"];

                if(!feat_id.is_string()) continue;
                if(!feat_name.is_string()) continue;

                const char* feat_name_str = feat_name.string_value().c_str();
                const char* feat_id_str = feat_id.string_value().c_str();

                TY_FEATURE_ID m_feat_id;
                sscanf(feat_id_str,"%x",&m_feat_id);

                param_list.push_back({m_comp_id, m_feat_id, feat_value});
            }
        }

        while(1) 
        {
            size_t cnt = param_list.size();
            for(auto it = param_list.begin(); it != param_list.end(); ) 
            {
                if(TY_STATUS_OK == device_write_feature(hDevice, it->compID, it->featID, it->feat_value))
                {
                    param_list.erase(it);
                } else {
                    ++it;
                }
            }

            if(param_list.size() == 0) {
                return true;
            }

            if(param_list.size() == cnt) {
                return false;
            }
        }
    }
    return false;
}
