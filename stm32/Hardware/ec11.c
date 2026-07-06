/**
  * @file    ec11.c
  * @brief   EC11 旋转编码器底层驱动
  *          - 外部中断检测 A/B 相下降沿 + SW 按键按下/松开
  *          - ISR 通过 vTaskNotifyGiveFromISR 唤醒编码器任务
  *          - 消抖和方向判断在任务中完成
  *          - SW 使用双边沿中断：按下和松开均唤醒任务，支持长按计时
  */
#include "ec11.h"
#include "FreeRTOS.h"
#include "task.h"

/* 外部任务句柄，在 main.c 创建任务时赋值 */
extern TaskHandle_t xEncoderTaskHandle;

/*==============================================================================
 * EXTI9_5 中断服务函数
 *   覆盖 PA5(Line5)、PA6(Line6)、PA7(Line7) 的边沿中断
 *   - PA7(A相)/PA6(B相)：仅下降沿（旋转检测）
 *   - PA5(SW)：双边沿（按下+松开，用于长按精确计时）
 *============================================================================*/
void EXTI9_5_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* PA7 — A 相下降沿（旋转检测） */
    if (EXTI_GetITStatus(EXTI_Line7) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line7);
        vTaskNotifyGiveFromISR(xEncoderTaskHandle, &xHigherPriorityTaskWoken);
    }

    /* PA6 — B 相下降沿（辅助判断） */
    if (EXTI_GetITStatus(EXTI_Line6) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line6);
        vTaskNotifyGiveFromISR(xEncoderTaskHandle, &xHigherPriorityTaskWoken);
    }

    /* PA5 — SW 双边沿（按下+松开都通知，用于长按计时） */
    if (EXTI_GetITStatus(EXTI_Line5) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line5);
        vTaskNotifyGiveFromISR(xEncoderTaskHandle, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*==============================================================================
 * EC11 初始化
 *============================================================================*/
void EC11_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    EXTI_InitTypeDef  EXTI_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* 1. 使能 GPIO 和 AFIO 时钟 */
    RCC_APB2PeriphClockCmd(EC11_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    /* 2. 配置引脚为内部上拉输入（IPU），硬件上无需外接上拉电阻 */
    GPIO_InitStructure.GPIO_Pin   = EC11_A_GPIO_PIN | EC11_B_GPIO_PIN | EC11_SW_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(EC11_A_GPIO_PORT, &GPIO_InitStructure);

    /* 3. 检测编码器是否物理连接
     *    如果三引脚全为高（IPU 默认电平），说明无外设驱动，大概率硬件未连接。
     *    此时跳过 EXTI 中断配置，编码器任务通过 5ms 轮询仍可正常工作（延迟略增）。
     *    注意：编码器静止时 A/B 自然为高，可能误判为未连接，但仅影响 EXTI，
     *          轮询保底确保功能完整。 */
    if (EC11_A_READ() && EC11_B_READ() && EC11_SW_READ()) {
        return;   /* 未检测到编码器 → 跳过 EXTI，仅保留 GPIO 配置供轮询使用 */
    }

    /* 4. 将 EXTI 线映射到 GPIO 端口 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource7);  /* A 相 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource6);  /* B 相 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource5);  /* SW   */

    /* 4. 配置 EXTI
     *    A/B 相：仅下降沿（旋转只需检测一个边沿）
     *    SW：   双边沿（按下+松开，用于精确长按计时） */
    EXTI_InitStructure.EXTI_Line    = EXTI_Line7 | EXTI_Line6;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* SW 单独配置双边沿 */
    EXTI_InitStructure.EXTI_Line    = EXTI_Line5;
    EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* 5. 配置 NVIC 优先级
     *    STM32 优先级：数值越小优先级越高
     *    FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
     *    本中断优先级设为 6（数值 > 5），即比 FreeRTOS 管理范围低一级
     *    这样 ISR 内可以安全调用 vTaskNotifyGiveFromISR 等 FreeRTOS API */
    NVIC_InitStructure.NVIC_IRQChannel                   = EXTI9_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
