/**
  * @file    dht11.c
  * @brief   DHT11 温湿度传感器驱动（适配 FreeRTOS）
  * @note    使用 DWT 周期计数器实现微秒级精确延时，
  *          20ms 启动信号使用 vTaskDelay 允许任务切换
  */
#include "dht11.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

/*==============================================================================
 * 宏定义：引脚模式切换 & 读写
 *============================================================================*/
#define DHT11_OUT()  do {                                       \
        GPIO_InitTypeDef GPIO_InitStruct;                        \
        GPIO_InitStruct.GPIO_Pin   = DHT11_GPIO_PIN;             \
        GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;           \
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;           \
        GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);            \
    } while(0)

#define DHT11_IN()   do {                                       \
        GPIO_InitTypeDef GPIO_InitStruct;                        \
        GPIO_InitStruct.GPIO_Pin   = DHT11_GPIO_PIN;             \
        GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_IN_FLOATING;      \
        GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;           \
        GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);            \
    } while(0)

#define DHT11_READ_PIN()   GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN)
#define DHT11_WRITE_PIN(x) GPIO_WriteBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN, (BitAction)(x))

/*==============================================================================
 * 初始化
 *============================================================================*/

/**
 * @brief  初始化 DHT11 的 GPIO（DWT 延时由 delay.c 统一管理）
 */
void DHT11_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIO 时钟 */
    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);

    /* 默认输出高电平 */
    GPIO_InitStructure.GPIO_Pin   = DHT11_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
    DHT11_WRITE_PIN(1);
}

/*==============================================================================
 * 读取一个字节
 *============================================================================*/

/**
  * @brief  从 DHT11 读取一个字节（8 位）
  * @retval 读取到的字节
  */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t i, byte = 0;

    for (i = 0; i < 8; i++) {
        /* 等待低电平结束（DHT11 拉低 ~50us 后释放） */
        while (DHT11_READ_PIN() == 0);

        /* 延时 30us 后判断高电平宽度 */
        Delay_us(30);

        if (DHT11_READ_PIN() == 1) {
            /* 高电平 > 30us，判定为 bit=1 */
            byte |= (1 << (7 - i));
            /* 等待高电平结束 */
            while (DHT11_READ_PIN() == 1);
        }
        /* 若高电平 < 30us，bit=0，无需额外操作 */
    }
    return byte;
}

/*==============================================================================
 * 读取一次完整的温湿度数据
 *============================================================================*/

/**
  * @brief  读取一次 DHT11 数据
  * @param  temp: 温度输出 (整数部分)
  * @param  humi: 湿度输出 (整数部分)
  * @retval 0 = 成功, 1 = 失败 (无响应/校验错)
  * @note   20ms 启动信号期间会调用 vTaskDelay 允许其他任务运行，
  *         之后的 bit-banging 在临界区内执行以保证时序。
  */
uint8_t DHT11_ReadData(uint8_t *temp, uint8_t *humi)
{
    uint8_t buf[5];
    uint8_t i;

    /*------------------ 1. 发送启动信号（允许调度） ------------------*/
    DHT11_OUT();
    DHT11_WRITE_PIN(0);
    vTaskDelay(pdMS_TO_TICKS(20));   /* 拉低 ≥18ms，同时允许任务切换 */
    DHT11_WRITE_PIN(1);
    Delay_us(30);               /* 拉高 20~40us */
    DHT11_IN();                      /* 释放总线 */

    /*------------------ 2. 临界区内完成时序敏感的通信 ------------------*/
    taskENTER_CRITICAL();

    /* 等待 DHT11 响应（低 80us + 高 80us） */
    if (DHT11_READ_PIN() == 0) {
        while (DHT11_READ_PIN() == 0);
    } else {
        taskEXIT_CRITICAL();
        return 1;   /* 无应答 */
    }

    if (DHT11_READ_PIN() == 1) {
        while (DHT11_READ_PIN() == 1);
    } else {
        taskEXIT_CRITICAL();
        return 1;
    }

    /* 读取 40 bit → 5 字节 */
    for (i = 0; i < 5; i++) {
        buf[i] = DHT11_ReadByte();
    }

    taskEXIT_CRITICAL();

    /*------------------ 3. 校验 ------------------*/
    if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) {
        *humi = buf[0];   /* 湿度整数 */
        *temp = buf[2];   /* 温度整数 */
        return 0;
    }

    return 1;   /* 校验失败 */
}
