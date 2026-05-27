#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

//-----------------------------------------------------------
// 硬件相关
//-----------------------------------------------------------
#define configCPU_CLOCK_HZ                     ( 72000000UL )
#define configTICK_RATE_HZ                     ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                   ( 8 )
#define configMINIMAL_STACK_SIZE               ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                  ( ( size_t ) ( 15 * 1024 ) )
#define configMAX_TASK_NAME_LEN                ( 16 )
#define configUSE_16_BIT_TICKS                 0
#define configIDLE_SHOULD_YIELD                1
#define configUSE_PREEMPTION                   1
#define configUSE_TIME_SLICING                 1

//-----------------------------------------------------------
// 软件定时器
//-----------------------------------------------------------
#define configUSE_TIMERS                       1
#define configTIMER_TASK_PRIORITY             ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH               10
#define configTIMER_TASK_STACK_DEPTH          ( configMINIMAL_STACK_SIZE * 2 )

//-----------------------------------------------------------
// 同步与通信
//-----------------------------------------------------------
#define configUSE_MUTEXES                      1
#define configUSE_RECURSIVE_MUTEXES            1
#define configUSE_COUNTING_SEMAPHORES          1
#define configUSE_QUEUE_SETS                   1
#define configUSE_TASK_NOTIFICATIONS           1
#define configUSE_TRACE_FACILITY               1

//-----------------------------------------------------------
// 内存管理 (动态分配，使用 heap_4.c)
//-----------------------------------------------------------
#define configSUPPORT_STATIC_ALLOCATION        0
#define configSUPPORT_DYNAMIC_ALLOCATION       1

//-----------------------------------------------------------
// Hooks
//-----------------------------------------------------------
#define configUSE_IDLE_HOOK                    0
#define configUSE_TICK_HOOK                    0
#define configUSE_MALLOC_FAILED_HOOK           0
#define configCHECK_FOR_STACK_OVERFLOW         2

//-----------------------------------------------------------
// 中断配置 (Cortex-M3 优先级分组 4)
//-----------------------------------------------------------
#define configKERNEL_INTERRUPT_PRIORITY       ( 15 << 4 )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( 5 << 4 )

//-----------------------------------------------------------
// 断言
//-----------------------------------------------------------
#define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

//-----------------------------------------------------------
// API 包含选项
//-----------------------------------------------------------
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelay                     1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xTimerPendFunctionCall         1
#define INCLUDE_uxTaskGetStackHighWaterMark    1

// 声明断言回调函数
extern void vAssertCalled( const char *pcFile, unsigned long ulLine );

#endif /* FREERTOS_CONFIG_H */