#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/*==============================================================================
 * 引脚定义 — USART2: PA2(TX), PA3(RX)
 *============================================================================*/
#define ESP8266_USART           USART2
#define ESP8266_USART_CLK       RCC_APB1Periph_USART2
#define ESP8266_GPIO_CLK        RCC_APB2Periph_GPIOA
#define ESP8266_TX_PIN          GPIO_Pin_2
#define ESP8266_RX_PIN          GPIO_Pin_3

/* 接收缓冲区大小 */
#define ESP8266_RX_BUF_SIZE     1024

/*==============================================================================
 * 初始化状态机
 *============================================================================*/
typedef enum {
    WIFI_INIT_START,
    WIFI_INIT_SET_MODE,
    WIFI_INIT_CONNECT_AP,
    WIFI_INIT_SINGLE_CONN,
    WIFI_INIT_MQTT_CONFIG,
    WIFI_INIT_MQTT_CONNECT,
    WIFI_INIT_MQTT_SUBSCRIBE,
    WIFI_INIT_DONE,
    WIFI_IDLE,
    WIFI_FAILED          /* 初始化失败，不再重试，串口打印错误信息后驻留 */
} WiFiState_t;

/*==============================================================================
 * 函数声明
 *============================================================================*/
void        ESP8266_USART_Init(uint32_t baudrate);
void        ESP8266_SendString(const char *str);
void        ESP8266_SendCmd(const char *cmd);
void        ESP8266_RX_Reset(void);
uint8_t     ESP8266_CheckResponse(const char *expected, uint32_t timeout_ms);
void        ESP8266_SetNotifyTask(TaskHandle_t task);
const char *ESP8266_GetRxBuf(void);             /* 获取接收缓冲区指针 */
uint16_t    ESP8266_GetRxLen(void);             /* 获取已接收字节数 */

#endif /* __ESP8266_H */
