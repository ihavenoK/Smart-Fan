/**
  * @file    motor.c
  * @brief   TB6612FNG 电机驱动
  *          STBY → PB12, AIN1 → PB13, AIN2 → PB14
  *          PWM  → PA1 (TIM2_CH2), 10kHz
  */
#include "motor.h"

/*==============================================================================
 * 宏：引脚控制
 *============================================================================*/
#define AIN1_HIGH()   GPIO_SetBits(MOTOR_GPIO_PORT, MOTOR_AIN1_PIN)
#define AIN1_LOW()    GPIO_ResetBits(MOTOR_GPIO_PORT, MOTOR_AIN1_PIN)
#define AIN2_HIGH()   GPIO_SetBits(MOTOR_GPIO_PORT, MOTOR_AIN2_PIN)
#define AIN2_LOW()    GPIO_ResetBits(MOTOR_GPIO_PORT, MOTOR_AIN2_PIN)
#define STBY_HIGH()   GPIO_SetBits(MOTOR_GPIO_PORT, MOTOR_STBY_PIN)
#define STBY_LOW()    GPIO_ResetBits(MOTOR_GPIO_PORT, MOTOR_STBY_PIN)

/*==============================================================================
 * 定时器 PWM 初始化（TIM2_CH2, PA1）
 *============================================================================*/
static void Motor_PWM_Init(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef        TIM_OCInitStructure;
    GPIO_InitTypeDef         GPIO_InitStructure;

    /* 1. 开启时钟 */
    RCC_APB2PeriphClockCmd(MOTOR_PWM_CLK, ENABLE);          /* GPIOA */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);    /* TIM2 在 APB1 */

    /* 2. 配置 PA1 为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = MOTOR_PWM_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_PWM_PORT, &GPIO_InitStructure);

    /* 3. 定时器时基配置
     *    PWM 频率 = 72MHz / (psc+1) / (period+1)
     *    选 10kHz: 72M / 72 / 100 = 10kHz */
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;
    TIM_TimeBaseStructure.TIM_Period            = 100 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* 4. PWM 输出通道配置（TIM2_CH2） */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 0;                    /* 初始占空比 0% */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    /* 5. 使能 TIM2 */
    TIM_Cmd(TIM2, ENABLE);
}

/*==============================================================================
 * 电机 GPIO 初始化
 *============================================================================*/
static void Motor_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(MOTOR_GPIO_CLK, ENABLE);

    /* STBY、AIN1、AIN2 均设为推挽输出 */
    GPIO_InitStructure.GPIO_Pin   = MOTOR_STBY_PIN | MOTOR_AIN1_PIN | MOTOR_AIN2_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MOTOR_GPIO_PORT, &GPIO_InitStructure);

    /* 初始状态：待机（低电平），芯片不工作 */
    STBY_LOW();
    AIN1_LOW();
    AIN2_LOW();
}

/*==============================================================================
 * 电机总初始化
 *============================================================================*/
void Motor_Init(void)
{
    Motor_GPIO_Init();
    Motor_PWM_Init();

    /* 初始化完成后使能芯片 */
    STBY_HIGH();
}

/*==============================================================================
 * 设置速度（0~100%）
 *============================================================================*/
void Motor_SetSpeed(uint8_t percent)
{
    uint16_t pulse;

    if (percent > 100)
        percent = 100;

    /* ARR = 100-1，所以 CCR 范围 0~99，1% = 1 个计数 */
    pulse = percent;

    TIM_SetCompare2(TIM2, pulse);
}

/*==============================================================================
 * 设置方向
 *============================================================================*/
void Motor_SetDirection(MotorDirection_t dir)
{
    switch (dir) {
        case MOTOR_FORWARD:
            AIN1_HIGH();
            AIN2_LOW();
            break;

        case MOTOR_REVERSE:
            AIN1_LOW();
            AIN2_HIGH();
            break;

        case MOTOR_BRAKE:
            /* 短路制动：两个下管同时导通 */
            AIN1_LOW();
            AIN2_LOW();
            break;

        case MOTOR_STOP:
        default:
            /* 停止：高阻态，惯性滑行 */
            AIN1_LOW();
            AIN2_LOW();
            break;
    }
}

/*==============================================================================
 * 停止（先刹车再降速）
 *============================================================================*/
void Motor_Stop(void)
{
    Motor_SetSpeed(0);
    Motor_SetDirection(MOTOR_BRAKE);
}

/*==============================================================================
 * 紧急停机（直接拉低 STBY，所有通道强制关闭）
 *============================================================================*/
void Motor_EmergencyStop(void)
{
    STBY_LOW();
    Motor_SetSpeed(0);
}
