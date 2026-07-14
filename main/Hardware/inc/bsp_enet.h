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


/**************************************************/
uint8_t enet_system_setup(void);
void enet_hard_reset(void);
void enet_hard_reinit(void);

#endif /* __BSP_ENET_H */

