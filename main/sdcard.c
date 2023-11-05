#include "sdcard.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

static const char *TAG = "sd card";

static const char *mount_point = "/sdcard";

static sdmmc_card_t *card = NULL;
static sdmmc_host_t host = SDSPI_HOST_DEFAULT();

#define EXAMPLE_MAX_CHAR_SIZE    64

// Pin assignments can be set in menuconfig, see "SD Card Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  CONFIG_SDCARD_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_SDCARD_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_SDCARD_PIN_CLK
#define PIN_NUM_CS    CONFIG_SDCARD_PIN_CS

esp_err_t init_sd_card()
{
    esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SPI peripheral");
    host.max_freq_khz = 10000;

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

/**
 * Deinitialize SD Card. Unmount it and deinitialize the SPI bus
*/
esp_err_t deinit_sd_card()
{
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    spi_bus_free(host.slot);
    ESP_LOGI(TAG, "Deinitialized SPI Bus");
    
    return ESP_OK;
}

esp_err_t read_file(const char *file_name, char *buf)
{
    char full_path[60] = {};
    strcat(full_path, mount_point);
    strcat(full_path, "/");
    strcat(full_path, file_name);
    ESP_LOGI(TAG, "Reading file %s", full_path);

    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(buf, 1, 1024, f);

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);
    fclose(f);

    return ESP_OK;
}