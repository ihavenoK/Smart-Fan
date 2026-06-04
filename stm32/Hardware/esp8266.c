/**
  * @file    esp8266.c
  * @brief   ESP8266 Wi-Fi 模块驱动（USART2, AT 指令）
  */
#include "esp8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/*==============================================================================
 * 接收环形缓冲区
 *============================================================================*/
static char rx_buf[ESP8266_RX_BUF_SIZE];
static volatile uint16_t rx_len = 0;

/* 任务句柄（供 ISR 通知） */
static TaskHandle_t xWiFiNotifyTaskHandle = NULL;

/*==============================================================================
 * 初始化 USART2
 *============================================================================*/
void ESP8266_USART_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(ESP8266_USART_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(ESP8266_GPIO_CLK,  ENABLE);

    /* TX — PA2 复用推挽 */
    GPIO_InitStructure.GPIO_Pin   = ESP8266_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* RX — PA3 上拉输入（与裸机成功版本一致） */
    GPIO_InitStructure.GPIO_Pin   = ESP8266_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 串口参数 */
    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(ESP8266_USART, &USART_InitStructure);

    /* 使能接收中断 */
    USART_ITConfig(ESP8266_USART, USART_IT_RXNE, ENABLE);

    /* NVIC 优先级 ≥ configMAX_SYSCALL_INTERRUPT_PRIORITY (5) */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(ESP8266_USART, ENABLE);
}

/*==============================================================================
 * 发送字符串
 *============================================================================*/
void ESP8266_SendString(const char *str)
{
    while (*str) {
        USART_SendData(ESP8266_USART, *str++);
        while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
    }
}

/*==============================================================================
 * 发送 AT 命令（自动追加 \r\n）
 *============================================================================*/
void ESP8266_SendCmd(const char *cmd)
{
    ESP8266_SendString(cmd);
    ESP8266_SendString("\r\n");
}

/*==============================================================================
 * 清空接收缓冲区
 *============================================================================*/
void ESP8266_RX_Reset(void)
{
    rx_len = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
}

/*==============================================================================
 * 注册 ISR 通知目标任务
 *============================================================================*/
void ESP8266_SetNotifyTask(TaskHandle_t task)
{
    xWiFiNotifyTaskHandle = task;
}


void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) != RESET) {
        uint8_t byte = USART_ReceiveData(ESP8266_USART);
        if (rx_len < ESP8266_RX_BUF_SIZE - 1) {
            rx_buf[rx_len++] = byte;
        }
        /* 通知值 = 0x00，表示 USART 数据到达
         * （与 REPORT_xxx 上报请求区分） */
        xTaskNotifyFromISR(xWiFiNotifyTaskHandle,
                           0x00,
                           eSetValueWithOverwrite,
                           &xHigherPriorityTaskWoken);
        USART_ClearITPendingBit(ESP8266_USART, USART_IT_RXNE);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*==============================================================================
 * 缓冲区访问器
 *============================================================================*/
const char *ESP8266_GetRxBuf(void)
{
    return rx_buf;
}

uint16_t ESP8266_GetRxLen(void)
{
    return rx_len;
}

/*==============================================================================
 * 检查接收缓冲中是否包含预期字符串（带超时，非阻塞）
 *============================================================================*/
uint8_t ESP8266_CheckResponse(const char *expected, uint32_t timeout_ms)
{
    TickType_t xStart = xTaskGetTickCount();

    while ((xTaskGetTickCount() - xStart) < pdMS_TO_TICKS(timeout_ms)) {
        if (strstr(rx_buf, expected) != NULL)
            return 1;
        vTaskDelay(pdMS_TO_TICKS(10));   /* 10ms 轮询一次 */
    }
    return 0;
}
