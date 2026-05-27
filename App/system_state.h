#ifndef __SYSTEM_STATE_H
#define __SYSTEM_STATE_H

#include "stdint.h"
#include "FreeRTOS.h"

/*==============================================================================
 * 风扇运行模式
 *============================================================================*/
typedef enum {
    MODE_MANUAL = 0,   /* 手动调速 */
    MODE_AUTO,         /* 自动温控 */
    MODE_TIMER         /* 定时关机 */
} FanMode_t;

/*==============================================================================
 * 菜单页面枚举
 *============================================================================*/
typedef enum {
    PAGE_HOME = 0,          /* 主页面 */
    PAGE_MAIN_MENU,         /* 主菜单 */
    PAGE_MODE_SELECT,       /* 子菜单：切换模式 */
    PAGE_TIMER_SET,         /* 子菜单：设置定时 */
    PAGE_WIFI_RECONNECT     /* 子菜单：重连 WiFi */
} MenuPage_t;

/*==============================================================================
 * 系统全局状态
 *============================================================================*/
typedef struct {
    FanMode_t mode;           /* 当前模式 */
    uint8_t   targetSpeed;    /* 目标风速 (0~100%) */
    uint8_t   currentSpeed;   /* 当前实际风速 */
    uint8_t   temperature;    /* 最新温度值 */
    uint8_t   humidity;       /* 最新湿度值 */
    uint8_t   wifiConnected;  /* Wi-Fi 连接状态 (0/1) */
    uint32_t  timerMinutes;   /* 定时关机剩余分钟数 */
} SystemState_t;

/*==============================================================================
 * 菜单状态
 *============================================================================*/
typedef struct {
    MenuPage_t currentPage;    /* 当前页面 */
    uint8_t    mainMenuCursor; /* 主菜单光标 (0:模式, 1:定时, 2:WiFi) */
    uint8_t    timerHours;     /* 定时关机小时数 (0~12) */
    TickType_t lastActivity;   /* 上次用户操作的时间戳 */
} MenuState_t;

/*==============================================================================
 * 全局显示信息（菜单任务生成 → 显示任务渲染）
 *   每行为 "\0" 结尾的字符串，OLED 逐行显示
 *============================================================================*/
typedef struct {
    char line1[21];   /* OLED 第一行 */
    char line2[21];   /* 第二行 */
    char line3[21];   /* 第三行 */
    char line4[21];   /* 第四行（菜单等） */
} DisplayInfo_t;

/*==============================================================================
 * extern 声明（定义在各自 .c 文件中）
 *============================================================================*/
extern SystemState_t  g_SystemState;   /* main.c */
extern MenuState_t    g_MenuState;     /* menu_task.c */
extern DisplayInfo_t  g_DisplayInfo;   /* menu_task.c */

#endif /* __SYSTEM_STATE_H */
