# STM32H563RGT6最终版闭环代码包V3：TIM8单极性SPWM+Dithering+MPPT电导增量法+PR电流环

## 1.本代码包定位

这是闭环控制用户层代码包，不是CubeMX工程生成代码包。

本代码包实现：

```text
1.TIM8四路互补PWM；
2.TIM8 Dithering兼容；
3.单极性SPWM底层发波；
4.ADC1/ADC2 DMA用户层采样缓冲；
5.Ud、Id、uREF、iF、uo测量换算；
6.MPPT电导增量法；
7.PV电压外环PI；
8.PR电流闭环；
9.ADC预热；
10.软启动；
11.软件保护状态机；
12.ISR快速关断+主循环HAL Stop清理。
```

## 2.不生成CubeMX代码

本代码包不生成：

```text
MX_TIM8_Init()
MX_ADC1_Init()
MX_ADC2_Init()
MX_GPDMA1_Init()
MX_GPIO_Init()
SystemClock_Config()
HAL_TIM_MspPostInit()
TIM8_UP_IRQHandler()
ADC_IRQHandler()
DMA_IRQHandler()
```

这些必须由CubeMX根据.ioc生成。

## 3.V3相对V2的修正

```text
1.加入ADC_WARMUP状态，避免滤波初值为0导致启动欠压误故障；
2.PVINV_Control_Start只允许READY状态启动，不能从IDLE直接启动；
3.故障发生在ISR中时，只执行快速关断，不在ISR中完整HAL Stop；
4.加入PVINV_Control_Service()，用于主循环中执行故障后的HAL Stop清理；
5.ADC滤波第一次更新直接使用瞬时测量值作为初值。
```

## 4.文件结构

```text
Core/Inc/pvinv_project_config.h
Core/Inc/pvinv_utils.h
Core/Inc/pvinv_pwm_tim8.h
Core/Src/pvinv_pwm_tim8.c
Core/Inc/pvinv_adc.h
Core/Src/pvinv_adc.c
Core/Inc/pvinv_pi.h
Core/Src/pvinv_pi.c
Core/Inc/pvinv_pr.h
Core/Src/pvinv_pr.c
Core/Inc/pvinv_mppt_inccond.h
Core/Src/pvinv_mppt_inccond.c
Core/Inc/pvinv_control.h
Core/Src/pvinv_control.c
examples/main_user_code_snippets.c
```

## 5.TIM8配置要求

```text
TIM8_CH1  -> PC6
TIM8_CH1N -> PA7
TIM8_CH2  -> PC7
TIM8_CH2N -> PB0

Channel1 = PWM Generation CH1 CH1N
Channel2 = PWM Generation CH2 CH2N
Counter Mode = Center Aligned
DeadTime = 非零
TIM8 Update interrupt = Enable
Dithering = 可开启
```

## 6.单极性SPWM核心

闭环控制最终输出归一化调制量D：

```text
D = (v_ff + PR输出) / Ud
```

TIM8底层执行：

```c
CCR1 = center + center * D;
CCR2 = center - center * D;
```

即：

```text
PC6占空比增大时，PC7占空比减小；
PC6占空比减小时，PC7占空比增大。
```

## 7.Dithering处理

默认：

```c
#define PVINV_CFG_ALLOW_TIM8_DITHERING (1u)
#define PVINV_CFG_TIM8_DITHERING_SCALE (16.0f)
```

频率计算：

```text
普通模式：effective_period_ticks = ARR + 1
Dithering模式：effective_period_ticks = ARR / 16
```

CCR写入：

```text
始终使用ARR寄存器单位，不把CCR除以16。
```

## 8.ADC Rank要求

ADC1：

```text
Rank1 -> Ud
Rank2 -> Id
Rank3 -> uREF
DMA length = 3
```

ADC2：

```text
Rank1 -> iF
Rank2 -> uo
DMA length = 2
```

如果CubeMX里Rank顺序不同，必须修改`pvinv_project_config.h`里的索引宏。

## 9.必须标定的参数

`pvinv_project_config.h`中的以下宏必须按实际硬件修改：

```text
PVINV_UD_SCALE
PVINV_ID_SCALE
PVINV_IF_SCALE
PVINV_UO_SCALE
PVINV_UD_SIGN
PVINV_ID_SIGN
PVINV_IF_SIGN
PVINV_UO_SIGN
PVINV_UREF_BIAS_V
PVINV_UREF_AMPLITUDE_V
```

默认值只是占位值，不能直接用于真实功率级闭环。

## 10.控制流程

```text
ADC_WARMUP
-> ADC DMA采样
-> Ud/Id/uREF/iF/uo换算
-> MPPT电导增量法更新Vmppt_ref
-> PV电压外环PI生成i_amp_cmd
-> i_ref=i_amp_cmd*ref_unit
-> PR电流环计算pr_out
-> D=(v_ff+pr_out)/Ud
-> TIM8单极性SPWM更新CCR1/CCR2
```

## 11.main.c使用方法

参考：

```text
examples/main_user_code_snippets.c
```

核心调用：

```c
PVINV_Control_Init();
PVINV_Control_Start();
```

定时器回调：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_Control_TIM_PeriodElapsedCallback(htim);
}
```

主循环建议加入：

```c
PVINV_Control_Service();
```

## 12.安全顺序

严禁直接接高压功率桥测试闭环。

推荐顺序：

```text
1.开环TIM8四路PWM验证；
2.Dithering scale确认；
3.20kHz载波确认；
4.50Hz包络确认；
5.ADC raw数组顺序确认；
6.Ud/Id/uREF/iF/uo方向和比例标定；
7.低压限流闭环；
8.逐步提高电压和负载。
```
