#pragma once

#include <esp_err.h>

esp_err_t init_sd_card();
esp_err_t deinit_sd_card();
esp_err_t read_file(const char *file_name, char *buf);