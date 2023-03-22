#ifndef _TYP_COMMAND_LINE_PARSER_HPP
#define _TYP_COMMAND_LINE_PARSER_HPP

#include <string>
#include <vector>
#include <map>

/// @brief  command line arg item
/// @tparam T  context type
template <class T>
class TyCommandlineItem
{
public:
    TyCommandlineItem(const std::string &name = "",
                      const std::string &describe = "",
                      bool is_flag = false,
                      const std::string &default_val = "")
    {
        this->name = name;
        this->describe = describe;
        this->default_val = default_val;
        this->is_flag = is_flag;
        has_set = false;
        curr_val = default_val;
    }
    std::string name, describe; ///< name and describe
    std::string default_val;    ///< default value
    bool is_flag;               ///< flag only, no value
    T ctx;                      ///< context

    bool has_set;         ///< has set by command line
    std::string curr_val; ///< current arg value


    int get_int_val() const
    {
        return std::stoi(curr_val);
    }

    float get_float_val() const
    {
        return std::stof(curr_val);
    }

    double get_double_val() const
    {
        return std::stod(curr_val);
    }

    std::string get_str_val() const
    {
        return curr_val;
    }

    bool get_bool_val() const
    {
        return curr_val == "true" || curr_val == "1";
    }
};


////--------------------

/// @brief  command line parser
/// @tparam T context type
template <class T>
class TyCommandlineParser
{

public:
    std::map<std::string, TyCommandlineItem<T>> cmd_items; ///< command line items

    /// @brief  add command line item
    /// @param name  item name
    /// @param describe  item describe
    /// @param is_flag  is flag only
    /// @param default_val  default value
    /// @param ctx  context
    void addItem(const std::string &name,
                 const std::string &describe,
                 bool is_flag = false,
                 const std::string &default_val = "0",
                 T ctx = T())
    {
        TyCommandlineItem<T> item(name, describe, is_flag, default_val);
        item.ctx = ctx;
        cmd_items.emplace(name, item);
    }

    /// @brief clear all items
    void clear()
    {
        cmd_items.clear();
    }

    /// @brief  parse command line
    /// @param argc  arg count
    /// @param argv  arg list
    /// @return  0: success, -1: failed
    int parse(int argc, char *argv[])
    {
        int idx = 1;
        while (idx < argc)
        {
            std::string arg = argv[idx];
            if (arg[0] != '-')
            {
                continue;
            }
            arg = arg.substr(1);
            auto find_res = cmd_items.find(arg);
            if (find_res== cmd_items.end()) {
                printf("TyCommandlineParser:ignore unknow param: %s\n", arg.c_str());
                idx++;
                continue;
            }
            auto& item = find_res->second;
            item.has_set = true;
            item.curr_val = item.default_val;
            if (idx + 1 < argc && !item.is_flag)
            {
                item.curr_val = argv[idx + 1];
                idx++;
            }
            idx++;
        }
        return 0;
    }

    /// @brief  get command line item
    /// @param name  item name
    /// @return  item
    const TyCommandlineItem<T> *get(const std::string &name) const
    {
        auto find_res = cmd_items.find(name);
        if (find_res != cmd_items.end()) {
            return &find_res->second;
        }
        LOGE("ERROR: not find command argv by name %s ", name.c_str());
        return nullptr;
    }

    /// @brief get usage string
    /// @return usage string
    std::string getUsage() const
    {
        std::string usage = "Usage: \n";
        size_t max_name_len = 1;
        for (auto& kv : cmd_items) {
            max_name_len = std::max(kv.first.size(), max_name_len);
        }
        for (auto& kv : cmd_items)
        {
            const auto &cmd = kv.second;
            std::string name = cmd.name;
            if (name.size() < max_name_len) {
                name.append(max_name_len - name.size(), ' ');
            }
            usage += " -" + name + " ";
            if (!cmd.is_flag)
            {
                usage += "<value>   ";
            }
            else {
                usage += "          ";
            }
            usage +=  cmd.describe + " \n";
        }
        return usage;
    }
};

#endif // _TYP_COMMAND_LINE_PARSER_HPP
