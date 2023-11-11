#include "antenna_control.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NUMBER_OF_ANTENNA 6

static const char* TAG = "antenna_control";
static int ant_led_gpio[NUMBER_OF_ANTENNA] = {CONFIG_ANT1_PIN_LED, CONFIG_ANT2_PIN_LED, CONFIG_ANT3_PIN_LED, CONFIG_ANT4_PIN_LED, CONFIG_ANT5_PIN_LED, CONFIG_ANT6_PIN_LED};
#define automode_long_button_press_ms 1500
#define automode_short_button_press_ms 100
static bool automode_enabled = false;
static TaskHandle_t xHandle;

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

static void automode_button_click_cb(void *arg,void *usr_data)
{
    xTaskNotify(xHandle, 2, eSetValueWithOverwrite);
}

static void automode_button_long_press_cb(void *arg,void *usr_data)
{
    xTaskNotify(xHandle, 1, eSetValueWithOverwrite);
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
            if(automode_enabled) {
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, true);
            }
        } else if(ulNotifiedValue == 2) {
            ESP_LOGI(TAG, "Enable or disable AutoMode");
            if(automode_enabled) {
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, false);
                automode_enabled = false;
            } else {
                gpio_set_level(CONFIG_AUTOMODE_PIN_LED, true);
                automode_enabled = true;
            }
        }
    }
}

static void init_automode_button()
{
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

void select_antenna(unsigned int antenna)
{
    if(antenna >= 1 && antenna <= NUMBER_OF_ANTENNA) {
        disable_all_antenna_leds();
        gpio_set_level(ant_led_gpio[antenna - 1], true);
    } else {
        ESP_LOGE(TAG, "select_antenna invalid antenna number: %u", antenna);
    }
}


void init_antenna_control()
{
    init_leds();
    disable_all_antenna_leds();
    init_automode_button();
}