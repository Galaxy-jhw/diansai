# STM32H563RGT6 LL 光伏逆变控制 V4 使用说明

## 1. 这版代码满足的内容

本代码包只提供用户控制代码，不提供 CubeMX 可以生成的初始化代码。

已严格按你上传的 `STM32H563RGT_CORE.ioc` 中 TIM8 引脚分配：

| 功能 | .ioc 引脚 | 定时器通道 |
|---|---|---|
| A 桥臂上管 | PC6 | TIM8_CH1 |
| A 桥臂下管 | PA7 | TIM8_CH1N |
| B 桥臂上管 | PC7 | TIM8_CH2 |
| B 桥臂下管 | PB0 | TIM8_CH2N |

`.ioc` 中 TIM8_CH3/CH3N、TIM8_CH4/CH4N 也配置为 PWM，但本代码主动关闭它们，避免误驱动额外桥臂。

发波公式严格参考 STM32learn/testcore：

```c
TIM8->CCR1 = center + center * m;
TIM8->CCR2 = center - center * m;
```

默认 `center = 6784`，与 testcore 的 `6784 + 6784 * D` 一致。若运行时发现 `TIM8->ARR` 不支持该 center，代码会安全回退到 `ARR/2`，并把 `p->pwm_center_fallback` 置 1。

控制算法包括：

- 电导增量法 MPPT：`g = ΔI / ΔU + I / U`
- uREF 上升过零同步
- 准 PR 电流环
- 单极性 SPWM
- 软启动
- 欠压、过压、过流、uREF 丢失、MOE/Break 故障检测

## 2. 这版代码没有假装 .ioc 已经完成 5 路 ADC

你当前 `.ioc` 中显式 regular conversion 主要是：

- ADC1 CH14 rank1
- ADC2 CH1 differential rank1

但是完整控制算法需要 5 路 raw：

```c
g_pvinv_adc_raw[0] = Ud;
g_pvinv_adc_raw[1] = Id;
g_pvinv_adc_raw[2] = uREF;
g_pvinv_adc_raw[3] = iF;
g_pvinv_adc_raw[4] = uo;
```

推荐你在 CubeMX 中使用 `.ioc` 已锁定为 ADC 的 PA2~PA6 补齐：

| raw | 功能 | 推荐引脚 / ADC 通道 |
|---|---|---|
| raw[0] | Ud | PA2 / ADC1_IN14 |
| raw[1] | Id | PA3 / ADC1_IN15 |
| raw[2] | uREF | PA4 / ADC1_IN18 |
| raw[3] | iF | PA5 / ADC1_IN19 |
| raw[4] | uo | PA6 / ADC1_IN3 |

你可以：

1. 在 CubeMX 里把 ADC1 配成 5-rank + DMA circular，DMA 目标指向 `g_pvinv_adc_raw`；或
2. 用自己的 ADC 采样代码调用：

```c
PVINV_LL_SetRawSamples(ud_raw, id_raw, uref_raw, ifb_raw, uo_raw);
```

如果 `adc_samples_valid = 0`，控制 ISR 不会进入闭环输出。

## 3. 控制节拍只能选择一种

本模块默认：

```c
#define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_EXTERNAL_ONLY
```

也就是你自己在确定 20kHz 的中断里调用：

```c
PVINV_LL_ControlISR();
```

可选两种 Hook：

### 方案 A：TIM8 Update Hook

在 `pvinv_h563_ll.h` 中设置：

```c
#define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_TIM8_UPDATE_HOOK
```

然后在 TIM8 Update IRQ 中调用：

```c
PVINV_LL_OnTim8UpdateIrq();
```

必须实测：Hook 分频后 `PVINV_LL_ControlISR()` 的频率等于 `PVINV_CTRL_FREQ_HZ`。

### 方案 B：ADC DMA 完成 Hook

在 `pvinv_h563_ll.h` 中设置：

```c
#define PVINV_CONTROL_TICK_SOURCE PVINV_TICK_ADC_DMA_HOOK
```

然后在 ADC DMA transfer complete 中调用：

```c
PVINV_LL_OnAdcDmaCompleteIrq();
```

不要同时启用两个 Hook。否则控制频率会错误。

## 4. 加入工程的方法

复制：

```text
Core/Inc/pvinv_h563_ll.h
Core/Src/pvinv_h563_ll.c
```

到你的工程对应目录。

在 `main.c` USER CODE 区域：

```c
#include "pvinv_h563_ll.h"
```

在 CubeMX 生成的所有 MX 初始化后调用：

```c
PVINV_LL_Init();
PVINV_LL_Start();
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;
```

`PVINV_LL_Init()` 只操作控制变量、TIM8 CCER/MOE/CCR；不生成系统时钟、GPIO、ADC、TIM 初始化。

## 5. 必须实测后修改的参数

在 `pvinv_h563_ll.h` 修改：

```c
PVINV_UD_SCALE / PVINV_UD_OFFSET
PVINV_ID_SCALE / PVINV_ID_OFFSET / PVINV_ID_SIGN
PVINV_UREF_SCALE / PVINV_UREF_OFFSET
PVINV_IFB_SCALE / PVINV_IFB_OFFSET / PVINV_IFB_SIGN
PVINV_UO_SCALE / PVINV_UO_OFFSET
```

特别是：

```c
PVINV_IFB_SIGN
```

如果方向错，PR 电流环会正反馈，可能一开 PWM 就过流。

## 6. 上板调试顺序

1. 不接功率桥，先确认 TIM8 CH1/CH1N、CH2/CH2N 波形。
2. 确认 CH3/CH3N、CH4/CH4N 没有被本代码启用。
3. 测 TIM8 PWM 频率、死区、互补关系。
4. 补齐 ADC 5 路 raw，并确认 raw[0]~raw[4] 顺序。
5. 标定 Ud、Id、uREF、iF、uo。
6. 低压条件下设置 `PVINV_TEST_FIXED_IAMP_ENABLE = 1`，固定小电流调 PR。
7. 确认 i_ref 与 iF 同相；若反相，修改 `PVINV_IFB_SIGN`。
8. 确认控制 ISR 真实频率为 `PVINV_CTRL_FREQ_HZ`。
9. 打开 MPPT：`PVINV_TEST_FIXED_IAMP_ENABLE = 0`。
10. 调 MPPT 步长，使 Ud 收敛到实际 `Us/2` 附近。

## 7. V4 相比 V3 修正点

- 默认 center 改为 testcore 的 6784，并增加 ARR 不匹配安全回退。
- 增加 `pwm_center_fallback` 诊断变量。
- 增加 `adc_samples_valid`，避免 ADC 未就绪时误闭环。
- 控制节拍用宏严格三选一，Hook 只在对应宏下编译。
- Break 清除函数增加底层 TIM8 Break flag 清除逻辑。
- 不直接定义多个 IRQ，避免与 CubeMX 生成函数重复。
- 保留 TIM8_CH3/CH4 主动关闭。
