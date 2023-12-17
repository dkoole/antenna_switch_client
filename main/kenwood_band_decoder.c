#include "kenwood_band_decoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "antenna_control.h"

static const char *TAG = "band_decoder";

#define COMMAND "IF;"
#define EX_UART_NUM UART_NUM_2
#define PATTERN_CHR_NUM    (1)

#define RX_BUF_SIZE (1024)
#define RD_BUF_SIZE (RX_BUF_SIZE)
static QueueHandle_t uart0_queue;

static int send_data(const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(EX_UART_NUM, data, len);
    // ESP_LOGI(TAG, "Wrote %s bytes", data);
    return txBytes;
}

static void tx_task(void *arg)
{
    // const int len = strlen(COMMAND);
    while (1) {
        send_data(COMMAND);
        // uart_write_bytes(EX_UART_NUM, COMMAND, len);
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

static void rx_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) {
            //Event of UART receving data
            /*We'd better handler data event fast, there would be much more data events than
            other types of events. If we take too much time on data event, the queue might
            be full.*/
            case UART_DATA:
                ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                ESP_LOGI(TAG, "[DATA EVT]:");
                // ESP_LOGI(TAG, "Received data: %s", (const char*)dtmp);
                // uart_write_bytes(EX_UART_NUM, (const char*) dtmp, event.size);
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                // If fifo overflow happened, you should consider adding flow control for your application.
                // The ISR has already reset the rx FIFO,
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                // If buffer full happened, you should consider increasing your buffer size
                // As an example, we directly flush the rx buffer here in order to read more data.
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                    // record the position. We should set a larger queue size.
                    // As an example, we directly flush the rx buffer here.
                    uart_flush_input(EX_UART_NUM);
                } else {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "read data: %s", dtmp);
                    ESP_LOGI(TAG, "read pat : %s", pat);
                    xQueueSend(qrg_queue, &dtmp[2], 0);
                }
                break;
            //Others
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

void init_band_decoder()
{
    const uart_config_t uart_config = {
        .baud_rate = 57600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, RX_BUF_SIZE * 2, 0, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    ESP_ERROR_CHECK(uart_set_pin(EX_UART_NUM, 16, 4, 32, 33));

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    // uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, ';', PATTERN_CHR_NUM, 20, 0, 0);
    
    //Reset the pattern queue length to record at most 1 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 5);

    //Create a task to handler UART event from ISR
    xTaskCreate(rx_task, "rx_task", 2048, NULL, 12, NULL);
    xTaskCreate(tx_task, "tx_task", 2048, NULL, configMAX_PRIORITIES - 2, NULL);
}