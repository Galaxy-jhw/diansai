/*
 * V13 clean main.c skeleton.
 * 这个文件不是要求你直接覆盖CubeMX生成的main.c，
 * 而是给你对照：旧while(1)里的GPDMA测试代码必须删除。
 */

#include "main.h"
#include "gpdma.h"
#include "icache.h"
#include "usart.h"
#include "gpio.h"
#include "pvinv_h563_ll.h"

/* 如果CubeMX生成了这些头文件，请按实际工程取消注释。 */
/* #include "adc.h" */
/* #include "tim.h" */

void SystemClock_Config(void);

int main(void)
{
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),15, 0));

    SystemClock_Config();

    MX_GPIO_Init();
    MX_GPDMA1_Init();

    /* 你必须在CubeMX里启用并生成ADC1/ADC2/TIM8初始化，然后在这里调用： */
    /* MX_ADC1_Init(); */
    /* MX_ADC2_Init(); */
    /* MX_TIM8_Init(); */

    MX_LPUART1_UART_Init();
    MX_ICACHE_Init();

    PVINV_LL_Init();

    /* 在这里启动ADC1/ADC2 DMA。
     * 由于你使用LL/GPDMA，启动函数和通道名以CubeMX生成结果为准。
     * DMA目标地址必须分别是：
     *   g_pvinv_adc1_raw，长度PVINV_ADC1_CH_NUM
     *   g_pvinv_adc2_raw，长度PVINV_ADC2_CH_NUM
     */

    PVINV_LL_ResetAdcPairSync();
    PVINV_LL_Start();

    /* 启动TIM8，TIM8通过TRGO触发ADC采样。 */
    PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;

    while (1)
    {
        const PVINV_Handle_t *p = PVINV_LL_GetHandle();
        (void)p;

        /* 只放低速任务：OLED显示、串口打印、按键处理、状态查看。
         * 禁止在这里反复操作GPDMA通道。
         */
    }
}
