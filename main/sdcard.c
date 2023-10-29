#include "sdcard.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <cJSON.h>

static const char *TAG = "sd card";

#define MOUNT_POINT "/sdcard"
#define EXAMPLE_MAX_CHAR_SIZE    64

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS    CONFIG_EXAMPLE_PIN_CS

// static esp_vfs_fat_sdmmc_mount_config_t mount_config;
// static sdmmc_card_t *card;
// static sdmmc_host_t host = = SDSPI_HOST_DEFAULT();
// static spi_bus_config_t bus_cfg;
// sdspi_device_config_t slot_config;

esp_err_t init_sd_card()
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    sdmmc_card_t *card;
    // sdspi_device_config_t
    // sdspi_device_config_t slot_config;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    // host.max_freq_khz = 10000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    
    return ESP_OK;
}

esp_err_t read_file(const char *path, char *buf)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(buf, 1, 1024, f);

        // if (chunksize > 0) {
        //     /* Send the buffer contents as HTTP response chunk */
        //     if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
        //         fclose(f);
        //         ESP_LOGE(TAG, "File sending failed!");
        //         /* Abort sending file */
        //         httpd_resp_sendstr_chunk(req, NULL);
        //         /* Respond with 500 Internal Server Error */
        //         httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        //        return ESP_FAIL;
        //    }
        // }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);
    fclose(f);
    // char line[EXAMPLE_MAX_CHAR_SIZE];
    // fgets(line, sizeof(line), f);
    // fclose(f);

    // // strip newline
    // char *pos = strchr(line, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}