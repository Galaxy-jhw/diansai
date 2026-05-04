#include "main.h"
#include "adc.h"
#include "tim.h"
#include "gpio.h"
#include "pv_inv_control.h"

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM1_Init();

    /*
     * 初始化控制模块。
     * 里面会启动 TIM1 PWM 通道、关闭 MOE、启动 ADC DMA。
     */
    PVINV_Init();

    /*
     * 上板初期建议改成按键启动。
     */
    PVINV_Start();

    while (1)
    {
        const PVINV_Handle_t *p = PVINV_GetHandle();

        /*
         * 可用于 OLED / 串口显示：
         *
         * p->ud_f
         * p->id_f
         * p->pv_power
         * p->ref_freq
         * p->ref_amp
         * p->iamp_cmd
         * p->modulation
         * p->mppt_v_avg
         * p->mppt_i_avg
         * p->mppt_g
         * p->state
         */
    }
}
