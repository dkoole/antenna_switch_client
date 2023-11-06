/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include <driver/gpio.h>
#include "sdcard.h"
#include "config.h"
#include "websocket_client.h"
#include "ethernet_init.h"

static const char *TAG = "antenna_switch_client";

#define CONFIG_FILE "config.json"

#define NUMBER_OF_ANTENNA 6

static int ant_led_gpio[NUMBER_OF_ANTENNA] = {CONFIG_ANT1_PIN_LED, CONFIG_ANT2_PIN_LED, CONFIG_ANT3_PIN_LED, CONFIG_ANT4_PIN_LED, CONFIG_ANT5_PIN_LED, CONFIG_ANT6_PIN_LED};

void init_leds()
{
    for(unsigned int i = 0; i < NUMBER_OF_ANTENNA; i++)
    {
        gpio_set_direction(ant_led_gpio[i], GPIO_MODE_OUTPUT);
    }
    gpio_set_direction(CONFIG_AUTOMODE_PIN_LED, GPIO_MODE_OUTPUT);
}

void disable_all_antenna_leds()
{
    for(unsigned int i = 0; i < NUMBER_OF_ANTENNA; i++)
    {
        gpio_set_level(ant_led_gpio[i], false);
    }
}

void select_antenna(unsigned int antenna)
{
    if(antenna >= 1 && antenna <= NUMBER_OF_ANTENNA) {
        disable_all_antenna_leds();
        gpio_set_level(ant_led_gpio[antenna - 1], true);
    } else {
        ESP_LOGE(TAG, "select_antenna invalid antenna number: %u", antenna);
    }
}

/**
 * Task that blinks a led to indicate something went wrong parsing the config file
 */
static void error_task()
{
    bool level = true;
    while(true) {
        gpio_set_level(CONFIG_AUTOMODE_PIN_LED, level);
        level = !level;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("websocket_client", ESP_LOG_DEBUG);
    esp_log_level_set("transport_ws", ESP_LOG_DEBUG);
    esp_log_level_set("trans_tcp", ESP_LOG_DEBUG);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    init_leds();

    if(init_sd_card() != ESP_OK) {
        xTaskCreate(error_task, "error_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);
        return;
    }

    char config_buf[1024];
    if(read_file(CONFIG_FILE, config_buf) != ESP_OK) {
        deinit_sd_card();
        xTaskCreate(error_task, "error_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);
        return;
    }
    
    Config myconfig = { .server_ip = {}, .use_wifi = false };
    if(!parse_config(config_buf, &myconfig)) {
        deinit_sd_card();
        xTaskCreate(error_task, "error_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);
        return;
    }

    deinit_sd_card();

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    // ESP_ERROR_CHECK(example_connect());

    // if(myconfig.use_wifi) {
    //     // connect_wifi();
    // } else {
        // ESP_LOGI(TAG, "Ethernet connection not yet supported");

    ethernet_init();
    
    // // Create default event loop that running in background
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // }

    websocket_client_connect(myconfig.server_ip);
}