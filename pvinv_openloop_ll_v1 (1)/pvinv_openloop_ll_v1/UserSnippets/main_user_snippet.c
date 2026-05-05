/* 放到 main.c 的 USER CODE 区域中，不要覆盖整个main.c。 */

/* USER CODE BEGIN Includes */
#include "pvinv_openloop_ll.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */

/*
 * CubeMX应已完成：MX_GPIO_Init(); MX_TIM8_Init(); 等初始化。
 * 本开环测试模块不需要ADC、不需要DMA、不需要MPPT。
 */
PVOL_Init();

/* 上板初期建议从很小调制度开始，例如0.02~0.03。 */
PVOL_SetFrequencyHz(50.0f);
PVOL_SetTargetModulation(0.03f);

/* 允许TIM8 Update中断。也可以在CubeMX/NVIC里开启。 */
PVOL_PWM_TIM->DIER |= TIM_DIER_UIE;
NVIC_EnableIRQ(TIM8_UP_IRQn);

/* 启动TIM8计数器。 */
PVOL_PWM_TIM->CR1 |= TIM_CR1_CEN;

/* 开始输出互补PWM。 */
PVOL_Start();

/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    const PVOL_Handle_t *p = PVOL_GetHandle();

    /*
     * 这里可以低速显示/打印：
     * p->state
     * p->ref_freq_hz
     * p->target_modulation
     * p->modulation_cmd
     * p->modulation_inst
     * p->pwm_center_ccr
     * p->pwm_center_fallback
     * p->ccr1_last
     * p->ccr2_last
     * p->isr_count
     *
     * 不要在while里反复操作GPDMA通道。
     */
    (void)p;
    /* USER CODE END 3 */
}
