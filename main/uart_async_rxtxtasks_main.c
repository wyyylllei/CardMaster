/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"


static const int RX_BUF_SIZE = 128;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int master_sendData(const char* logName, const char* data, int lenth)
{
    // const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, lenth);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) 
    {
        // sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(200);
    }
}

uint8_t data [128] = {0};        //串口接收的数据缓存(485总线的数据)
void receive_handle(uint8_t data[], uint32_t length);
static uint8_t uUpload_Flag = 0;
static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    while (1) 
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 5);
        if (rxBytes >= 10) {
            data[rxBytes] = 0;
            //数据处理
            receive_handle(data,rxBytes);  
            master_sendData("master_uart", &data,rxBytes);
        }
         vTaskDelay(5);
    }
}


void receive_handle(uint8_t data[], uint32_t length) 
{
    if((data[0] == 0x55) && (data[1] == 0xAA))
    {
        //printf("receive head...\n");
        uUpload_Flag = 1;

    }
    else
    {
        printf("Wrong  head...\n");
        return;
    }
}

//整理需要传给服务器的数据
void server_data_send(uint8_t datain[])
{
    char jsonStrings[][100] = {
        "{\"card\": \"12345678\"}",
        "{\"command\": \"#\"}",  
        "{\"copid\": \"!@$^\"}",      
    };

    
    char replacement1[9] = "12345678";
    int numStrings = 0;
    char temp_str_card[9];//暂存转换的卡号
    char temp_str_command[3];//暂存转换的命令
    char temp_str_copid[5];//暂存转换的从机id

    if (uUpload_Flag) { 
        //提取4字节十六进制卡号为字符串
        sprintf(temp_str_card, "%02X%02X%02X%02X", datain[6], datain[7], datain[8], datain[9]);
        printf("Data String: %s\n", temp_str_card);
        //提取1字节十六进制功能码为字符串
        sprintf(temp_str_command, "%02X", datain[3]);
        printf("Data String: %s\n", temp_str_command);
        //提取4字节十六进制从机id为字符串
        sprintf(temp_str_copid, "%02X%02X", datain[4],datain[5]);
        printf("Data String: %s\n", temp_str_copid);

        //更改jsonStrings中的卡号
        for (int i = 0; i < sizeof(jsonStrings) / sizeof(jsonStrings[0]); i++) {          
            // 在当前字符串中查找 "12345678"
            char* ptr = strstr(jsonStrings[i], "12345678");

            // 如果找到了 "12345678"
            if (ptr != NULL) {
                // 计算替换后的字符串的长度
                int newStringLength = strlen(jsonStrings[i]) - strlen("12345678") + strlen(temp_str_card);
                // 创建一个临时数组存储替换后的字符串
                char tempString[newStringLength + 1];
                // 将替换字符串复制到临时数组中
                strcpy(tempString, jsonStrings[i]);
                memcpy(tempString + (ptr - jsonStrings[i]), temp_str_card, strlen(temp_str_card));
                // 移动剩余的字符
                memmove(tempString + (ptr - jsonStrings[i]) + strlen(temp_str_card), ptr + strlen("12345678"), strlen(ptr + strlen("12345678")) + 1);
                // 将替换后的字符串复制回原始字符串数组
                strcpy(jsonStrings[i], tempString);
            }
        }

        //更改jsonStrings中command
        for (int i = 0; i < sizeof(jsonStrings) / sizeof(jsonStrings[0]); i++) {          
            // 在当前字符串中查找 "#"
            char* ptr = strstr(jsonStrings[i], "#");

            // 如果找到了 "#"
            if (ptr != NULL) {
                // 计算替换后的字符串的长度
                int newStringLength = strlen(jsonStrings[i]) - strlen("#") + strlen(temp_str_command);
                // 创建一个临时数组存储替换后的字符串
                char tempString[newStringLength + 1];
                // 将替换字符串复制到临时数组中
                strcpy(tempString, jsonStrings[i]);
                memcpy(tempString + (ptr - jsonStrings[i]), temp_str_command, strlen(temp_str_command));
                // 移动剩余的字符
                memmove(tempString + (ptr - jsonStrings[i]) + strlen(temp_str_command), ptr + strlen("#"), strlen(ptr + strlen("#")) + 1);
                // 将替换后的字符串复制回原始字符串数组
                strcpy(jsonStrings[i], tempString);
            }
        }

        //更改jsonStrings中 从机id
        for (int i = 0; i < sizeof(jsonStrings) / sizeof(jsonStrings[0]); i++) {          
            // 在当前字符串中查找 "#"
            char* ptr = strstr(jsonStrings[i], "!@$^");

            // 如果找到了 "!@$^"
            if (ptr != NULL) {
                // 计算替换后的字符串的长度
                int newStringLength = strlen(jsonStrings[i]) - strlen("!@$^") + strlen(temp_str_copid);
                // 创建一个临时数组存储替换后的字符串
                char tempString[newStringLength + 1];
                // 将替换字符串复制到临时数组中
                strcpy(tempString, jsonStrings[i]);
                memcpy(tempString + (ptr - jsonStrings[i]), temp_str_copid, strlen(temp_str_copid));
                // 移动剩余的字符
                memmove(tempString + (ptr - jsonStrings[i]) + strlen(temp_str_copid), ptr + strlen("!@$^"), strlen(ptr + strlen("!@$^")) + 1);
                // 将替换后的字符串复制回原始字符串数组
                strcpy(jsonStrings[i], tempString);
            }
        }

        // 打印修改后的字符串
        numStrings = sizeof(jsonStrings) / sizeof(jsonStrings[0]);
        for (int i = 0; i < numStrings; i++) {
            printf("Modified String %d: %s\n", i, jsonStrings[i]);
        }



        uUpload_Flag = 0;
    }
}


void app_main(void)
{
    init();
    //485串口接收数据的线程
    xTaskCreate(rx_task, "uart_rx_task", 2048, NULL, 15, NULL);

    //485串口发送数据的线程
    // xTaskCreate(tx_task, "uart_tx_task", 1024, NULL, 5, NULL);

    //开机测试串口数据
    printf("card master code 0.4  run ...\r\n");
    master_sendData("master_uart", "485 uart test...",sizeof("485 uart test...") - 1);
    while(1)
    {
        vTaskDelay(100);
        server_data_send(data);  
    }
}
