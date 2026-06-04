#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/*==============================================================================
 * 引脚定义（可根据实际接线修改）
 *============================================================================*/
#define MOTOR_GPIO_PORT         GPIOB
#define MOTOR_GPIO_CLK          RCC_APB2Periph_GPIOB

#define MOTOR_STBY_PIN          GPIO_Pin_12   /* 待机控制 */
#define MOTOR_AIN1_PIN          GPIO_Pin_13   /* A 通道方向 1 */
#define MOTOR_AIN2_PIN          GPIO_Pin_14   /* A 通道方向 2 */

/* PWM 引脚（PA1 = TIM2_CH2） */
#define MOTOR_PWM_PORT          GPIOA
#define MOTOR_PWM_CLK           RCC_APB2Periph_GPIOA
#define MOTOR_PWM_PIN           GPIO_Pin_1

/*==============================================================================
 * 电机状态枚举
 *============================================================================*/
typedef enum {
    MOTOR_STOP = 0,     /* 停止（高阻态，惯性滑行） */
    MOTOR_FORWARD,      /* 正转 */
    MOTOR_REVERSE,      /* 反转 */
    MOTOR_BRAKE         /* 刹车（短路制动） */
} MotorDirection_t;

/*==============================================================================
 * 函数声明
 *============================================================================*/
void Motor_Init(void);
void Motor_SetSpeed(uint8_t percent);           /* 0~100% */
void Motor_SetDirection(MotorDirection_t dir);
void Motor_Stop(void);                          /* 停止并刹车 */
void Motor_EmergencyStop(void);                 /* 紧急停机（拉低 STBY） */

#endif /* __MOTOR_H */
