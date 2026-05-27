/**
  * @file    debug_serial.c
  * @brief   USART1 调试串口驱动（TX: PA9, RX: PA10）
  *          参考 success/Serial.c 裸机工作版实现
  *          中断优先级设为最低，不影响 FreeRTOS 实时任务
  */
#include "debug_serial.h"
#include <string.h>

/*==============================================================================
 * 接收缓冲区
 *============================================================================*/
char     DEBUG_RX_BUF[DEBUG_RX_BUF_SIZE];
uint16_t DEBUG_RX_LEN = 0;

/*==============================================================================
 * 内部辅助：乘方运算（用于数字格式化输出）
 *============================================================================*/
static uint32_t Serial_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1;
    while (y--) result *= x;
    return result;
}

/*==============================================================================
 * 初始化 USART1 — TX: PA9, RX: PA10
 *============================================================================*/
void DebugSerial_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(DEBUG_USART_CLK | DEBUG_USART_GPIO_CLK, ENABLE);

    /* TX — PA9 复用推挽 */
    GPIO_InitStructure.GPIO_Pin   = DEBUG_TX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* RX — PA10 上拉输入 */
    GPIO_InitStructure.GPIO_Pin   = DEBUG_RX_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 串口参数 */
    USART_InitStructure.USART_BaudRate            = baudrate;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(DEBUG_USART, &USART_InitStructure);

    /* 使能接收中断 */
    USART_ITConfig(DEBUG_USART, USART_IT_RXNE, ENABLE);

    /* NVIC 优先级设为最低（不参与 FreeRTOS 临界区抢占） */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 15;  /* 最低优先级 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(DEBUG_USART, ENABLE);

    /* 清空缓冲区 */
    DebugSerial_RX_Reset();
}

/*==============================================================================
 * 发送一个字节
 *============================================================================*/
void DebugSerial_SendByte(uint8_t byte)
{
    USART_SendData(DEBUG_USART, byte);
    while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
}

/*==============================================================================
 * 发送字符串
 *============================================================================*/
void DebugSerial_SendString(const char *str)
{
    while (*str) {
        DebugSerial_SendByte(*str++);
    }
}

/*==============================================================================
 * 发送数字（指定显示位数）
 *============================================================================*/
void DebugSerial_SendNumber(uint32_t num, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        DebugSerial_SendByte(num / Serial_Pow(10, len - i - 1) % 10 + '0');
    }
}

/*==============================================================================
 * 格式化打印（类似 printf）
 *============================================================================*/
void DebugSerial_Printf(const char *format, ...)
{
    char buf[256];
    va_list arg;
    va_start(arg, format);
    vsnprintf(buf, sizeof(buf), format, arg);
    va_end(arg);
    DebugSerial_SendString(buf);
}

/*==============================================================================
 * 清空接收缓冲区
 *============================================================================*/
void DebugSerial_RX_Reset(void)
{
    DEBUG_RX_LEN = 0;
    memset(DEBUG_RX_BUF, 0, sizeof(DEBUG_RX_BUF));
}

/*==============================================================================
 * USART1 中断服务函数
 *   仅做数据缓存，禁用 FreeRTOS API（防止低优先级中断干扰调度）
 *============================================================================*/
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(DEBUG_USART, USART_IT_RXNE) != RESET) {
        uint8_t byte = USART_ReceiveData(DEBUG_USART);
        if (DEBUG_RX_LEN < DEBUG_RX_BUF_SIZE - 1) {
            DEBUG_RX_BUF[DEBUG_RX_LEN++] = byte;
        }
        USART_ClearITPendingBit(DEBUG_USART, USART_IT_RXNE);
    }
}
