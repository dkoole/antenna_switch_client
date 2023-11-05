#pragma once

#include <stdbool.h>
#include <cJSON.h>

typedef struct Config
{
    char server_ip[60];
    bool use_wifi;
} Config;

bool parse_config(const char* config_str, Config* config);