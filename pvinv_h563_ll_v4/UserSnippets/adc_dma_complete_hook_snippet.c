/* 仅当你已经把 ADC1 配成 5-rank + DMA circular，并选择 ADC DMA 完成作为控制节拍时使用。
 * 1. pvinv_h563_ll.h 中设置：
 *      #define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_ADC_DMA_HOOK
 * 2. 确认 DMA 搬运目标就是 g_pvinv_adc_raw[5]。
 * 3. 不要再在 TIM8 Update 中断里调用 PVINV_LL_ControlISR()。
 *
 * 注意：这里没有写具体 DMAx_Channelx_IRQHandler 名称，因为 H563 的 DMA 实例/通道
 * 要以你的 CubeMX 生成结果为准。你只需要在 DMA transfer complete 分支调用：
 *      PVINV_LL_OnAdcDmaCompleteIrq();
 */

#include "pvinv_h563_ll.h"

/* 伪代码：放入 CubeMX 生成的 DMA 完成中断 USER CODE 区域。
if (DMA_TC_FLAG_IS_SET)
{
    CLEAR_DMA_TC_FLAG;
    PVINV_LL_OnAdcDmaCompleteIrq();
}
*/
