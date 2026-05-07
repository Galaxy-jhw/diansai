# STM32H563RGT6 TIM8 Dithering开环单极性SPWM最终代码包

## 1.代码包内容

```text
Core/Inc/pvinv_project_config.h
Core/Inc/pvinv_pwm_tim8.h
Core/Src/pvinv_pwm_tim8.c
Core/Inc/pvinv_openloop_spwm.h
Core/Src/pvinv_openloop_spwm.c
examples/main_user_code_snippets.c
README.md
```

## 2.是否生成CubeMX代码

没有生成CubeMX可以自动生成的代码。

本代码包不包含：

```text
MX_TIM8_Init()
MX_GPIO_Init()
SystemClock_Config()
HAL_TIM_MspPostInit()
TIM8_UP_IRQHandler()
```

这些仍然必须由CubeMX/.ioc生成。

本代码包只提供用户层模块：

```text
pvinv_pwm_tim8      -> TIM8底层发波封装
pvinv_openloop_spwm -> 开环D=M*sin(theta)控制封装
pvinv_project_config -> 用户可调宏配置
```

## 3.是否只用TIM8

是。

发波只使用TIM8：

```text
TIM8_CH1  -> PC6 -> A桥臂上管
TIM8_CH1N -> PA7 -> A桥臂下管
TIM8_CH2  -> PC7 -> B桥臂上管
TIM8_CH2N -> PB0 -> B桥臂下管
```

不使用TIM1、TIM2、TIM3、HRTIM。

## 4.Dithering支持方式

本代码默认允许TIM8 Dithering：

```c
#define PVINV_CFG_ALLOW_TIM8_DITHERING (1u)
```

Dithering处理分成两个概念：

### 4.1频率计算用等效周期

普通模式：

```text
effective_period_ticks = ARR + 1
```

Dithering模式：

```text
effective_period_ticks = ARR / PVINV_CFG_TIM8_DITHERING_SCALE
```

默认：

```c
#define PVINV_CFG_TIM8_DITHERING_SCALE (16.0f)
```

例如：

```text
TIM8_CLK = 250MHz
PSC = 1
ARR = 48000
Dithering scale = 16

effective_period_ticks = 48000 / 16 = 3000

fPWM = 250MHz / [2 * (1+1) * 3000]
     ≈ 20.8kHz
```

所以Dithering开启时，ARR=48000不能按普通ARR=48000理解。

### 4.2CCR写入用ARR寄存器单位

Dithering模式下，ARR和CCR都使用扩展后的寄存器单位。

因此CCR写入不除以16：

```c
ccr_period = TIM8->ARR;
center = 0.5f * ccr_period;

CCR1 = center + center * D;
CCR2 = center - center * D;
```

核心原则：

```text
频率计算：使用等效周期
CCR写入：使用寄存器周期
```

不能混用。

## 5.CubeMX配置要求

### TIM8基本配置

```text
Clock Source = Internal Clock
Channel1 = PWM Generation CH1 CH1N
Channel2 = PWM Generation CH2 CH2N
Channel3 = Disable
Channel4 = Disable
Counter Mode = Center Aligned Mode 1
Dead Time = 非零
TIM8 Update Interrupt = Enable
```

### 普通模式建议

```text
Prescaler = 1
Period = 3124
Repetition Counter = 1
Dithering = Disable
```

若TIM8_CLK约250MHz：

```text
fPWM ≈ 250MHz / [2 * 2 * 3125] = 20kHz
```

### Dithering模式建议

```text
Prescaler = 1
Dithering = Enable
Period/ARR按CubeMX/HAL规则生成
Repetition Counter = 1
Dead Time = 非零
```

如果Dithering后ARR寄存器约48000，并且scale=16：

```text
effective_period_ticks = 48000 / 16 = 3000
fPWM ≈ 20.8kHz
```

## 6.main.c使用方法

把`examples/main_user_code_snippets.c`中的内容复制到`main.c`对应USER CODE区域。

关键调用：

```c
PVINV_OpenLoopSPWM_Init();
PVINV_OpenLoopSPWM_SetSineFrequencyHz(50.0f);
PVINV_OpenLoopSPWM_SetTargetModulation(0.03f);
PVINV_OpenLoopSPWM_Start();
```

在HAL定时器回调中：

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(htim);
}
```

如果已有该函数，不要重复定义，只合并调用语句。

## 7.Keil使用方法

把文件放到工程中：

```text
Core/Inc/*.h
Core/Src/*.c
```

然后在Keil中手动添加：

```text
Core/Src/pvinv_pwm_tim8.c
Core/Src/pvinv_openloop_spwm.c
```

否则会出现Undefined symbol。

## 8.示波器验证

第一次测试不要接驱动板、功率桥、高压母线和负载。

只测：

```text
PC6
PA7
PC7
PB0
```

必须确认：

```text
PC6和PA7互补
PC7和PB0互补
上下管有死区
PC6和PC7占空比反向变化
PWM载波约20kHz
正弦包络约50Hz
```

没有完成这些验证，绝不能接功率桥。

## 9.常见错误

### 死区为0

状态：

```text
PVINV_PWM_STATUS_ERR_ZERO_DEADTIME
```

解决：

```text
CubeMX中TIM8 Dead Time设为非零。
```

### 载波频率过低

状态：

```text
PVINV_PWM_STATUS_ERR_CARRIER_TOO_LOW
```

如果启用了Dithering，检查：

```text
PVINV_CFG_TIM8_DITHERING_SCALE是否为16
ARR寄存器是否是Dithering扩展值
示波器实测载波是否约20kHz
```

### 没有互补输出

检查：

```text
Channel1是否为PWM Generation CH1 CH1N
Channel2是否为PWM Generation CH2 CH2N
PA7是否为TIM8_CH1N
PB0是否为TIM8_CH2N
代码是否调用HAL_TIMEx_PWMN_Start
```

## 10.最终验收标准

```text
1.PC6有PWM
2.PA7有PWM
3.PC7有PWM
4.PB0有PWM
5.PC6/PA7互补
6.PC7/PB0互补
7.两组互补都有死区
8.PC6和PC7占空比反向变化
9.载波约20kHz
10.正弦包络约50Hz
```
