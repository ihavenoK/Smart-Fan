/**
  * @file    sensor_task.c
  * @brief   温湿度传感器采集任务
  *          - 每 2 秒读取一次 DHT11
  *          - 成功时：发送 SensorData_t 到 xSensorQueue，更新 g_SystemState
  *          - 自动模式：根据温度自动调节目标风速（带 1°C 滞回，防抖动）
  *          - 失败时：仅延时，等待下次重试
  */
#include "tasks.h"
#include "dht11.h"
#include "system_config.h"

void vTaskSensor(void *pvParameters)
{
    SensorData_t data;
    uint8_t      auto_speed = 0;   /* 自动模式当前风速，用于滞回比较 */
    (void)pvParameters;

    /* 上电后等待 DHT11 稳定 */
    vTaskDelay(pdMS_TO_TICKS(1500));

    while (1) {
        if (DHT11_ReadData(&data.temperature, &data.humidity) == 0) {
            /*-- 推送到传感器数据队列（不阻塞） --*/
            xQueueSend(xSensorQueue, &data, 0);

            /*-- 更新全局系统状态 + 自动模式调速（互斥锁保护） --*/
            if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_SystemState.temperature = data.temperature;
                g_SystemState.humidity    = data.humidity;

                /*=====================================================
                 * 自动模式：根据温度设置目标风速
                 *
                 *  档位   温度范围           风速
                 *  ─────────────────────────────────
                 *  L0     < TEMP_LOW (24°C)    0%
                 *  L1     24 ~ 27°C           30%
                 *  L2     28 ~ 31°C           60%
                 *  L3     ≥ TEMP_HIGH (32°C)  100%
                 *
                 *  带 ±1°C 滞回：升温需超出阈值 1°C 才升档，
                 *  降温需低于阈值 1°C 才降档，防止温度在临界
                 *  点附近波动时频繁切换风速。
                 *=====================================================*/
                if (g_SystemState.mode == MODE_AUTO) {
                    uint8_t temp = data.temperature;
                    uint8_t calc_level, calc_speed;

                    /* 1. 基础分档 */
                    if (temp < TEMP_LOW) {
                        calc_level = 0;  calc_speed = 0;
                    } else if (temp < TEMP_MID) {
                        calc_level = 1;  calc_speed = 30;
                    } else if (temp < TEMP_HIGH) {
                        calc_level = 2;  calc_speed = 60;
                    } else {
                        calc_level = 3;  calc_speed = 100;
                    }

                    /* 2. 计算当前档位 */
                    uint8_t auto_level;
                    if      (auto_speed == 0)   auto_level = 0;
                    else if (auto_speed == 30)  auto_level = 1;
                    else if (auto_speed == 60)  auto_level = 2;
                    else                        auto_level = 3;

                    /* 3. 滞回：逐级升降，每级需跨越 ±1°C 确认 */
                    if (calc_level > auto_level) {
                        /* 升温升档 */
                        switch (auto_level) {
                            case 0: if (temp >= TEMP_LOW  + 1) auto_speed = 30; break;
                            case 1: if (temp >= TEMP_MID   + 1) auto_speed = 60; break;
                            case 2: if (temp >= TEMP_HIGH  + 1) auto_speed = 100; break;
                            default: break;
                        }
                    } else if (calc_level < auto_level) {
                        /* 降温降档 */
                        switch (auto_level) {
                            case 3: if (temp <= TEMP_HIGH - 1) auto_speed = 60; break;
                            case 2: if (temp <= TEMP_MID  - 1) auto_speed = 30; break;
                            case 1: if (temp <= TEMP_LOW  - 1) auto_speed = 0;  break;
                            default: break;
                        }
                    }
                    /* 同档 → 不变 */

                    g_SystemState.targetSpeed = auto_speed;
                }

                xSemaphoreGive(xSystemMutex);
            }
        }
        /* 读取失败时静默跳过，等下一周期重试 */

        vTaskDelay(pdMS_TO_TICKS(2000));   /* 每 2 秒采集一次 */
    }
}
