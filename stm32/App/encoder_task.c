/**
  * @file    encoder_task.c
  * @brief   EC11 编码器任务（工业标准消抖算法）
  *
  *   旋转消抖：Ben Buxton 四倍频状态机
  *     - 基于 A/B 相完整格雷码状态转换表，每个定位点产生 1 个事件
  *     - 天然抗抖动：非法状态转换被表映射为 0（忽略），无需额外延时
  *     - 5ms 轮询（200Hz），可检测最高 ~150RPM 旋转速度
  *
  *   按键消抖：标准计数器法（嵌入式最常用消抖算法）
  *     - 读取与稳定值不同 → 计数器递增
  *     - 读取与稳定值相同 → 计数器清零
  *     - 计数器达到阈值 → 确认状态翻转
  *     - 5ms×10=50ms 消抖窗口，兼顾速度与可靠性
  *
  *   长按判定：按住 ≥ 3 秒立即发送 ENCODER_LONG（不等松开）
  *             < 3 秒松开 → ENCODER_SHORT
  *
  *   参考文献：
  *     [1] Ben Buxton, "Rotary encoder interrupt service routine",
  *         https://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
  *     [2] Jack Ganssle, "A Guide to Debouncing",
  *         https://www.ganssle.com/debouncing.htm
  */
#include "tasks.h"
#include "ec11.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* 队列句柄（在 main.c 中定义） */
extern QueueHandle_t xEncoderQueue;

/*==============================================================================
 * 常量
 *============================================================================*/
#define POLL_INTERVAL_MS          5    /* 轮询间隔 5ms (200Hz) */
#define SW_DEBOUNCE_COUNT        10    /* 按键消抖：连续 10 次 (50ms) 不同才确认 */
#define LONG_PRESS_MS          3000    /* 长按阈值 3 秒 */
#define STEPS_PER_DETENT          4    /* 每定位点步数（四倍频解码=4） */

/*==============================================================================
 * Ben Buxton 旋转编码器状态转换表
 *
 *   索引 = (old_AB << 2) | new_AB
 *   AB 编码: A=bit1, B=bit0
 *   值:  0=无变化, +1=CW一步, -1=CCW一步
 *
 *   合法 CW 序列:  0→2→3→1→0  (00→10→11→01→00)
 *   合法 CCW 序列: 0→1→3→2→0  (00→01→11→10→00)
 *   非法跳转（抖动）: 全部映射为 0，天然消抖
 *============================================================================*/
static const int8_t enc_states[16] = {
     0, -1,  1,  0,   /* old=00 */
     1,  0,  0, -1,   /* old=01 */
    -1,  0,  0,  1,   /* old=10 */
     0,  1, -1,  0    /* old=11 */
};

/*==============================================================================
 * vTaskEncoder
 *============================================================================*/
void vTaskEncoder(void *pvParameters)
{
    EncoderMsg msg;

    /*--- 旋转编码器变量 ---*/
    uint8_t  enc_ab    = 0;    /* 上次 A/B 相读数 (2-bit) */
    int8_t   enc_accum = 0;    /* 步进累加器，满 ±STEPS_PER_DETENT 发一次事件 */

    /* 初始化 enc_ab：读取当前 A/B 相电平 */
    if (EC11_A_READ()) enc_ab |= 0x02;
    if (EC11_B_READ()) enc_ab |= 0x01;

    /*--- 按键变量 ---*/
    uint8_t    sw_stable     = 1;   /* 消抖后稳定电平（上拉，默认 1=松开） */
    uint8_t    sw_counter    = 0;   /* 消抖计数器 */
    TickType_t sw_press_tick = 0;   /* 确认按下的时刻 */
    uint8_t    sw_long_sent  = 0;   /* 长按事件是否已发送，防止重复 */

    (void)pvParameters;

    while (1) {
        /*--------------------------------------------------
         * 等待唤醒
         *   - 5ms 超时：定期轮询（保底扫描）
         *   - ISR 通知：有边沿变化时立即唤醒（降低延迟）
         *--------------------------------------------------*/
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_INTERVAL_MS));

        /*==========================================================
         * 1. 旋转编码器：Ben Buxton 四倍频状态机
         *==========================================================*/
        {
            /* 读取当前 A/B 相，编码为 2-bit */
            uint8_t new_ab = 0;
            if (EC11_A_READ()) new_ab |= 0x02;
            if (EC11_B_READ()) new_ab |= 0x01;

            /* 构造查表索引: (旧状态 << 2) | 新状态 */
            uint8_t idx = (enc_ab << 2) | new_ab;
            enc_ab = new_ab;   /* 保存当前状态供下次使用 */

            /* 查表获取方向: +1=CW, -1=CCW, 0=无变化/抖动 */
            int8_t dir = enc_states[idx];
            enc_accum += dir;

            /* 累加到 STEPS_PER_DETENT → 一个完整的定位点 */
            if (enc_accum >= STEPS_PER_DETENT) {
                msg.event = ENCODER_CW;
                xQueueSend(xEncoderQueue, &msg, 0);
                enc_accum = 0;
            } else if (enc_accum <= -STEPS_PER_DETENT) {
                msg.event = ENCODER_CCW;
                xQueueSend(xEncoderQueue, &msg, 0);
                enc_accum = 0;
            }
        }

        /*==========================================================
         * 2. 按键消抖：标准计数器法 (Ganssle 推荐方案)
         *
         *   原理：采样与稳定值不同时递增计数，相同时清零。
         *         计数达到阈值确认翻转。单次毛刺无法翻转状态。
         *==========================================================*/
        {
            uint8_t sw_raw = EC11_SW_READ();

            if (sw_raw != sw_stable) {
                /* 读取与稳定值不同 → 计数器递增 */
                sw_counter++;
                if (sw_counter >= SW_DEBOUNCE_COUNT) {
                    /* 连续 N 次不同 → 确认状态翻转 */
                    sw_stable  = sw_raw;
                    sw_counter = 0;

                    if (sw_stable == 0) {
                        /* ---- 确认按下 ---- */
                        sw_press_tick = xTaskGetTickCount();
                        sw_long_sent  = 0;
                    } else {
                        /* ---- 确认松开 ---- */
                        if (!sw_long_sent) {
                            /* 3秒内松开 → 短按 */
                            msg.event = ENCODER_SHORT;
                            xQueueSend(xEncoderQueue, &msg, 0);
                        }
                        /* 已发送过长按 → 松开时不再重复 */
                    }
                }
            } else {
                /* 读取与稳定值相同 → 重置计数器 */
                sw_counter = 0;
            }

            /* ---- 长按检测（每次轮询都检查，不需等松开） ---- */
            if (sw_stable == 0 && !sw_long_sent) {
                if ((xTaskGetTickCount() - sw_press_tick)
                        >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                    msg.event = ENCODER_LONG;
                    xQueueSend(xEncoderQueue, &msg, 0);
                    sw_long_sent = 1;
                }
            }
        }
    }
}
