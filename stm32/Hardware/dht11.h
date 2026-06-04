#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

// DHT11 GPIO 定义（使用 PA0，可根据实际接线修改）
#define DHT11_GPIO_PORT     GPIOA
#define DHT11_GPIO_PIN      GPIO_Pin_0
#define DHT11_GPIO_CLK      RCC_APB2Periph_GPIOA

// 函数声明
void DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi); // 返回0成功，1失败

#endif
