/**
  * @file    sensor_task.c
  * @brief   温湿度传感器采集任务
  *          - 每 2 秒读取一次 DHT11
  *          - 成功时：发送 SensorData_t 到 xSensorQueue，更新 g_SystemState
  *          - 失败时：仅延时，等待下次重试
  */
#include "tasks.h"
#include "dht11.h"

void vTaskSensor(void *pvParameters)
{
    SensorData_t data;
    (void)pvParameters;

    /* 上电后等待 DHT11 稳定 */
    vTaskDelay(pdMS_TO_TICKS(1500));

    while (1) {
        if (DHT11_ReadData(&data.temperature, &data.humidity) == 0) {
            /*-- 推送到传感器数据队列（不阻塞） --*/
            xQueueSend(xSensorQueue, &data, 0);

            /*-- 更新全局系统状态（互斥锁保护） --*/
            if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_SystemState.temperature = data.temperature;
                g_SystemState.humidity    = data.humidity;
                xSemaphoreGive(xSystemMutex);
            }
        }
        /* 读取失败时静默跳过，等下一周期重试 */

        vTaskDelay(pdMS_TO_TICKS(2000));   /* 每 2 秒采集一次 */
    }
}
