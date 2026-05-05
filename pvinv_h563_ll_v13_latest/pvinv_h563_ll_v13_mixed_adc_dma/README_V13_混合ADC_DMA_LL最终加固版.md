# V13混合ADC+DMA+LL库最终加固版

## 1. 本版定位

V13是用户控制模块，不包含CubeMX可生成的`SystemClock_Config`、`MX_GPIO_Init`、`MX_ADC1_Init`、`MX_ADC2_Init`、`MX_TIM8_Init`、`MX_GPDMA1_Init`。

本版目标：

- ADC1：`IN10 Differential`、`IN12 Differential`。
- ADC2：`IN1 Differential`、`IN18 Single-ended`。
- `uREF`由信号发生器输出，默认使用`ADC2_IN18 Single-ended`。
- 电流测量不按霍尔处理，而按微小采样电阻：`I = Vadc_diff / (Rshunt × TotalGain)`。
- DMA只上传raw，软件通过映射宏解释`Ud/Id/uREF/iF`。
- ADC1和ADC2都完成一轮DMA后，才执行一次控制ISR。
- TIM8发波参考testcore：`CCR1 = center + center*m`，`CCR2 = center - center*m`。
- 主动关闭TIM8 CH3/CH4，避免误驱动。

## 2. 与V12相比，本版最重要的优化

V13把“旧main.c中的GPDMA测试代码必须删除”作为集成重点处理：

- `UserSnippets/delete_old_main_gpdma_test_code.md`列出必须删除的代码。
- `UserSnippets/main_user_snippet.c`给出最终USER CODE写法。
- `IntegrationExamples/main_clean_final_example.c`给出干净main骨架。

旧工程`while(1)`中不能再反复操作GPDMA通道，否则会破坏ADC DMA连续搬运。

## 3. 默认ADC映射

```c
#define PVINV_SRC_UD    PVINV_ADC_SRC_ADC1_IN10_DIFF
#define PVINV_SRC_ID    PVINV_ADC_SRC_ADC1_IN12_DIFF
#define PVINV_SRC_UREF  PVINV_ADC_SRC_ADC2_IN18_SINGLE
#define PVINV_SRC_IFB   PVINV_ADC_SRC_ADC2_IN1_DIFF
#define PVINV_SRC_UO    PVINV_ADC_SRC_NONE
```

如果最终硬件接线改变，只改这些宏，不改控制算法主体。

## 4. CubeMX必须配置

### ADC1

- Regular Conversion数量：2
- Rank1：ADC1_IN10 Differential
- Rank2：ADC1_IN12 Differential
- DMA Circular，HalfWord到HalfWord，Memory Increment Enable

### ADC2

- Regular Conversion数量：2
- Rank1：ADC2_IN1 Differential
- Rank2：ADC2_IN18 Single-ended
- DMA Circular，HalfWord到HalfWord，Memory Increment Enable

### TIM8

- CH1/CH1N、CH2/CH2N互补PWM
- Dead Time
- Break建议启用
- TRGO或TRGO2触发ADC

### GPDMA

- ADC1和ADC2各用独立DMA通道
- 不要使用已给LPUART占用的GPDMA1 Channel0/1
- 推荐让CubeMX分配Channel2/3，最终以CubeMX生成结果为准

## 5. 你必须实测的量

- 差分ADC raw格式：确认`INP>INN`时code为正，`INP<INN`时code为负。
- `PVINV_ADC_DIFF_RES_BITS`与CubeMX ADC分辨率一致。
- 信号发生器uREF必须带偏置，ADC输入不能低于0V或高于3.3V。
- `Rshunt`、INA240增益、OPA2365后级等效差分增益。
- `PVINV_ID_SIGN`、`PVINV_IFB_SIGN`方向。
- 控制ISR频率是否等于`PVINV_CTRL_FREQ_HZ`，默认20kHz。
- TIM8互补PWM、死区、Break、MOE。

## 6. 仍需注意

V13不是完整CubeMX工程。它不会替你生成ADC/TIM/GPDMA底层初始化。你必须先在CubeMX里把ADC1、ADC2、TIM8、GPDMA配置好，再接入本模块。
