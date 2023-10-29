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
#include <arpa/inet.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include <cJSON.h>
#include "driver/gpio.h"
#include "sdcard.h"

// #define NO_DATA_TIMEOUT_SEC 100

static const char *TAG = "antenna_switch_client";

#define MOUNT_POINT "/sdcard"
#define LED_OUTPUT_PORT 22

// static TimerHandle_t shutdown_signal_timer;
// static SemaphoreHandle_t shutdown_sema;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// static void shutdown_signaler(TimerHandle_t xTimer)
// {
//     ESP_LOGI(TAG, "No data received for %d seconds, signaling shutdown", NO_DATA_TIMEOUT_SEC);
//     xSemaphoreGive(shutdown_sema);
// }

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGW(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        } else if (data->op_code == 0xA) {
            ESP_LOGI(TAG, "Received Pong frame");
        } else {
            ESP_LOGW(TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
            if(strncmp(data->data_ptr, "ant4", data->data_len) == 0) {
                ESP_LOGI(TAG, "Enabling LED");
                gpio_set_level(LED_OUTPUT_PORT, 1);
            } else {
                gpio_set_level(LED_OUTPUT_PORT, 0);
            }
        }

        // If received data contains json structure it succeed to parse
        // cJSON *root = cJSON_Parse(data->data_ptr);
        // if (root) {
        //     for (int i = 0 ; i < cJSON_GetArraySize(root) ; i++) {
        //         cJSON *elem = cJSON_GetArrayItem(root, i);
        //         cJSON *id = cJSON_GetObjectItem(elem, "id");
        //         cJSON *name = cJSON_GetObjectItem(elem, "name");
        //         ESP_LOGW(TAG, "Json={'id': '%s', 'name': '%s'}", id->valuestring, name->valuestring);
        //     }
        //     cJSON_Delete(root);
        // }
        ESP_LOGW(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);

        // xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  data->error_handle.esp_transport_sock_errno);
        }
        break;
    }
}

static void websocket_app_start(const char *ip_address)
{
    gpio_set_direction(LED_OUTPUT_PORT, GPIO_MODE_OUTPUT);
    esp_websocket_client_config_t websocket_cfg = {};

    // shutdown_signal_timer = xTimerCreate("Websocket shutdown timer", NO_DATA_TIMEOUT_SEC * 1000 / portTICK_PERIOD_MS,
    //                                      pdFALSE, NULL, shutdown_signaler);
    // shutdown_sema = xSemaphoreCreateBinary();

    // websocket_cfg.uri = CONFIG_WEBSOCKET_URI;
    char uri[60];
    strcat(uri, "ws://");
    strcat(uri, ip_address);
    strcat(uri, "/ws");
    websocket_cfg.uri = uri;

    ESP_LOGI(TAG, "Connecting to %s...", websocket_cfg.uri);

    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);

    esp_websocket_client_start(client);
    //xTimerStart(shutdown_signal_timer, portMAX_DELAY);
    char data[32];
    int len = snprintf(data, 32, "ant4");
    ESP_LOGI(TAG, "Sending %s", data);
    esp_websocket_client_send_text(client, data, len, portMAX_DELAY);

    //xSemaphoreTake(shutdown_sema, portMAX_DELAY);
    //esp_websocket_client_close(client, portMAX_DELAY);
    //ESP_LOGI(TAG, "Websocket Stopped");
    //esp_websocket_client_destroy(client);
}

bool is_valid_ip_address(const char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
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

    ESP_ERROR_CHECK(init_sd_card());
    char *ip_address_str = NULL;
    char config_buf[1024];
    const char *file_config = MOUNT_POINT"/config.json";
    read_file(file_config, config_buf);
    cJSON *root = cJSON_Parse(config_buf);
    if(root) {
        ESP_LOGI(TAG, "Parsed config file");
        cJSON *ip_adress_element = cJSON_GetObjectItem(root,"ip_address");
        if(ip_adress_element) {
            ip_address_str = ip_adress_element->valuestring;
            if(is_valid_ip_address(ip_address_str)) {
                ESP_LOGI(TAG, "Valid IP Address: %s", ip_address_str);    
           } else {
                    ESP_LOGE(TAG, "%s is not a valid IP Address", ip_address_str);
            }
        } else {
                ESP_LOGE(TAG, "ip_address not found in config file");
        } 
    } else {
            ESP_LOGE(TAG, "Could not parse config file");
        }

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    websocket_app_start(ip_address_str);
}
