/*
 * 放入 main.c 的 USER CODE 区域。
 * 这是 V5 推荐方案：ADC1 5-rank + DMA circular，ADC完成回调触发控制。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
#include "adc.h"
#include "tim.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */

/* 1. 初始化用户控制模块：不会生成CubeMX初始化，只设置控制变量、CCR中心值、关闭MOE等。 */
PVINV_LL_Init();

/* 2. 启动ADC1 DMA。
 *    重要：DMA目标必须是g_pvinv_adc_raw，长度必须是PVINV_ADC_CH_NUM=5。
 *    重要：CubeMX中ADC1必须配置5个Regular Rank，且顺序必须为Ud/Id/uREF/iF/uo。
 */
if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_pvinv_adc_raw, PVINV_ADC_CH_NUM) != HAL_OK)
{
    Error_Handler();
}

/* 3. 启动状态机。实际不会立刻输出，必须等待ADC有效、uREF锁定、Ud正常。 */
PVINV_LL_Start();

/* 4. 启动TIM8计数。
 *    如果CubeMX已经在别处启动TIM8，可不要重复写。
 *    TIM8开始运行后，TRGO/TRGO2触发ADC采样，ADC DMA完成后进入回调。
 */
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;

/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    const PVINV_Handle_t *p = PVINV_LL_GetHandle();

    /* 这里可以OLED/串口显示：
     * p->state
     * p->adc_samples_valid
     * p->isr_count
     * p->pwm_center_ccr
     * p->pwm_center_fallback
     * p->ud_f / p->id_f / p->pv_power
     * p->ref_freq / p->ref_amp / p->ref_locked
     * p->iamp_cmd / p->i_ref / p->ifb_f / p->modulation
     * p->mppt_v_avg / p->mppt_i_avg / p->mppt_g
     */
    (void)p;
}
/* USER CODE END WHILE */
