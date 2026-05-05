# V17 四路差分测量 + uREF单端PC4 + DMA raw上传 + LL控制模块

## 1. 默认采样分配

本版按当前确认的需求组织采样资源：四路差分通道全部留给四个被测模拟量，uREF另用PC4单端ADC。

| 物理量 | 默认ADC源 | 模式 | DMA数组位置 |
|---|---|---|---|
| Ud | ADC1_IN10 | Differential | g_pvinv_adc1_raw[0] |
| Id | ADC1_IN12 | Differential | g_pvinv_adc1_raw[1] |
| uREF | ADC1_IN4 / PC4 | Single-ended | g_pvinv_adc1_raw[2] |
| iF | ADC2_IN1 | Differential | g_pvinv_adc2_raw[0] |
| uo | ADC2_IN18 | Differential | g_pvinv_adc2_raw[1] |

默认映射在 `pvinv_h563_ll.h` 中：

```c
#define PVINV_SRC_UD    PVINV_ADC_SRC_ADC1_IN10_DIFF
#define PVINV_SRC_ID    PVINV_ADC_SRC_ADC1_IN12_DIFF
#define PVINV_SRC_UREF  PVINV_ADC_SRC_ADC1_IN4_SINGLE
#define PVINV_SRC_IFB   PVINV_ADC_SRC_ADC2_IN1_DIFF
#define PVINV_SRC_UO    PVINV_ADC_SRC_ADC2_IN18_DIFF
```

V17会在编译期检查：uREF必须使用ADC1_IN4单端；Ud、Id、iF、uo必须使用差分源；uo不能为空；物理量映射不能重复。

## 2. 相比V16的重要加固

1. 新增 `last_fault`、`fault_count`、`adc_dma_error_count`、`adc_pair_reset_count`，便于调试判断故障来源。
2. 新增 `PVINV_LL_OnAdc1DmaErrorIrq()`、`PVINV_LL_OnAdc2DmaErrorIrq()`，GPDMA错误可直接进入 `ADC_INVALID` 故障，避免继续闭环。
3. 新增 `PVINV_LL_ResetAdcPairSync()`，可在重新启动ADC DMA或人工恢复前清除ADC1/ADC2配对状态。
4. ADC1/ADC2单边重复完成超限时立即返回并锁定ADC_INVALID，不再继续尝试配对控制。
5. 普通故障复位会重置uREF同步器和ADC配对状态，避免用旧锁相/旧DMA状态继续启动。
6. 采样电阻电流换算增加分母保护，若 `Rshunt × TotalGain` 配置异常，会进入ADC_INVALID。
7. 保留低通滤波首样本初始化，避免启动时滤波值从0缓慢爬升导致误欠压。
8. WAIT_REF状态下低Ud不会锁死欠压故障，而是等待Ud恢复到 `PVINV_UD_RECOVER_TH` 后再软启动。
9. PWM Break仍然使用专用 `PVINV_LL_ClearPwmBreakFaultAfterCheck()` 恢复，不会被普通复位误清。
10. 继续保持不写CubeMX可生成初始化代码。

## 3. CubeMX必须配置

- ADC1 Regular + DMA Circular，长度3：
  - Rank1 = ADC1_IN10 Differential
  - Rank2 = ADC1_IN12 Differential
  - Rank3 = ADC1_IN4 Single-ended，PC4，uREF
- ADC2 Regular + DMA Circular，长度2：
  - Rank1 = ADC2_IN1 Differential
  - Rank2 = ADC2_IN18 Differential
- TIM8：CH1/CH1N、CH2/CH2N、Dead Time、Break、TRGO触发ADC。
- GPDMA：ADC1和ADC2使用独立GPDMA通道，避免占用LPUART已经使用的Channel0/1。

## 4. 必须删除旧main.c里的GPDMA测试代码

不要在 `while(1)` 里反复调用：

```c
LL_DMA_DisableChannel(...);
LL_DMA_SetSrcAddress(...);
LL_DMA_SetBlkDataLength(...);
LL_DMA_EnableChannel(...);
```

最终 `while(1)` 只做显示、串口打印、按键、状态监测等低速任务。

## 5. 电流换算

Id和iF按微小采样电阻模型：

```text
I = Vadc_diff / (Rshunt × TotalGain)
```

必须按实际硬件修改：

- `PVINV_ID_SHUNT_R_OHM`
- `PVINV_ID_TOTAL_GAIN`
- `PVINV_IFB_SHUNT_R_OHM`
- `PVINV_IFB_TOTAL_GAIN`
- `PVINV_ID_SIGN`
- `PVINV_IFB_SIGN`

## 6. uREF输入安全

uREF来自信号发生器，但PC4/ADC1_IN4输入必须满足：

```text
0V <= PC4电压 <= 3.3V
```

推荐：1.65V直流偏置，正弦幅值不超过约1.5V。代码中使用：

```c
#define PVINV_UREF_ZERO_VOLT 1.650f
```

## 7. TIM8发波

发波参考testcore：

```c
TIM8->CCR1 = center + center * m;
TIM8->CCR2 = center - center * m;
```

只启用CH1/CH1N、CH2/CH2N；主动关闭CH3/CH4，避免误驱动。

## 8. 上板前必须实测

1. ADC差分raw格式：INP>INN时code为正，INP<INN时code为负，INP=INN时接近0。
2. ADC1/ADC2 DMA完成后，`adc_pair_count`持续增加。
3. `PVINV_LL_ControlISR()`实际执行频率等于`PVINV_CTRL_FREQ_HZ`，默认20kHz。
4. TIM8互补PWM、死区、Break、MOE正确。
5. Id/iF方向正确，否则改`PVINV_ID_SIGN`、`PVINV_IFB_SIGN`。
6. uREF进入PC4前不能有负电压。
7. 若出现 `FAULT_ADC_INVALID`，查看 `last_fault`、`adc_dma_error_count`、`adc_pair_miss_count`、`adc_pair_reset_count`。
