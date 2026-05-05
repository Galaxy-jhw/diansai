# .ioc 核对与必须补齐项

## 已严格使用的 .ioc 引脚

PWM 发波使用 TIM8：

| .ioc 引脚 | 功能 | 本代码用途 |
|---|---|---|
| PC6 | TIM8_CH1 | A 桥臂上管 |
| PA7 | TIM8_CH1N | A 桥臂下管 |
| PC7 | TIM8_CH2 | B 桥臂上管 |
| PB0 | TIM8_CH2N | B 桥臂下管 |

本代码不会使用：

- PC8 / TIM8_CH3
- PB1 / TIM8_CH3N
- PC9 / TIM8_CH4
- PB2 / TIM8_CH4N

并会主动关闭这些未使用通道的输出使能位。

## 必须补齐的 ADC

当前 .ioc 没有完整五路 regular conversion。本算法需要：

| raw | 建议通道 | 用途 |
|---|---|---|
| raw[0] | PA2 / ADC1_IN14 | Ud 输入电压 |
| raw[1] | PA3 / ADC1_IN15 | Id 输入电流 |
| raw[2] | PA4 / ADC1_IN18 | uREF 参考正弦 |
| raw[3] | PA5 / ADC1_IN19 | iF 输出电流反馈 |
| raw[4] | PA6 / ADC1_IN3 | uo 输出电压 |

CubeMX 中建议设置：

- ADC1 Scan Conversion Enable
- Number of Conversion = 5
- DMA Circular
- DMA memory increment enable
- Half-word width
- 外部触发可选 TIM8 TRGO/TRGO2，但要保证控制节拍唯一

如果你不补齐 ADC DMA，就必须用自己的 ADC 代码调用：

```c
PVINV_LL_SetRawSamples(ud, id, uref, ifb, uo);
```

## 必须实测

- TIM8 PWM 频率
- TIM8 CH1/CH1N、CH2/CH2N 互补关系
- 死区时间
- 控制 ISR 频率
- ADC raw 顺序
- 电流方向 `PVINV_ID_SIGN`、`PVINV_IFB_SIGN`
- Break/MOE 保护动作
