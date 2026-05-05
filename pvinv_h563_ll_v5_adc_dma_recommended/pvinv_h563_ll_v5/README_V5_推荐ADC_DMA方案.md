# pvinv_h563_ll_v5 使用说明

本版本固定采用“最推荐方案”：ADC1五路Regular采样 + DMA Circular + ADC DMA完成回调触发控制。

## 文件

- Core/Inc/pvinv_h563_ll.h
- Core/Src/pvinv_h563_ll.c
- UserSnippets/main_user_adc_dma_recommended.c
- UserSnippets/hal_adc_callback_recommended.c
- UserSnippets/cubemx_adc1_rank_order.txt

## 核心链路

TIM8产生PWM并触发ADC → ADC1按Rank顺序采5路 → DMA搬到g_pvinv_adc_raw[0..4] → HAL_ADC_ConvCpltCallback → PVINV_LL_OnAdcDmaCompleteIrq → PVINV_LL_ControlISR → MPPT/PR/SPWM → 更新TIM8->CCR1/CCR2。

## TIM8 PWM引脚

- PC6 / TIM8_CH1  -> A桥臂上管
- PA7 / TIM8_CH1N -> A桥臂下管
- PC7 / TIM8_CH2  -> B桥臂上管
- PB0 / TIM8_CH2N -> B桥臂下管

代码会主动关闭TIM8_CH3/CH3N、TIM8_CH4/CH4N。

## ADC raw顺序

必须保证：

- g_pvinv_adc_raw[0] = Ud
- g_pvinv_adc_raw[1] = Id
- g_pvinv_adc_raw[2] = uREF
- g_pvinv_adc_raw[3] = iF
- g_pvinv_adc_raw[4] = uo

推荐CubeMX Rank：

1. PA2 / ADC1_IN14 -> Ud
2. PA3 / ADC1_IN15 -> Id
3. PA4 / ADC1_IN18 -> uREF
4. PA5 / ADC1_IN19 -> iF
5. PA6 / ADC1_IN3  -> uo

## 调试默认设置

V5默认启用电导增量法MPPT：

- PVINV_TEST_FIXED_IAMP_ENABLE = 0
- PVINV_IAMP_MAX = 0.50A
- PVINV_SOFTSTART_IAMP = 0.12A

上板初期如果要先调电流环，建议临时改成：

- PVINV_TEST_FIXED_IAMP_ENABLE = 1
- PVINV_TEST_FIXED_IAMP = 0.20A

确认电流反馈方向、PR参数和保护逻辑后，再改回0启用MPPT，并逐步提高PVINV_IAMP_MAX。

## 必须实测后修改

pvinv_h563_ll.h 中这些参数必须实测后修改：

- PVINV_UD_SCALE / PVINV_UD_OFFSET
- PVINV_ID_SCALE / PVINV_ID_OFFSET / PVINV_ID_SIGN
- PVINV_UREF_SCALE / PVINV_UREF_OFFSET
- PVINV_IFB_SCALE / PVINV_IFB_OFFSET / PVINV_IFB_SIGN
- PVINV_UO_SCALE / PVINV_UO_OFFSET

尤其是PVINV_IFB_SIGN，方向错会导致电流环正反馈。

## 绝对不要做的事

- 不要同时在TIM8 Update中断和ADC回调中调用控制函数。
- 不要在while(1)中调用PVINV_LL_ControlISR。
- 不要在ADC Rank顺序未确认前接高压功率桥。
- 不要在未验证死区和Break保护前提高电流上限。
