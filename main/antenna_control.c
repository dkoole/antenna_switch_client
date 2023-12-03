#include "antenna_control.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "websocket_client.h"
#include "nvs.h"

#define NUMBER_OF_ANTENNA 6

static const char* TAG = "antenna_control";
static int ant_led_gpio[NUMBER_OF_ANTENNA] = {CONFIG_ANT1_PIN_LED, CONFIG_ANT2_PIN_LED, CONFIG_ANT3_PIN_LED, CONFIG_ANT4_PIN_LED, CONFIG_ANT5_PIN_LED, CONFIG_ANT6_PIN_LED};
#define automode_long_button_press_ms 1500
#define automode_short_button_press_ms 100
static bool automode_enabled = false;
static TaskHandle_t xHandle;
static TaskHandle_t antTaskHandle;
static uint8_t ant1_value = 1;
static uint8_t ant2_value = 2;
static uint8_t ant3_value = 3;
static uint8_t ant4_value = 4;
static uint8_t ant5_value = 5;
static uint8_t ant6_value = 6;
static SemaphoreHandle_t automodeSemaphore = NULL;
static QueueHandle_t qrg_queue;
static nvs_handle_t my_nvs_handle;

enum AmateurBand 
{
    _160M,
    _80M,
    _60M,
    _40M,
    _30M,
    _20M,
    _17M,
    _15M,
    _10M,
    _6M,
    UNKNOWN
};

static const char* const AmateurBandStr[] =
{
    "160M",
    "80M",
    "60M",
    "40M",
    "30M",
    "20M",
    "17M",
    "15M",
    "10M",
    "6M",
    "UNKNOWN"
};

static void init_leds()
{
    for(unsigned int i = 0; i < NUMBER_OF_ANTENNA; i++)
    {
        gpio_set_direction(ant_led_gpio[i], GPIO_MODE_OUTPUT);
    }
    gpio_set_direction(CONFIG_AUTOMODE_PIN_LED, GPIO_MODE_OUTPUT);
}

static void disable_all_antenna_leds()
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

static void automode_button_click_cb(void *arg,void *usr_data)
{
    xTaskNotify(xHandle, 2, eSetValueWithOverwrite);
}

static void automode_button_long_press_cb(void *arg,void *usr_data)
{
    xTaskNotify(xHandle, 1, eSetValueWithOverwrite);
}

static enum AmateurBand hz_to_amateur_band(unsigned int qrg)
{
    if(qrg >= 1800000 && qrg <= 2000000) {
        return _160M;
    } else if(qrg >= 3500000 && qrg <= 3800000) {
        return _80M;
    } else if(qrg >= 5351500 && qrg <= 5366500) {
        return _60M;
    } else if(qrg >= 7000000 && qrg <= 7300000) {
        return _40M;
    } else if(qrg >= 10100000 && qrg <= 10150000) {
        return _30M;
    } else if(qrg >= 14000000 && qrg <= 14350000) {
        return _20M;
    } else if(qrg >= 18068000 && qrg <= 18168000) { 
        return _17M;
    } else if(qrg >= 21000000 && qrg <= 21450000) {
        return _15M;
    } else if(qrg >= 28000000 && qrg <= 29700000) {
        return _10M;
    } else if(qrg >= 50000000 && qrg <= 54000000) {
        return _6M;
    } else {
        return UNKNOWN;
    }
}

static void automode_control_task()
{
    char qrg_str[11];
    uint8_t antenna_number = 0;
    enum AmateurBand previous_band = UNKNOWN;
    enum AmateurBand active_band = UNKNOWN;
    int qrg = 0;
    for(;;) {
        if (xQueueReceive(qrg_queue, (void *)&qrg_str, (TickType_t)portMAX_DELAY)) {
            if(automode_enabled) {
                qrg = atoi(qrg_str);
                previous_band = active_band;
                active_band = hz_to_amateur_band(qrg);
                if((active_band != previous_band) && active_band != UNKNOWN) {
                    esp_err_t err = nvs_get_u8(my_nvs_handle, AmateurBandStr[active_band], &antenna_number);
                    if(err == ESP_OK) {
                        disable_all_antenna_leds();
                        send_current_antenna(antenna_number);
                    }
                }
            }
        }
    }
}

static void automode_button_task()
{
    uint32_t ulNotifiedValue;
    for( ;; )
    {
        xTaskNotifyWait( 0x00, /* Don't clear any notification bits on entry. */
                        ULONG_MAX, /* Reset the notification value to 0 on exit. */
                        &ulNotifiedValue, /* Notified value pass out in ulNotifiedValue. */
                        portMAX_DELAY ); /* Block indefinitely. */
        if(ulNotifiedValue == 1) {
            ESP_LOGI(TAG, "Reset Automode");
            for(int i = 0; i < 3;++i) {
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, true);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, false);
                vTaskDelay(300 / portTICK_PERIOD_MS);
            }
            if (automode_enabled) {
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, true);
            }
        } else if(ulNotifiedValue == 2) {
            ESP_LOGI(TAG, "Enable or disable AutoMode");
            if(automodeSemaphore != NULL) {
                if( xSemaphoreTake(automodeSemaphore, ( TickType_t ) 10 ) == pdTRUE ) {
                    if(automode_enabled) {
                        gpio_set_level(CONFIG_AUTOMODE_PIN_LED, false);
                        automode_enabled = false;
                    } else {
                        gpio_set_level(CONFIG_AUTOMODE_PIN_LED, true);
                        automode_enabled = true;
                    }
                    xSemaphoreGive(automodeSemaphore);
                }
            }
        }
    }
}

