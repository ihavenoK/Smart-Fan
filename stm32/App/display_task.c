/**
  * @file    display_task.c
  * @brief   OLED 显示刷新任务
  *          - 每 200ms 渲染一次 g_DisplayInfo 到 OLED
  *          - g_DisplayInfo 由菜单任务生成，显示任务只负责渲染
  *          - 128x64 屏幕，OLED_8X16 字体 → 16 列 × 4 行
  *            行坐标: Line0→Y=0, Line1→Y=16, Line2→Y=32, Line3→Y=48
  */
#include "tasks.h"
#include "OLED.h"

void vTaskDisplay(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        /* 获取 OLED 互斥锁 */
        if (xSemaphoreTake(xOledMutex, pdMS_TO_TICKS(100)) == pdTRUE) {

            OLED_Clear();

            /* 逐行渲染菜单任务准备好的字符串 */
            OLED_ShowString(0, 0,  g_DisplayInfo.line1, OLED_8X16);
            OLED_ShowString(0, 16, g_DisplayInfo.line2, OLED_8X16);
            OLED_ShowString(0, 32, g_DisplayInfo.line3, OLED_8X16);
            OLED_ShowString(0, 48, g_DisplayInfo.line4, OLED_8X16);

            OLED_Update();   /* 刷新显存到屏幕 */

            xSemaphoreGive(xOledMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(200));   /* 200ms 刷新周期 */
    }
}
