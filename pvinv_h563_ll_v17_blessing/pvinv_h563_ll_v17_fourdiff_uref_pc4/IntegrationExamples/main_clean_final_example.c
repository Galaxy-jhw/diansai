/*
 * This is a clean main.c structure example only.
 * Keep CubeMX generated code and place user code in USER CODE sections.
 */

#include "main.h"
#include "gpdma.h"
#include "icache.h"
#include "usart.h"
#include "gpio.h"
/* #include "adc.h" */
/* #include "tim.h" */
#include "pvinv_h563_ll.h"

int main(void)
{
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    SystemClock_Config();

    MX_GPIO_Init();
    MX_GPDMA1_Init();
    MX_LPUART1_UART_Init();
    MX_ICACHE_Init();
    /* MX_ADC1_Init(); */
    /* MX_ADC2_Init(); */
    /* MX_TIM8_Init(); */

    PVINV_LL_Init();

    /* Start ADC1 DMA -> g_pvinv_adc1_raw, length 3. */
    /* Start ADC2 DMA -> g_pvinv_adc2_raw, length 2. */

    PVINV_LL_Start();
    PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;

    while (1)
    {
        const PVINV_Handle_t *p = PVINV_LL_GetHandle();
        (void)p;
        /* Low-speed tasks only. Never reconfigure GPDMA repeatedly here. */
    }
}
