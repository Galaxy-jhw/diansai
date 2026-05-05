/*
 * V13 GPDMA中断USER CODE示例。
 * 你必须根据CubeMX实际分配给ADC1/ADC2的GPDMA通道修改。
 * 重点：不要使用已经分配给LPUART的GPDMA1 Channel0/Channel1。
 * 推荐：ADC1用Channel2，ADC2用Channel3，最终以CubeMX生成结果为准。
 */

#include "pvinv_h563_ll.h"

/*
 * 推荐写法：不要重复定义CubeMX已经生成的IRQ函数。
 * 应把下面两行Hook放入CubeMX生成的对应GPDMA IRQ函数USER CODE区域，
 * 并且只在确认Transfer Complete后调用；Transfer Error时调用Error Hook。
 */

/* ADC1 DMA Transfer Complete后调用： */
/* PVINV_LL_OnAdc1DmaCompleteIrq(); */

/* ADC2 DMA Transfer Complete后调用： */
/* PVINV_LL_OnAdc2DmaCompleteIrq(); */

/* ADC1 DMA Transfer Error后调用： */
/* PVINV_LL_OnAdc1DmaErrorIrq(); */

/* ADC2 DMA Transfer Error后调用： */
/* PVINV_LL_OnAdc2DmaErrorIrq(); */

/* 示例，假设CubeMX生成了GPDMA1_Channel2_IRQHandler作为ADC1 DMA：
void GPDMA1_Channel2_IRQHandler(void)
{
    // 1. 按CubeMX/LL库生成逻辑判断并清除TC/TE标志。
    // 2. 如果是ADC1 DMA传输完成：
    PVINV_LL_OnAdc1DmaCompleteIrq();
    // 3. 如果是ADC1 DMA传输错误：
    // PVINV_LL_OnAdc1DmaErrorIrq();
}
*/
