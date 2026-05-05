/*
 * 这只是示例，不建议直接覆盖CubeMX生成的main.c。
 * 请把关键调用放入CubeMX的USER CODE区域。
 */
#include "main.h"
#include "gpio.h"
#include "tim.h"
#include "pvinv_openloop_ll.h"

void SystemClock_Config(void);

int main(void)
{
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM8_Init();

    PVOL_Init();
    PVOL_SetFrequencyHz(50.0f);
    PVOL_SetTargetModulation(0.03f);

    TIM8->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM8_UP_IRQn);
    TIM8->CR1 |= TIM_CR1_CEN;

    PVOL_Start();

    while (1)
    {
        const PVOL_Handle_t *p = PVOL_GetHandle();
        (void)p;
    }
}
