#pragma once

#include <esp_err.h>

esp_err_t init_sd_card();
esp_err_t read_file(const char *path, char *buf);