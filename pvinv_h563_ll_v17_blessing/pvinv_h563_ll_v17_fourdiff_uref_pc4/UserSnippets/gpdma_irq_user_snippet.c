/*
 * Insert these calls into the CubeMX-generated GPDMA IRQ handlers for ADC1 and ADC2.
 * Do not use GPDMA1 Channel0/1 for ADC if they are already used by LPUART RX/TX.
 * Example only: actual channel numbers depend on CubeMX allocation.
 *
 * IMPORTANT:
 *   - Call PVINV_LL_OnAdc1DmaCompleteIrq() only after ADC1 DMA transfer-complete flag is confirmed and cleared.
 *   - Call PVINV_LL_OnAdc2DmaCompleteIrq() only after ADC2 DMA transfer-complete flag is confirmed and cleared.
 *   - If the DMA channel reports TE/DTE/ULE/SUSE or equivalent error, call the corresponding ErrorIrq hook.
 */

/* In ADC1 DMA transfer-complete path, after clearing TC flag: */
PVINV_LL_OnAdc1DmaCompleteIrq();

/* In ADC1 DMA error path, after clearing error flags: */
PVINV_LL_OnAdc1DmaErrorIrq();

/* In ADC2 DMA transfer-complete path, after clearing TC flag: */
PVINV_LL_OnAdc2DmaCompleteIrq();

/* In ADC2 DMA error path, after clearing error flags: */
PVINV_LL_OnAdc2DmaErrorIrq();

/*
 * If using HAL-style callbacks instead of pure LL IRQ flag handling, the equivalent idea is:
 *   ADC1 conversion-complete/DMA-complete callback -> PVINV_LL_OnAdc1DmaCompleteIrq();
 *   ADC1 DMA error callback                       -> PVINV_LL_OnAdc1DmaErrorIrq();
 *   ADC2 conversion-complete/DMA-complete callback -> PVINV_LL_OnAdc2DmaCompleteIrq();
 *   ADC2 DMA error callback                       -> PVINV_LL_OnAdc2DmaErrorIrq();
 */
