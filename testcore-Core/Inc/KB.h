#ifndef _KB_H
#define _KB_H
#include "main.h"
//键盘引脚
//LD,CK为高速，输出模式，推挽，初始为低，无上拉下拉
//DA为输入模式
//统一在mx中配置好，不需要使用c文件中的键盘初始化函数
//中断配置，在mx中使能IRQ对应引脚的外部中断，选择上升沿触发
#define GPIO_IRQ_		  KB_IRQ_GPIO_Port
#define GPIO_IRQ_PIN  KB_IRQ_Pin
#define GPIO_DA			  KB_DA_GPIO_Port
#define GPIO_DA_PIN   KB_DA_Pin
#define GPIO_CK			  KB_CLK_GPIO_Port
#define GPIO_CK_PIN	  KB_CLK_Pin
#define GPIO_LD			  KB_LD_GPIO_Port
#define GPIO_LD_PIN	  KB_LD_Pin

uint16_t Keyboard_ReadData(void);
void KeyAction(uint8_t key);

#endif
