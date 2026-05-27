/**
  * @file    menu_task.c
  * @brief   菜单状态机任务
  *          - 从 xEncoderQueue 接收编码器事件
  *          - 状态机处理：主页 / 主菜单 / 模式选择 / 定时设置 / WiFi 重连
  *          - 生成 g_DisplayInfo 供显示任务渲染
  *          - 10 秒无操作自动返回主页
  */
#include "tasks.h"
#include "motor.h"
#include "string.h"
#include "stdio.h"

/*==============================================================================
 * 全局变量定义
 *============================================================================*/
MenuState_t   g_MenuState;
DisplayInfo_t g_DisplayInfo;

/*==============================================================================
 * 前向声明
 *============================================================================*/
static void Menu_ProcessEvent(EncoderMsg *msg);
static void Menu_UpdateDisplay(void);
static void Menu_ResetIdleTimer(void);
static void Menu_CheckIdleTimeout(void);

/*==============================================================================
 * 任务入口
 *============================================================================*/
void vTaskMenu(void *pvParameters)
{
    EncoderMsg msg;
    (void)pvParameters;

    /* 初始化菜单状态 */
    g_MenuState.currentPage    = PAGE_HOME;
    g_MenuState.mainMenuCursor = 0;
    g_MenuState.timerHours     = 0;
    g_MenuState.lastActivity   = xTaskGetTickCount();

    /* 初始化全局显示内容 */
    Menu_UpdateDisplay();

    while (1) {
        /* 等待编码器事件，超时 1 秒（用于空闲检测） */
        if (xQueueReceive(xEncoderQueue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            Menu_ProcessEvent(&msg);
            Menu_UpdateDisplay();
        } else {
            /* 1 秒无事件，检查 10 秒空闲超时 */
            Menu_CheckIdleTimeout();
            /* 即使无事件也刷新（如定时倒计时） */
            Menu_UpdateDisplay();
        }
    }
}

/*==============================================================================
 * 编码器事件处理（状态机核心）
 *============================================================================*/
static void Menu_ProcessEvent(EncoderMsg *msg)
{
    int speed;

    Menu_ResetIdleTimer();   /* 任何操作都重置空闲计时器 */

    switch (g_MenuState.currentPage) {

    /*------ PAGE_HOME：主页面 ------*/
    case PAGE_HOME:
        if (msg->event == ENCODER_CW || msg->event == ENCODER_CCW) {
            /* 仅在手动模式下旋转调风速（步进 5%） */
            if (g_SystemState.mode == MODE_MANUAL) {
                speed = g_SystemState.targetSpeed;
                if (msg->event == ENCODER_CW)
                    speed += 5;
                else
                    speed -= 5;

                if (speed > 100) speed = 100;
                if (speed < 0)   speed = 0;

                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_SystemState.targetSpeed = (uint8_t)speed;
                    xSemaphoreGive(xSystemMutex);
                }
            }
            /* 自动/定时模式下旋转无效果 */
        } else if (msg->event == ENCODER_LONG) {
            /* 长按 → 进入主菜单 */
            g_MenuState.currentPage    = PAGE_MAIN_MENU;
            g_MenuState.mainMenuCursor = 0;
        }
        /* 短按在主页面无作用 */
        break;

    /*------ PAGE_MAIN_MENU：主菜单 ------*/
    case PAGE_MAIN_MENU:
        if (msg->event == ENCODER_CW) {
            g_MenuState.mainMenuCursor++;
            if (g_MenuState.mainMenuCursor > 2)
                g_MenuState.mainMenuCursor = 0;
        } else if (msg->event == ENCODER_CCW) {
            if (g_MenuState.mainMenuCursor == 0)
                g_MenuState.mainMenuCursor = 2;
            else
                g_MenuState.mainMenuCursor--;
        } else if (msg->event == ENCODER_SHORT) {
            /* 根据光标进入子页面 */
            switch (g_MenuState.mainMenuCursor) {
                case 0:
                    g_MenuState.currentPage = PAGE_MODE_SELECT;
                    break;
                case 1:
                    g_MenuState.currentPage = PAGE_TIMER_SET;
                    break;
                case 2:
                    g_MenuState.currentPage = PAGE_WIFI_RECONNECT;
                    break;
            }
        } else if (msg->event == ENCODER_LONG) {
            g_MenuState.currentPage = PAGE_HOME;  /* 长按返回主页 */
        }
        break;

    /*------ PAGE_MODE_SELECT：模式选择 ------*/
    case PAGE_MODE_SELECT:
        if (msg->event == ENCODER_CW || msg->event == ENCODER_CCW) {
            if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (msg->event == ENCODER_CW) {
                    if (g_SystemState.mode == MODE_TIMER)
                        g_SystemState.mode = MODE_MANUAL;
                    else
                        g_SystemState.mode++;
                } else {
                    if (g_SystemState.mode == MODE_MANUAL)
                        g_SystemState.mode = MODE_TIMER;
                    else
                        g_SystemState.mode--;
                }
                xSemaphoreGive(xSystemMutex);
            }
        } else if (msg->event == ENCODER_SHORT || msg->event == ENCODER_LONG) {
            g_MenuState.currentPage = PAGE_HOME;  /* 确认/长按均返回 */
        }
        break;

    /*------ PAGE_TIMER_SET：定时设置 ------*/
    case PAGE_TIMER_SET:
        if (msg->event == ENCODER_CW) {
            if (g_MenuState.timerHours < 12)
                g_MenuState.timerHours++;
        } else if (msg->event == ENCODER_CCW) {
            if (g_MenuState.timerHours > 0)
                g_MenuState.timerHours--;
        } else if (msg->event == ENCODER_SHORT) {
            /* 确认定时，设置倒计时并切换到定时模式 */
            if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_SystemState.mode         = MODE_TIMER;
                g_SystemState.timerMinutes = g_MenuState.timerHours * 60;
                xSemaphoreGive(xSystemMutex);
            }
            g_MenuState.currentPage = PAGE_HOME;
        } else if (msg->event == ENCODER_LONG) {
            g_MenuState.currentPage = PAGE_HOME;  /* 取消返回 */
        }
        break;

    /*------ PAGE_WIFI_RECONNECT：WiFi 重连 ------*/
    case PAGE_WIFI_RECONNECT:
        if (msg->event == ENCODER_SHORT || msg->event == ENCODER_LONG) {
            g_MenuState.currentPage = PAGE_HOME;
        }
        break;
    }
}

