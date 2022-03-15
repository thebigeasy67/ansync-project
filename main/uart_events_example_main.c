#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "uart_events";

/**
 * This example shows how to use the UART driver to handle special UART events.
 *
 * It also reads data from UART0 directly, and echoes it to console.
 *
 * - Port: UART0
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: on
 * - Pin assignment: TxD (default), RxD (default)
 */

#define EX_UART_NUM UART_NUM_0
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define BLINK_GPIO 2
#define CONFIG_BLINK_PERIOD 1000

static uint8_t s_led_state = 0;
static QueueHandle_t uart0_queue;
static int str_size = 24;
TaskHandle_t handle = NULL;

// static void readLine(char input[]) {
//     int size;
//     char insert;
//     char* c = &insert;
//     uart_event_t event;

//     for(;;) {
//         // fgets(input, 8, stdin);
//         if (xQueueReceive(uart0_queue, &event, (portTickType)portMAX_DELAY)) {
//             size = uart_read_bytes(EX_UART_NUM, (unsigned char *) c, 1, portMAX_DELAY);
//             putchar(insert);
//             fflush(stdout);
//             uart_flush_input(EX_UART_NUM);
//             if (size == 1) {
//                 if (insert == 13) {
//                     return;
//                 }
//                 strncat(input, &insert, 1);
//                 // printf("string: %s\n", input);
//                 if (strlen(input) == str_size) {
//                     return;
//                 }
//             }
//             else {
//                 printf("size = 0\n");
//             }

//         }
//     }
// }

static void configure_led(void)
{
    ESP_LOGI(TAG, "Configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

static void update_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void blinking (void) 
{
    for (;;)
    {
        s_led_state = !s_led_state;
        update_led();
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}

static void readInput(void)
{
    
    char input[str_size];
    uart_event_t event;
    char output[100];
    char insert;
    char* c = &insert;

    nvs_handle nvs_h;  //NVS storage handle
    
    memset(input, '\0', sizeof input);

    //Initialize NVS Storage
    esp_err_t error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);

    //Open for power-on state loading
    error = nvs_open("storage", NVS_READWRITE, &nvs_h);

    if (error != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(error));
    }
    else
    {
        error = nvs_get_i32(nvs_h, "LED_state", &insert);
        if (error == ESP_OK)
        {
            switch (insert)
            {
                case '0':
                    s_led_state = 0;
                    update_led();
                    break;
                case '1':
                    s_led_state = 1;
                    update_led();
                    break;
                case '2':
                    xTaskCreate(blinking, "blinking", 2048, NULL, 12, &handle);
                    break;
            }
        }
        nvs_close(nvs_h);
    }

    for(;;) {
        printf("\n\nEnter 0 for ledOff, 1 for ledOn, 2 for blinking: ");
        fflush(stdout);
        uart_flush_input(EX_UART_NUM);
        
        if (xQueueReceive(uart0_queue, &event, (portTickType)portMAX_DELAY))
        {
            uart_read_bytes(EX_UART_NUM, c, 1, portMAX_DELAY);
            putchar(insert);
            fflush(stdout);
            uart_flush_input(EX_UART_NUM);
        }
        
        error = nvs_open("storage", NVS_READWRITE, &nvs_h);

        if (error != ESP_OK)
        {
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(error));
        }
        else
        {
            switch (insert)
            {
                case '0':
                    nvs_set_i32(nvs_h, "LED_state", insert);
                    strcpy(output, "Led is turned OFF!");
                    s_led_state = 0;
                    update_led();
                    if (handle != NULL) {
                        printf("\n\nDeleting blinking task...");
                        fflush(stdout);
                        uart_flush_input(EX_UART_NUM);
                        vTaskDelete(handle);
                        handle = NULL;
                    }
                    break;
                case '1':
                    nvs_set_i32(nvs_h, "LED_state", insert);
                    strcpy(output, "Led is turned ON!");
                    s_led_state = 1;
                    update_led();
                    if (handle != NULL) {
                        printf("\n\nDeleting blinking task...");
                        fflush(stdout);
                        uart_flush_input(EX_UART_NUM);
                        vTaskDelete(handle);
                        handle = NULL;
                    }
                    break;
                case '2':
                    nvs_set_i32(nvs_h, "LED_state", insert);
                    strcpy(output, "LED is blinking!");
                    if (!handle)
                    {
                        xTaskCreate(blinking, "blinking", 2048, NULL, 12, &handle);
                    }
                    break;
                default:
                    strcpy(output, "Please enter a valid command.");
                    break;
            }

            printf("\n\n%s", output);
            nvs_commit(nvs_h);
            nvs_close(nvs_h);
            
            fflush(stdout);
            uart_flush_input(EX_UART_NUM);

            memset(input, '\0', sizeof input);
        }       
    }
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);

    //Configure LED
    configure_led();

    //Create a task to handler UART event from ISR
    xTaskCreate(readInput, "readInput", 2048, NULL, 12, NULL);
}