#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t qrg_queue;

void init_antenna_control();
void select_antenna(unsigned int antenna);