#ifndef __SYSTEM_CONFIG_H
#define __SYSTEM_CONFIG_H

/*==============================================================================
 * 智能风扇 — 全局宏定义
 *============================================================================*/

/* 硬件版本 */
#define FW_VERSION     "1.0.0"

/* 风扇控制参数 */
#define FAN_SPEED_MIN   0       /* 最小风速 % */
#define FAN_SPEED_MAX   100     /* 最大风速 % */
#define FAN_PWM_FREQ    25000   /* PWM 频率 (Hz) */

/* 温度阈值（自动模式） */
#define TEMP_LOW        24      /* 低于此温度关闭风扇 */
#define TEMP_MID        28      /* 中速档温度 */
#define TEMP_HIGH       32      /* 高速档温度 */

/* 定时器参数 */
#define TIMER_MAX_MIN   120     /* 最大定时 (分钟) */
#define TIMER_STEP      10      /* 定时步进 (分钟) */

/* 传感器 */
#define SENSOR_INTERVAL_MS  2000    /* 采集间隔 */

#endif /* __SYSTEM_CONFIG_H */
