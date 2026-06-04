/**
  * @file    delay.c
  * @brief   DWT 微秒/毫秒延时（72MHz → 72 周期/us）
  * @note    STM32F10x 标准库的 core_cm3.h 无 DWT 定义，此处手动声明
  */
#include "delay.h"

/*==============================================================================
 * DWT 寄存器手动定义（Cortex-M3 固定地址）
 *============================================================================*/
typedef struct {
    volatile uint32_t CTRL;       /* 0x00: Control */
    volatile uint32_t CYCCNT;     /* 0x04: Cycle Count */
    volatile uint32_t CPICNT;     /* 0x08: CPI Count */
    volatile uint32_t EXCCNT;     /* 0x0C: Exception Overhead Count */
    volatile uint32_t SLEEPCNT;   /* 0x10: Sleep Count */
    volatile uint32_t LSUCNT;     /* 0x14: LSU Count */
    volatile uint32_t FOLDCNT;    /* 0x18: Fold Count */
    volatile uint32_t PCSR;       /* 0x1C: Program Counter Sample */
} DWT_TypeDef;

#define DWT_BASE                (0xE0001000UL)
#define DWT                     ((DWT_TypeDef *)DWT_BASE)
#define DWT_CTRL_CYCCNTENA_Msk  (1UL << 0)

/*==============================================================================
 * 初始化 DWT 周期计数器
 *============================================================================*/
void Delay_Init(void)
{
    /* 使能 TRC */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 清零计数器 */
    DWT->CYCCNT = 0;

    /* 使能 CYCCNT */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*==============================================================================
 * 微秒延时
 *   CYCCNT 是 32 位，72MHz 下可支撑约 59 秒
 *============================================================================*/
void Delay_us(uint32_t nus)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = nus * (SystemCoreClock / 1000000UL);   /* 1us = 72 cycles */
    while ((DWT->CYCCNT - start) < cycles);
}

/*==============================================================================
 * 毫秒延时（非任务上下文用，FreeRTOS 任务中请用 vTaskDelay）
 *============================================================================*/
void Delay_ms(uint32_t nms)
{
    while (nms--) {
        Delay_us(1000);
    }
}
