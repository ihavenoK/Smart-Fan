#ifndef __TASKS_H
#define __TASKS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "system_state.h"

/*==============================================================================
 * Wi-Fi 上报请求类型（通过任务通知值传递）
 *   通知值 = 0x00 → USART 数据到达（ISR 通知）
 *   通知值 = REPORT_xxx → 上报请求（菜单/定时器触发）
 *============================================================================*/
#define REPORT_ALL      0x01   /* 全量上报（温湿度+风速+模式） */
#define REPORT_SPEED    0x02   /* 仅风速 */

/*==============================================================================
 * 编码器事件定义
 *============================================================================*/
typedef enum {
    ENCODER_CW,      /* 顺时针旋转 */
    ENCODER_CCW,     /* 逆时针旋转 */
    ENCODER_SHORT,   /* 短按 */
    ENCODER_LONG     /* 长按 */
} EncoderEventType;

typedef struct {
    EncoderEventType event;
} EncoderMsg;

/*==============================================================================
 * 传感器数据类型
 *============================================================================*/
typedef struct {
    uint8_t temperature;
    uint8_t humidity;
} SensorData_t;

/*==============================================================================
 * 全局内核对象句柄（extern 声明，定义在 main.c）
 *============================================================================*/
extern QueueHandle_t     xEncoderQueue;
extern QueueHandle_t     xSensorQueue;
extern SemaphoreHandle_t xOledMutex;
extern SemaphoreHandle_t xSystemMutex;

/*==============================================================================
 * 任务句柄（供 ISR 使用）
 *============================================================================*/
extern TaskHandle_t xEncoderTaskHandle;
extern TaskHandle_t xWiFiTaskHandle;
extern TaskHandle_t xMotorTaskHandle;
extern TaskHandle_t xMenuTaskHandle;
extern TaskHandle_t xSensorTaskHandle;
extern TaskHandle_t xDisplayTaskHandle;

/*==============================================================================
 * 任务函数声明
 *============================================================================*/
void vTaskEncoder(void *pvParameters);
void vTaskMotor(void *pvParameters);
void vTaskMenu(void *pvParameters);
void vTaskSensor(void *pvParameters);
void vTaskDisplay(void *pvParameters);
void vTaskWiFi(void *pvParameters);

#endif /* __TASKS_H */
