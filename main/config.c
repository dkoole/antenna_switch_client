#include "config.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <esp_log.h>

static const char *TAG = "config";

/**
 * Check if the given string is a valid IP Address
*/
static bool is_valid_ip_address(const char *address)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, address, &(sa.sin_addr));
    return result != 0;
}

/**
 * Parse JSON configuration
*/
bool parse_config(const char* config_str, Config* config)
{
    cJSON *root = cJSON_Parse(config_str);
    if(!root) {
        ESP_LOGE(TAG, "Confifuration could not be parsed");
        return false;
    }

    cJSON *server_address = cJSON_GetObjectItem(root,"server_address");
    if(!server_address) {
        ESP_LOGE(TAG, "Server address not found");
        return false;
    }

    if(!is_valid_ip_address(server_address->valuestring)) {
        ESP_LOGE(TAG, "Server address is not valid");
        return false;
    }

    strcpy(config->server_ip, server_address->valuestring);
    cJSON *use_wifi = cJSON_GetObjectItem(root, "use_wifi");
    if(!use_wifi) {
        ESP_LOGE(TAG, "use_wifi not found");
        return false;
    }

    if(cJSON_IsTrue(use_wifi) == 1) {
        ESP_LOGI(TAG, "use_wifi set to true");
        config->use_wifi = true;
    } 

    ESP_LOGI(TAG, "Parsed configuration file successfully");
    return true;
}