/*==============================================================================
 * 根据当前菜单状态生成全局显示字符串
 *============================================================================*/
static void Menu_UpdateDisplay(void)
{
    char line1[21] = {0}, line2[21] = {0}, line3[21] = {0}, line4[21] = {0};
    uint8_t  temp, humi, speed, wifi;
    FanMode_t mode;

    /* 读取系统状态 */
    if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        temp  = g_SystemState.temperature;
        humi  = g_SystemState.humidity;
        speed = g_SystemState.currentSpeed;
        wifi  = g_SystemState.wifiConnected;
        mode  = g_SystemState.mode;
        xSemaphoreGive(xSystemMutex);
    } else {
        return;  /* 拿不到锁，保持上次显示 */
    }

    switch (g_MenuState.currentPage) {

    /*------ 主页 ------*/
    case PAGE_HOME:
        snprintf(line1, sizeof(line1), "T:%dC H:%d%%", temp, humi);
        snprintf(line2, sizeof(line2), "Speed: %d%%", speed);
        if (mode == MODE_MANUAL)
            snprintf(line3, sizeof(line3), "Mode: MANUAL");
        else if (mode == MODE_AUTO)
            snprintf(line3, sizeof(line3), "Mode: AUTO");
        else if (mode == MODE_TIMER) {
            uint16_t mins = g_SystemState.timerMinutes;
            snprintf(line3, sizeof(line3), "Mode: TIMER %d:%02d",
                     mins / 60, mins % 60);
        }
        snprintf(line4, sizeof(line4), "WiFi: %s", wifi ? "OK" : "NO");
        break;

    /*------ 主菜单 ------*/
    case PAGE_MAIN_MENU:
        snprintf(line1, sizeof(line1), "== Main Menu ==");
        snprintf(line2, sizeof(line2), "%c Mode Select",
                 (g_MenuState.mainMenuCursor == 0) ? '>' : ' ');
        snprintf(line3, sizeof(line3), "%c Set Timer",
                 (g_MenuState.mainMenuCursor == 1) ? '>' : ' ');
        snprintf(line4, sizeof(line4), "%c Reconnect WiFi",
                 (g_MenuState.mainMenuCursor == 2) ? '>' : ' ');
        break;

    /*------ 模式选择 ------*/
    case PAGE_MODE_SELECT:
        snprintf(line1, sizeof(line1), "== Select Mode ==");
        snprintf(line2, sizeof(line2), " %s",
                 (mode == MODE_MANUAL) ? "[MANUAL]" : " MANUAL");
        snprintf(line3, sizeof(line3), " %s",
                 (mode == MODE_AUTO)   ? "[AUTO]"   : " AUTO");
        snprintf(line4, sizeof(line4), " %s",
                 (mode == MODE_TIMER)  ? "[TIMER]"  : " TIMER");
        break;

    /*------ 定时设置 ------*/
    case PAGE_TIMER_SET:
        snprintf(line1, sizeof(line1), "== Set Timer ==");
        snprintf(line2, sizeof(line2), "Hours: %d", g_MenuState.timerHours);
        snprintf(line3, sizeof(line3), "(Short to confirm)");
        snprintf(line4, sizeof(line4), "(Long to cancel)");
        break;

    /*------ WiFi 重连 ------*/
    case PAGE_WIFI_RECONNECT:
        snprintf(line1, sizeof(line1), "Reconnecting...");
        snprintf(line2, sizeof(line2), "Please wait");
        snprintf(line3, sizeof(line3), "");
        snprintf(line4, sizeof(line4), "Press any key back");
        break;
    }

    /* 复制到全局显示信息 */
    strncpy(g_DisplayInfo.line1, line1, 21);
    strncpy(g_DisplayInfo.line2, line2, 21);
    strncpy(g_DisplayInfo.line3, line3, 21);
    strncpy(g_DisplayInfo.line4, line4, 21);
}

/*==============================================================================
 * 辅助函数
 *============================================================================*/
static void Menu_ResetIdleTimer(void)
{
    g_MenuState.lastActivity = xTaskGetTickCount();
}

/**
  * @brief  检查空闲超时，10 秒无操作自动返回主页
  */
static void Menu_CheckIdleTimeout(void)
{
    if (g_MenuState.currentPage != PAGE_HOME) {
        if ((xTaskGetTickCount() - g_MenuState.lastActivity)
            >= pdMS_TO_TICKS(10000)) {
            g_MenuState.currentPage = PAGE_HOME;
            Menu_UpdateDisplay();
        }
    }
}
