/**
  * @file    motor_task.c
  * @brief   电机控制任务
  *          - 每 50ms 从 g_SystemState 读取目标速度
  *          - 斜坡加减速平滑过渡（每周期 ±1%）
  *          - 更新全局状态中的当前实际速度
  */
#include "tasks.h"
#include "motor.h"

void vTaskMotor(void *pvParameters)
{
    uint8_t   targetSpeed;
    uint8_t   currentSpeed = 0;
    TickType_t xLastWakeTime;

    (void)pvParameters;

    /* 记录任务启动时刻（用于精确周期控制） */
    xLastWakeTime = xTaskGetTickCount();

    while (1) {
        /* 1. 读取目标速度（受互斥锁保护） */
        if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            targetSpeed = g_SystemState.targetSpeed;
            xSemaphoreGive(xSystemMutex);
        }
        /* 拿不到锁时保持上次目标值 */

        /* 2. 斜坡加减速（平滑过渡，减少冲击） */
        if (currentSpeed < targetSpeed) {
            currentSpeed++;
        } else if (currentSpeed > targetSpeed) {
            currentSpeed--;
        }

        /* 3. 写入硬件 */
        Motor_SetSpeed(currentSpeed);

        /* 4. 更新全局状态中的实际速度 */
        if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            g_SystemState.currentSpeed = currentSpeed;
            xSemaphoreGive(xSystemMutex);
        }

        /* 5. 精确 50ms 周期 */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}
