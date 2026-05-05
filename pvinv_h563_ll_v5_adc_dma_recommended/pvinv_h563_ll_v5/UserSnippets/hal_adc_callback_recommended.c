/*
 * 放入 main.c 的 USER CODE BEGIN 4 / END 4 区域。
 * 注意：不要同时在TIM8 Update中断里调用控制函数。
 */

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        /* ADC1一整轮5路Regular转换完成，DMA已经把数据写入g_pvinv_adc_raw[0..4]。 */
        PVINV_LL_OnAdcDmaCompleteIrq();
    }
}
/* USER CODE END 4 */
