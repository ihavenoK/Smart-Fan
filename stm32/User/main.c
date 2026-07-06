#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// 硬件驱动
#include "delay.h"
#include "dht11.h"
#include "ec11.h"
#include "OLED.h"
#include "motor.h"
#include "debug_serial.h"

// 应用层
#include "tasks.h"
#include "system_state.h"

/*-----------------------------------------------------------*/
/* 全局内核对象句柄定义                                      */
/*-----------------------------------------------------------*/
QueueHandle_t xEncoderQueue = NULL;    // 编码器事件队列
QueueHandle_t xSensorQueue = NULL;     // 传感器数据队列
SemaphoreHandle_t xOledMutex = NULL;   // OLED 互斥锁
SemaphoreHandle_t xSystemMutex = NULL; // 系统状态互斥锁

/*-----------------------------------------------------------*/
/* 全局系统状态（受 xSystemMutex 保护）                      */
/*-----------------------------------------------------------*/
SystemState_t g_SystemState = {
    .mode = MODE_MANUAL,
    .targetSpeed = 0,
    .currentSpeed = 0,
    .temperature = 0,
    .humidity = 0,
    .wifiConnected = 0,
    .timerMinutes = 0
};

/*-----------------------------------------------------------*/
/* 任务句柄（供 ISR 和其他任务使用）                         */
/*-----------------------------------------------------------*/
TaskHandle_t xEncoderTaskHandle = NULL;
TaskHandle_t xWiFiTaskHandle = NULL;
TaskHandle_t xMotorTaskHandle = NULL;
TaskHandle_t xMenuTaskHandle = NULL;
TaskHandle_t xSensorTaskHandle = NULL;
TaskHandle_t xDisplayTaskHandle = NULL;

/*-----------------------------------------------------------*/
/* 硬件初始化（标准库）                                      */
/*-----------------------------------------------------------*/
static void Hardware_Init(void)
{
    // DWT 延时初始化
    Delay_Init();

    // 调试串口（USART1: PA9 TX, PA10 RX, 9600）
    DebugSerial_Init(9600);
    DebugSerial_Printf("\r\n===== Smart Fan v2.0 =====\r\n");
    DebugSerial_Printf("CPU: 72MHz | FreeRTOS\r\n");
    DebugSerial_Printf("USART1 debug OK\r\n");
    DebugSerial_Printf("===========================\r\n\r\n");

    // OLED 初始化（显示启动画面）
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0,  "Smart Fan",   OLED_8X16);
    OLED_ShowString(0, 16, "Starting...", OLED_8X16);
    OLED_Update();

    // 传感器和交互模块初始化
    DHT11_Init();
    EC11_Init();

    // 电机初始化
    Motor_Init();

    // 注意：ESP8266 由 Wi-Fi 任务自行初始化，不在此处进行
}

/*-----------------------------------------------------------*/
/* 创建内核对象（队列、互斥锁）                              */
/*-----------------------------------------------------------*/
static void CreateKernelObjects(void)
{
    xEncoderQueue = xQueueCreate(4, sizeof(EncoderMsg));
    xSensorQueue  = xQueueCreate(2, sizeof(SensorData_t));
    xOledMutex    = xSemaphoreCreateMutex();
    xSystemMutex  = xSemaphoreCreateMutex();

    configASSERT(xEncoderQueue != NULL);
    configASSERT(xSensorQueue != NULL);
    configASSERT(xOledMutex != NULL);
    configASSERT(xSystemMutex != NULL);
}

/*-----------------------------------------------------------*/
/* 创建所有任务                                              */
/*-----------------------------------------------------------*/
static void CreateAllTasks(void)
{
    BaseType_t ret;

    // 编码器任务 - 最高优先级 6
    ret = xTaskCreate(
        vTaskEncoder,
        "Encoder",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        6,
        &xEncoderTaskHandle
    );
    configASSERT(ret == pdPASS);

    // Wi-Fi 任务 - 优先级 5（栈加大防止溢出）
    ret = xTaskCreate(
        vTaskWiFi,
        "WiFi",
        configMINIMAL_STACK_SIZE * 6,  // 4→6 防止 JSON + Debug 打印栈溢出
        NULL,
        5,
        &xWiFiTaskHandle
    );
    configASSERT(ret == pdPASS);

    // 电机控制任务 - 优先级 3
    ret = xTaskCreate(
        vTaskMotor,
        "Motor",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        3,
        &xMotorTaskHandle
    );
    configASSERT(ret == pdPASS);

    // 菜单处理任务 - 优先级 2
    ret = xTaskCreate(
        vTaskMenu,
        "Menu",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        2,
        &xMenuTaskHandle
    );
    configASSERT(ret == pdPASS);

    // 传感器读取任务 - 优先级 1
    ret = xTaskCreate(
        vTaskSensor,
        "Sensor",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        1,
        &xSensorTaskHandle
    );
    configASSERT(ret == pdPASS);

    // 显示刷新任务 - 优先级 1
    ret = xTaskCreate(
        vTaskDisplay,
        "Display",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        1,
        &xDisplayTaskHandle
    );
    configASSERT(ret == pdPASS);
}

/*-----------------------------------------------------------*/
/* 主函数                                                    */
/*-----------------------------------------------------------*/
int main(void)
{
    // 1. 设置中断优先级分组（必须最先执行，FreeRTOS 要求分组4）
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    // 2. 硬件初始化
    Hardware_Init();

    // 3. 创建内核对象（队列、互斥锁）
    CreateKernelObjects();

    // 4. 创建所有任务
    CreateAllTasks();

    // 5. 启动调度器（此函数不会返回）
    vTaskStartScheduler();

    // 正常情况下不会执行到这里
    while(1);
}