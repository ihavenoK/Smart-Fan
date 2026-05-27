#include "FreeRTOS.h"
#include "task.h"

//-----------------------------------------------------------
// 断言失败回调（configASSERT 需要）
//-----------------------------------------------------------
void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    // 进入无限循环，方便调试时暂停在此处
    taskDISABLE_INTERRUPTS();
    while(1)
    {
    }
    // 避免编译器警告
    (void)pcFile;
    (void)ulLine;
}

//-----------------------------------------------------------
// 堆栈溢出钩子函数（configCHECK_FOR_STACK_OVERFLOW == 2 时必须）
//-----------------------------------------------------------
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // 堆栈溢出处理：记录出错任务名，进入死循环以便调试
    // 实际产品中可尝试保存日志或重启，这里简单处理
    taskDISABLE_INTERRUPTS();
    while(1)
    {
    }
    // 避免编译器警告
    (void)xTask;
    (void)pcTaskName;
}

//-----------------------------------------------------------
// 其他可能需要的钩子（如果 configUSE_IDLE_HOOK == 1 则需实现）
// 目前配置为 0，可以不加
// void vApplicationIdleHook(void){}
//-----------------------------------------------------------