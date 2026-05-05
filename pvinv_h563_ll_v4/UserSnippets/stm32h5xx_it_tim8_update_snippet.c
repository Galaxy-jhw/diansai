/* 仅当你选择 TIM8 Update 作为控制节拍时使用。
 * 1. pvinv_h563_ll.h 中设置：
 *      #define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_TIM8_UPDATE_HOOK
 * 2. CubeMX 中开启 TIM8 Update interrupt。
 * 3. 不要再在 ADC DMA 完成中断里调用 PVINV_LL_ControlISR()。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
/* USER CODE END Includes */

void TIM8_UP_IRQHandler(void)
{
    /* USER CODE BEGIN TIM8_UP_IRQn 0 */
    PVINV_LL_OnTim8UpdateIrq();
    /* USER CODE END TIM8_UP_IRQn 0 */
}
