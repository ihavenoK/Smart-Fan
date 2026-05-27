#ifndef __DEBUG_SERIAL_H
#define __DEBUG_SERIAL_H

#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>

/*==============================================================================
 * 引脚定义 — USART1: PA9(TX), PA10(RX)  标准 STM32F103 默认映射
 *============================================================================*/
#define DEBUG_USART             USART1
#define DEBUG_USART_CLK         RCC_APB2Periph_USART1
#define DEBUG_USART_GPIO_CLK    RCC_APB2Periph_GPIOA
#define DEBUG_TX_PIN            GPIO_Pin_9
#define DEBUG_RX_PIN            GPIO_Pin_10
#define DEBUG_RX_BUF_SIZE       1024

/*==============================================================================
 * 接收缓冲区（extern 声明）
 *============================================================================*/
extern char     DEBUG_RX_BUF[DEBUG_RX_BUF_SIZE];
extern uint16_t DEBUG_RX_LEN;

/*==============================================================================
 * 函数声明
 *============================================================================*/
void DebugSerial_Init(uint32_t baudrate);
void DebugSerial_SendByte(uint8_t byte);
void DebugSerial_SendString(const char *str);
void DebugSerial_SendNumber(uint32_t num, uint16_t len);
void DebugSerial_Printf(const char *format, ...);
void DebugSerial_RX_Reset(void);

/* USART1 中断服务函数（定义在 .c 中，避免与 stm32f10x_it.c 重复） */
void USART1_IRQHandler(void);

#endif /* __DEBUG_SERIAL_H */
