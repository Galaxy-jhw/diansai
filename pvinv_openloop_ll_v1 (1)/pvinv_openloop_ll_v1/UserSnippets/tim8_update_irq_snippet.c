/*
 * 放到 stm32h5xx_it.c 中。
 * 如果CubeMX已经生成了TIM8_UP_IRQHandler，不要重复定义函数，
 * 只把PVOL_OnTim8UpdateIrq()插入到USER CODE区域，并确保Update标志被清除。
 */

#include "pvinv_openloop_ll.h"

void TIM8_UP_IRQHandler(void)
{
    /* USER CODE BEGIN TIM8_UP_IRQn 0 */
    if ((TIM8->SR & TIM_SR_UIF) != 0u)
    {
        TIM8->SR &= ~TIM_SR_UIF;
        PVOL_OnTim8UpdateIrq();
    }
    /* USER CODE END TIM8_UP_IRQn 0 */
}
