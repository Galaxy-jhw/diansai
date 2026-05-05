/*
 * main.c USER CODE片段（V10）。
 * 不要整体替换main.c，只把对应内容复制到CubeMX保留区。
 * 本模块不包含CubeMX可生成的SystemClock/GPIO/ADC/TIM8/GPDMA初始化。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */

/* CubeMX必须已经生成并调用：
 *   MX_GPIO_Init();
 *   MX_GPDMA1_Init();
 *   MX_ADC1_Init();
 *   MX_ADC2_Init();
 *   MX_TIM8_Init();
 *
 * 你旧main.c里while(1)中的GPDMA测试代码必须删除；主循环只能做低速显示/按键/串口。
 */
PVINV_LL_Init();

/* ADC/GPDMA启动由CubeMX实际生成代码决定。必须保证：
 *   ADC1差分Regular序列：IN10、IN12，DMA目的地址=g_pvinv_adc1_raw，长度=2。
 *   ADC2差分Regular序列：IN1、IN18， DMA目的地址=g_pvinv_adc2_raw，长度=2。
 *   ADC1和ADC2由同一个TIM8 TRGO/TRGO2同步触发，控制频率=PVINV_CTRL_FREQ_HZ。
 *
 * 如果CubeMX没有自动启动ADC/DMA，你需要在USER CODE中用LL启动ADC、DMA和触发源。
 * 不要占用LPUART已使用的GPDMA Channel0/Channel1。
 */

PVINV_LL_Start();

/* 启动TIM8计数器。CH1/CH1N、CH2/CH2N、死区、Break由CubeMX初始化。 */
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;

/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    const PVINV_Handle_t *p = PVINV_LL_GetHandle();
    (void)p;
    /* 可显示：p->state、p->isr_count、p->code_id、p->vdiff_id、p->ifb_f等。 */
}
/* USER CODE END WHILE */
