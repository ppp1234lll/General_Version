/*
 * @Description: 以太网模块头文件
 * @Version: v1.0.0
 * @Autor: gxf
 * @Date: 2022-02-05 22:07:32
 * @LastEditors: gxf
 * @LastEditTime: 2022-02-06 16:43:03
 */

#ifndef __BSP_ENET_H
#define __BSP_ENET_H

// 以太网复位引脚
#define ENET_RESET_GPIO_CLK               RCU_GPIOB
#define ENET_RESET_GPIO_PORT              GPIOB
#define ENET_RESET_PIN                    GPIO_PIN_10

// 以太网地址引脚
#define ENET_ADDR_GPIO_CLK               RCU_GPIOB
#define ENET_ADDR_GPIO_PORT              GPIOB
#define ENET_ADDR_PIN                    GPIO_PIN_0

/**************************************************/
void enet_system_setup(void);
void enet_hard_reset(void);

#endif /* __BSP_ENET_H */

