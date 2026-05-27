#ifndef __SERIAL_H__
#define __SERIAL_H__
#include <stdio.h>
#include <stdarg.h>

void Serial_Init (uint8_t USART_Name);
void Serial_SendByte (uint8_t USART_Name,uint8_t Byte);//发送一个字节
void Serial_SendArray (uint8_t USART_Name,uint8_t *Array,uint16_t Length);//发送一个数组
void Serial_SendString (uint8_t USART_Name,char *String);//发送字符串
void Serial_SendNumber (uint8_t USART_Name,uint32_t Number,uint16_t Length);//发送一个数字
void Serial_Printf (uint8_t USART_Name,char *format,...);//Printf函数

extern char USART1_RX_BUF[1024];//接收数据的数组
extern uint16_t USART1_RX_LEN;//接收数据的长度
extern char USART2_RX_BUF[1024];//接收数据的数组
extern uint16_t USART2_RX_LEN;//接收数据的长度
#endif
