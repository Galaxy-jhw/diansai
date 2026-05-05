/* 放到 Core/Src/main.c 的 USER CODE 区域。
 * 本片段不包含 CubeMX 可生成的 SystemClock/MX_GPIO/MX_TIM/MX_ADC 初始化。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
/* CubeMX 生成的 MX_GPIO_Init/MX_ADCx_Init/MX_TIM8_Init 等执行完成后调用。 */
PVINV_LL_Init();
PVINV_LL_Start();

/* 如果你选择 TIM8 Update 作为控制节拍，需要你在 CubeMX 中打开 TIM8 update interrupt，
 * 并在 pvinv_h563_ll.h 中设置：
 * #define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_TIM8_UPDATE_HOOK
 * 同时确保 TIM8 Update Hook 分频后真实控制频率等于 PVINV_CTRL_FREQ_HZ。
 */
#if (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_TIM8_UPDATE_HOOK)
PVINV_PWM_TIM->DIER |= TIM_DIER_UIE;
#endif

/* 启动 TIM8 计数器。通道使能、MOE 由 PVINV_LL_Init/状态机控制。 */
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;
/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    const PVINV_Handle_t *p = PVINV_LL_GetHandle();

    /* 可用于 OLED/串口显示：
     * p->ud_f, p->id_f, p->pv_power
     * p->ref_freq, p->ref_amp, p->ref_locked
     * p->iamp_cmd, p->modulation
     * p->mppt_v_avg, p->mppt_i_avg, p->mppt_g
     * p->pwm_center_ccr, p->pwm_center_fallback
     * p->adc_samples_valid, p->state
     */

    (void)p;
}
/* USER CODE END WHILE */