static void init_automode_button()
{
    automodeSemaphore = xSemaphoreCreateMutex();

    button_config_t automode_button_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = automode_long_button_press_ms,
        .short_press_time = automode_short_button_press_ms,
        .gpio_button_config = {
            .gpio_num = CONFIG_AUTOMODE_BUTTON_GPIO,
            .active_level = 0,
        },
    };
    
    button_handle_t automode_button = iot_button_create(&automode_button_config);
    if(NULL == automode_button) {
        ESP_LOGE(TAG, "Automode Button create failed");
        return;
    }

    xTaskCreate(automode_button_task, "automode_button_task", 2048, NULL, 12, &xHandle);
    if(NULL == xHandle) {
        ESP_LOGE(TAG, "Cannot create automode button task");
    }

    iot_button_register_cb(automode_button, BUTTON_SINGLE_CLICK, automode_button_click_cb, NULL);
    iot_button_register_cb(automode_button, BUTTON_LONG_PRESS_START, automode_button_long_press_cb, NULL);
}

static void antenna_button_cb(void *arg,void *usr_data)
{
    uint8_t value = *((uint8_t*)usr_data);
    ESP_LOGI(TAG, "Antenna button clicked");
    xTaskNotify(antTaskHandle, value, eSetValueWithOverwrite);
}

static void antenna_button_task()
{
    uint32_t ulNotifiedValue;
    for( ;; )
    {
        xTaskNotifyWait( 0x00, /* Don't clear any notification bits on entry. */
                        ULONG_MAX, /* Reset the notification value to 0 on exit. */
                        &ulNotifiedValue, /* Notified value pass out in ulNotifiedValue. */
                        portMAX_DELAY ); /* Block indefinitely. */
        if(!automode_enabled) {
            // disable_all_antenna_leds();
            send_current_antenna(ulNotifiedValue);
        }
    }

}

static void init_antenna_buttons()
{
    button_config_t ant1_button_config = {
        .type = BUTTON_TYPE_ADC,
        .long_press_time = automode_long_button_press_ms,
        .short_press_time = automode_short_button_press_ms,
        .adc_button_config = {
            .adc_channel = 0,
            .button_index = 0,
            .min = 700,
            .max = 2000,
        },
    };
    
    button_handle_t ant1_button = iot_button_create(&ant1_button_config);
    if(NULL == ant1_button) {
        ESP_LOGE(TAG, "ANT1 Button create failed");
    }

    button_config_t ant2_button_config = {
    .type = BUTTON_TYPE_ADC,
    .long_press_time = automode_long_button_press_ms,
    .short_press_time = automode_short_button_press_ms,
        .adc_button_config = {
            .adc_channel = 0,
            .button_index = 1,
            .min = 2300,
            .max = 2700,
        },
    };
    button_handle_t ant2_button = iot_button_create(&ant2_button_config);
    if(NULL == ant2_button) {
        ESP_LOGE(TAG, "ANT2 Button create failed");
    }

    button_config_t ant3_button_config = {
    .type = BUTTON_TYPE_ADC,
    .long_press_time = automode_long_button_press_ms,
    .short_press_time = automode_short_button_press_ms,
    .adc_button_config = {
        .adc_channel = 0,
        .button_index = 2,
        .min = 3000,
        .max = 4000,
    },
    };
    button_handle_t ant3_button = iot_button_create(&ant3_button_config);
    if(NULL == ant3_button) {
        ESP_LOGE(TAG, "ANT3 Button create failed");
    }

    button_config_t ant4_button_config = {
        .type = BUTTON_TYPE_ADC,
        .long_press_time = automode_long_button_press_ms,
        .short_press_time = automode_short_button_press_ms,
        .adc_button_config = {
            .adc_channel = 1,
            .button_index = 0,
            .min = 700,
            .max = 2000,
        },
    };
    
    button_handle_t ant4_button = iot_button_create(&ant4_button_config);
    if(NULL == ant4_button) {
        ESP_LOGE(TAG, "ANT4 Button create failed");
    }

    button_config_t ant5_button_config = {
    .type = BUTTON_TYPE_ADC,
    .long_press_time = automode_long_button_press_ms,
    .short_press_time = automode_short_button_press_ms,
        .adc_button_config = {
            .adc_channel = 1,
            .button_index = 1,
            .min = 2300,
            .max = 2700,
        },
    };
    button_handle_t ant5_button = iot_button_create(&ant5_button_config);
    if(NULL == ant5_button) {
        ESP_LOGE(TAG, "ANT5 Button create failed");
    }

    button_config_t ant6_button_config = {
    .type = BUTTON_TYPE_ADC,
    .long_press_time = automode_long_button_press_ms,
    .short_press_time = automode_short_button_press_ms,
    .adc_button_config = {
        .adc_channel = 1,
        .button_index = 2,
        .min = 3000,
        .max = 4000,
    },
    };
    button_handle_t ant6_button = iot_button_create(&ant6_button_config);
    if(NULL == ant6_button) {
        ESP_LOGE(TAG, "ANT6 Button create failed");
    }

    xTaskCreate(antenna_button_task, "antenna_button_task", 2048, NULL, 12, &antTaskHandle);

    iot_button_register_cb(ant1_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant1_value);
    iot_button_register_cb(ant2_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant2_value);
    iot_button_register_cb(ant3_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant3_value);
    iot_button_register_cb(ant4_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant4_value);
    iot_button_register_cb(ant5_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant5_value);
    iot_button_register_cb(ant6_button, BUTTON_SINGLE_CLICK, antenna_button_cb, &ant6_value);
}

/**
 * Assumes nvs_flash_init is already called!
*/
void init_antenna_control()
{
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize NVS handle!: (%s)", esp_err_to_name(err));
    }

    init_leds();
    disable_all_antenna_leds();
    init_automode_button();
    init_antenna_buttons();
}