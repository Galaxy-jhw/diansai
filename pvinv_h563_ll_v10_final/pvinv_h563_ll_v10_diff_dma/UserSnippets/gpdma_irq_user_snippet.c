/*
 * stm32h5xx_it.c USER CODE 片段（V10）。
 * 不要直接照抄成完整中断函数；GPDMA通道号和清标志代码必须以CubeMX生成结果为准。
 *
 * 原则：
 *   ADC1 DMA完整搬运 g_pvinv_adc1_raw[0..1] 后调用 PVINV_LL_OnAdc1DmaCompleteIrq();
 *   ADC2 DMA完整搬运 g_pvinv_adc2_raw[0..1] 后调用 PVINV_LL_OnAdc2DmaCompleteIrq();
 *
 * 重要：必须先判断并清除DMA传输完成标志，再调用Hook。
 * 不要在半传输HT中断里调用Hook。
 */

#include "pvinv_h563_ll.h"

/* 示例：假设ADC1 DMA由CubeMX分配到GPDMA1 Channel2。把这一行放进对应IRQ的TC分支/USER CODE区。 */
/*
if (ADC1_DMA_TC_FLAG_IS_ACTIVE)
{
    CLEAR_ADC1_DMA_TC_FLAG();
    PVINV_LL_OnAdc1DmaCompleteIrq();
}
*/

/* 示例：假设ADC2 DMA由CubeMX分配到GPDMA1 Channel3。把这一行放进对应IRQ的TC分支/USER CODE区。 */
/*
if (ADC2_DMA_TC_FLAG_IS_ACTIVE)
{
    CLEAR_ADC2_DMA_TC_FLAG();
    PVINV_LL_OnAdc2DmaCompleteIrq();
}
*/

/*
 * 如果你后续用同一个硬件事件确认ADC1/ADC2都已经完成，也可以调用：
 *     PVINV_LL_OnBothAdcDmaCompleteIrq();
 * 但不能再分别调用ADC1/ADC2 Hook，否则控制ISR会重复执行。
 */
