#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

/**
  * @brief  初始化延时（使能 DWT 周期计数器）
  */
void Delay_Init(void);

/**
  * @brief  微秒级延时
  * @param  nus: 延时的微秒数 (1~65535)
  */
void Delay_us(uint32_t nus);

/**
  * @brief  毫秒级延时（非任务上下文使用，FreeRTOS 任务中请用 vTaskDelay）
  * @param  nms: 延时的毫秒数
  */
void Delay_ms(uint32_t nms);

#endif /* __DELAY_H */
