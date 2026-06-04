#ifndef __EC11_H
#define __EC11_H

#include "stm32f10x.h"

/*==============================================================================
 * 引脚定义（可根据实际接线修改）
 *    EC11_A  → PA7   (旋转 A 相)
 *    EC11_B  → PA6   (旋转 B 相)
 *    EC11_SW → PA5   (按键)
 *============================================================================*/
#define EC11_A_GPIO_PORT       GPIOA
#define EC11_A_GPIO_PIN        GPIO_Pin_7
#define EC11_B_GPIO_PORT       GPIOA
#define EC11_B_GPIO_PIN        GPIO_Pin_6
#define EC11_SW_GPIO_PORT      GPIOA
#define EC11_SW_GPIO_PIN       GPIO_Pin_5
#define EC11_GPIO_CLK          RCC_APB2Periph_GPIOA

/*==============================================================================
 * 引脚电平读取宏
 *============================================================================*/
#define EC11_A_READ()   GPIO_ReadInputDataBit(EC11_A_GPIO_PORT, EC11_A_GPIO_PIN)
#define EC11_B_READ()   GPIO_ReadInputDataBit(EC11_B_GPIO_PORT, EC11_B_GPIO_PIN)
#define EC11_SW_READ()  GPIO_ReadInputDataBit(EC11_SW_GPIO_PORT, EC11_SW_GPIO_PIN)

/*==============================================================================
 * 函数声明
 *============================================================================*/
void EC11_Init(void);

#endif /* __EC11_H */
