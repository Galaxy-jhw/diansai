# STM32H563RGT6 V10差分ADC+DMA+LL控制模块

## 1. 版本定位

本版本是用户控制模块，不包含CubeMX可以生成的初始化代码：`SystemClock`、`GPIO`、`ADC1/ADC2`、`TIM8`、`GPDMA`均由CubeMX根据你的`.ioc`生成。

控制模块实现：

- 电导增量法MPPT
- uREF同步
- PR电流环
- TIM8单极性SPWM
- 保护状态机
- 差分ADC raw映射
- 采样电阻电流换算
- ADC1/ADC2 DMA成对完成后触发一次控制

## 2. V10相对V9的关键加固

V9默认把差分ADC原始码当成16-bit二补码满量程处理。这个假设可能被严格审查指出问题，因为STM32H5的ADC分辨率和数据对齐方式必须以CubeMX配置为准。

V10新增：

```c
#define PVINV_ADC_DIFF_RES_BITS 14u
```

并使用位宽相关的符号扩展逻辑解释差分raw。也就是说，如果你的CubeMX ADC分辨率是14-bit，就保持14；如果你配置成12-bit或16-bit，就必须改成12或16。

V10仍然保留：

```c
#define PVINV_ADC_DIFF_FORMAT PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT
```

如果实测发现差分ADC结果是偏移二进制，则改成：

```c
#define PVINV_ADC_DIFF_FORMAT PVINV_ADC_DIFF_FORMAT_OFFSET_BINARY
```

并确认：

```c
#define PVINV_ADC_DIFF_ZERO_CODE (1u << (PVINV_ADC_DIFF_RES_BITS - 1u))
```

## 3. PWM资源

发波定时器：TIM8。只使用：

- PC6 / TIM8_CH1  -> A桥臂上管
- PA7 / TIM8_CH1N -> A桥臂下管
- PC7 / TIM8_CH2  -> B桥臂上管
- PB0 / TIM8_CH2N -> B桥臂下管

本模块主动关闭TIM8_CH3/CH3N、TIM8_CH4/CH4N。

发波公式参考testcore：

```c
TIM8->CCR1 = center + center * m;
TIM8->CCR2 = center - center * m;
```

其中`m`是PR电流环输出的调制量。

## 4. ADC差分输入

你最新指定：

- ADC1：IN10 Differential、IN12 Differential
- ADC2：IN1 Differential、IN18 Differential

DMA缓冲区：

```c
volatile uint16_t g_pvinv_adc1_raw[2];
volatile uint16_t g_pvinv_adc2_raw[2];
```

推荐Rank：

```text
ADC1 Rank1 = ADC1_IN10 Differential -> g_pvinv_adc1_raw[0]
ADC1 Rank2 = ADC1_IN12 Differential -> g_pvinv_adc1_raw[1]
ADC2 Rank1 = ADC2_IN1  Differential -> g_pvinv_adc2_raw[0]
ADC2 Rank2 = ADC2_IN18 Differential -> g_pvinv_adc2_raw[1]
```

如果Rank顺序改变，只改：

```c
PVINV_ADC1_IDX_IN10
PVINV_ADC1_IDX_IN12
PVINV_ADC2_IDX_IN1
PVINV_ADC2_IDX_IN18
```

## 5. 物理量映射

默认映射只是占位，按最终接线修改：

```c
#define PVINV_SRC_UD    PVINV_ADC_SRC_ADC1_IN10
#define PVINV_SRC_ID    PVINV_ADC_SRC_ADC1_IN12
#define PVINV_SRC_UREF  PVINV_ADC_SRC_ADC2_IN1
#define PVINV_SRC_IFB   PVINV_ADC_SRC_ADC2_IN18
#define PVINV_SRC_UO    PVINV_ADC_SRC_NONE
```

目前你只指定了4个差分通道，所以默认`UO=NONE`。如果必须采`uo`，需要额外增加ADC通道和映射。

## 6. 电流采样模型

`Id`和`iF`使用微小采样电阻模型：

```text
I = Vadc_diff / (Rshunt × TotalGain)
```

其中：

```text
TotalGain = INA240增益 × OPA2365后级等效差分增益
```

必须按实测或原理图计算修改：

```c
PVINV_ID_SHUNT_R_OHM
PVINV_ID_TOTAL_GAIN
PVINV_IFB_SHUNT_R_OHM
PVINV_IFB_TOTAL_GAIN
PVINV_ID_SIGN
PVINV_IFB_SIGN
```

## 7. 双ADC DMA成对完成机制

不要在ADC1 DMA完成时直接控制，也不要在ADC2 DMA完成时直接控制。

本模块逻辑是：

```text
ADC1 DMA完成 -> 锁存ADC1 raw快照
ADC2 DMA完成 -> 锁存ADC2 raw快照
两者都完成 -> 执行一次PVINV_LL_ControlISR()
```

如果某一路DMA反复完成，另一路一直不完成，模块会进入`PVINV_STATE_FAULT_ADC_INVALID`，避免用半新半旧数据闭环。

## 8. 旧工程必须处理的事项

你的旧工程中：

- `main.c`还没有ADC1、ADC2、TIM8初始化调用。
- `while(1)`里还有GPDMA测试代码，必须删除。
- `GPDMA1_Channel0/1_IRQHandler`为空。
- LPUART已经使用GPDMA1 Channel0/1，所以ADC1/ADC2建议使用新的GPDMA通道，例如Channel2/3，具体以CubeMX分配为准。

## 9. 上板前必须验证

1. `PVINV_ADC_DIFF_RES_BITS`是否与你CubeMX ADC分辨率一致。
2. `INP>INN`时`code_*`为正，`INP<INN`时为负，`INP=INN`时接近0。
3. ADC1/ADC2 DMA完成频率最终让控制ISR等于`PVINV_CTRL_FREQ_HZ`。
4. TIM8互补PWM、死区、Break、MOE是否正确。
5. `PVINV_ID_SIGN`、`PVINV_IFB_SIGN`方向是否正确。
6. 采样电阻和总增益参数是否已实测/计算。

## 10. 文件使用

复制：

```text
Core/Inc/pvinv_h563_ll.h
Core/Src/pvinv_h563_ll.c
```

到你的工程对应目录。

参考：

```text
UserSnippets/main_user_snippet.c
UserSnippets/gpdma_irq_user_snippet.c
UserSnippets/cubemx_adc_dma_mapping.txt
```

把片段放进CubeMX的USER CODE区域，不要整体替换CubeMX生成文件。
