#ifndef _PARAMETERS_PARSE_H_
#define _PARAMETERS_PARSE_H_
#include "TYApi.h"
bool isValidJsonString(const char* code);
bool json_parse(const TY_DEV_HANDLE hDevice, const char* jscode);
#endif