# STM32H563RGT6 TIM8+Dithering开环单极性SPWM最终使用说明

> 当前目标：只验证TIM8开环发波。  
> 核心要求：保留Dithering、封装发波代码、不生成CubeMX可以自动生成的代码、便于上传GitHub。  
> 适用芯片：STM32H563RGT6。  
> 发波外设：只使用TIM8。

---

## 1.本代码到底做什么

本代码只做一件事：

```text
使用TIM8输出四路互补PWM，并在TIM8更新中断中动态更新CCR1/CCR2，实现开环单极性SPWM。
```

它验证：

```text
1.PC6/TIM8_CH1是否有PWM；
2.PA7/TIM8_CH1N是否是PC6的互补输出；
3.PC7/TIM8_CH2是否有PWM；
4.PB0/TIM8_CH2N是否是PC7的互补输出；
5.PC6/PA7之间是否有死区；
6.PC7/PB0之间是否有死区；
7.PC6和PC7占空比是否反向变化；
8.PWM载波是否约20kHz；
9.正弦调制包络是否约50Hz。
```

它不做：

```text
1.MPPT；
2.电导增量法；
3.ADC DMA；
4.uREF同步；
5.PR电流闭环；
6.输出电压闭环；
7.完整保护逻辑；
8.最终功率级控制。
```

---

## 2.本代码只使用TIM8

发波只用TIM8，不用TIM1、TIM2、TIM3、HRTIM。

四路输出对应关系：

```text
TIM8_CH1  -> PC6 -> A桥臂上管
TIM8_CH1N -> PA7 -> A桥臂下管

TIM8_CH2  -> PC7 -> B桥臂上管
TIM8_CH2N -> PB0 -> B桥臂下管
```

TIM8承担两个任务：

```text
1.产生四路互补PWM；
2.通过TIM8更新中断刷新CCR1/CCR2。
```

---

## 3.最终代码文件结构

建议放置为：

```text
Core/
├── Inc/
│   ├── pvinv_project_config.h
│   ├── pvinv_pwm_tim8.h
│   └── pvinv_openloop_spwm.h
└── Src/
    ├── pvinv_pwm_tim8.c
    └── pvinv_openloop_spwm.c

examples/
└── main_user_code_snippets.c

README_TIM8_OpenLoop_Dithering_SPWM_Final.md
```

### 3.1`pvinv_project_config.h`

用户配置文件，管理：

```text
1.是否允许Dithering；
2.Dithering缩放系数；
3.是否允许DeadTime=0；
4.PWM载波频率范围；
5.TIM8更新中断频率范围；
6.默认正弦频率；
7.默认调制比；
8.软启动速度；
9.D限幅。
```

关键宏：

```c
#define PVINV_CFG_ALLOW_TIM8_DITHERING           (1u)
#define PVINV_CFG_TIM8_DITHERING_SCALE           (16.0f)
#define PVINV_CFG_ALLOW_ZERO_DEADTIME_LOGIC_ONLY (0u)

#define PVINV_CFG_CARRIER_MIN_HZ                 (18000.0f)
#define PVINV_CFG_CARRIER_MAX_HZ                 (25000.0f)

#define PVINV_CFG_UPDATE_MIN_HZ                  (15000.0f)
#define PVINV_CFG_UPDATE_MAX_HZ                  (50000.0f)

#define PVINV_CFG_DEFAULT_SINE_HZ                (50.0f)
#define PVINV_CFG_DEFAULT_MODULATION             (0.03f)

#define PVINV_CFG_MOD_RAMP_PER_SEC               (0.20f)
#define PVINV_CFG_D_LIMIT                        (0.98f)
```

### 3.2`pvinv_pwm_tim8.h/.c`

TIM8底层发波封装，负责：

```text
1.检查TIM8是否中心对齐；
2.检查DeadTime是否非零；
3.识别Dithering是否开启；
4.计算Dithering下的等效周期；
5.估算PWM载波频率；
6.估算TIM8更新中断频率；
7.启动CH1/CH1N、CH2/CH2N；
8.提供PVINV_PWM_TIM8_SetUnipolarD(D)。
```

核心底层接口：

```c
PVINV_PWM_TIM8_SetUnipolarD(float d);
```

内部执行：

```c
TIM8->CCR1 = center + center * d;
TIM8->CCR2 = center - center * d;
```

### 3.3`pvinv_openloop_spwm.h/.c`

开环控制封装，负责：

```text
1.生成theta；
2.计算D=M*sin(theta)；
3.调制比软启动；
4.在TIM8更新中断中调用底层发波接口。
```

核心接口：

```c
PVINV_OpenLoopSPWM_Init();
PVINV_OpenLoopSPWM_SetSineFrequencyHz(50.0f);
PVINV_OpenLoopSPWM_SetTargetModulation(0.03f);
PVINV_OpenLoopSPWM_Start();
```

---

## 4.没有生成哪些CubeMX代码

本代码包不生成、不替代：

```text
1.MX_TIM8_Init()
2.MX_GPIO_Init()
3.SystemClock_Config()
4.HAL_TIM_MspPostInit()
5.TIM8_UP_IRQHandler()
6.ADC初始化
7.DMA初始化
8.GPIO初始化
9.时钟初始化
```

这些仍然必须由CubeMX根据`.ioc`生成。

---

## 5.单极性SPWM核心原理

开环阶段：

```c
D = M * sin(theta);
```

底层单极性SPWM：

```c
TIM8->CCR1 = center + center * D;
TIM8->CCR2 = center - center * D;
```

含义：

```text
A桥臂占空比 = 0.5 + 0.5D
B桥臂占空比 = 0.5 - 0.5D
```

现象：

```text
D>0：PC6占空比增大，PC7占空比减小；
D<0：PC6占空比减小，PC7占空比增大；
D=0：PC6和PC7都约50%。
```

这就是单极性SPWM的核心。

---

## 6.Dithering支持逻辑

### 6.1为什么保留Dithering

Dithering用于提高PWM发波精度，尤其在小调制比时，可以提高有效分辨率。所以代码默认允许：

```c
#define PVINV_CFG_ALLOW_TIM8_DITHERING (1u)
```

### 6.2Dithering后ARR不能按普通ARR理解

如果看到：

```text
普通等效周期约3000；
Dithering后TIM8->ARR约48000。
```

则：

```text
48000 / 3000 = 16
```

说明Dithering缩放系数大概率为16。

### 6.3频率计算和CCR写入必须分开

最重要原则：

```text
频率计算：使用等效周期；
CCR写入：使用ARR寄存器单位。
```

普通模式：

```text
effective_period_ticks = ARR + 1
```

Dithering模式：

```text
effective_period_ticks = ARR / PVINV_CFG_TIM8_DITHERING_SCALE
```

如果：

```text
ARR=48000
scale=16
```

则：

```text
effective_period_ticks=3000
```

但是CCR写入仍使用：

```c
ccr_period = TIM8->ARR;
center = 0.5f * ccr_period;
```

不要把CCR写入时的ARR除以16，否则占空比单位会错。

---

## 7.CubeMX保姆级配置教程

### 7.1打开工程

打开STM32CubeMX：

```text
File -> Open Project
```

打开你的`.ioc`文件。

注意：

```text
.uvprojx是Keil工程文件，不是CubeMX配置文件。
```

### 7.2确认芯片

确认型号：

```text
STM32H563RGT6
```

### 7.3系统时钟

如果使用25MHz外部时钟，常见配置：

```text
HSE = 25MHz
PLL Source = HSE
PLLM = 2
PLLN = 40
PLLR = 2
SYSCLK ≈ 250MHz
APB2 Prescaler = 1
TIM8_CLK ≈ 250MHz
```

HSE是Bypass还是Crystal必须以原理图为准。

### 7.4配置TIM8引脚

在Pinout中设置：

```text
PC6 -> TIM8_CH1
PA7 -> TIM8_CH1N
PC7 -> TIM8_CH2
PB0 -> TIM8_CH2N
```

### 7.5配置TIM8模式

进入：

```text
Timers -> TIM8
```

设置：

```text
Clock Source = Internal Clock
Channel1 = PWM Generation CH1 CH1N
Channel2 = PWM Generation CH2 CH2N
Channel3 = Disable
Channel4 = Disable
```

一定要选`CH1 CH1N`和`CH2 CH2N`，不能只开CH1/CH2。

### 7.6配置TIM8基础参数

推荐普通模式：

```text
Prescaler = 1
Counter Mode = Center Aligned Mode 1
Counter Period = 3124
Repetition Counter = 1
Auto Reload Preload = Enable
Dithering = Disable
```

推荐Dithering模式：

```text
Prescaler = 1
Counter Mode = Center Aligned Mode 1
Counter Period按CubeMX/HAL Dithering方式生成
Repetition Counter = 1
Auto Reload Preload = Enable
Dithering = Enable
```

如果保留Dithering，确认代码中：

```c
#define PVINV_CFG_TIM8_DITHERING_SCALE (16.0f)
```

是否和实际一致。

### 7.7普通模式频率

中心对齐PWM频率：

```text
fPWM = TIM8_CLK / [2 * (PSC+1) * (ARR+1)]
```

若：

```text
TIM8_CLK = 250MHz
PSC = 1
ARR = 3124
```

则：

```text
fPWM = 250MHz / [2 * 2 * 3125] = 20kHz
```

### 7.8Dithering模式频率

若：

```text
TIM8_CLK = 250MHz
PSC = 1
ARR寄存器 = 48000
scale = 16
```

则：

```text
effective_period_ticks = 48000 / 16 = 3000
fPWM = 250MHz / [2 * 2 * 3000] ≈ 20.8kHz
```

所以`ARR=48000`不代表普通ARR=48000。

### 7.9配置DeadTime

进入：

```text
TIM8 -> Break and Dead Time
```

设置：

```text
Dead Time = 非零
```

不要为0。没有死区时绝不能接功率桥。

### 7.10Break配置

开环逻辑测试阶段，如果没有硬件过流比较器，可以：

```text
Break Input = Disable
Break2 Input = Disable
Automatic Output = Disable
```

后续功率级建议加入硬件Break保护。

### 7.11开启TIM8更新中断

进入：

```text
TIM8 -> NVIC Settings
```

勾选：

```text
TIM8 update interrupt
```

可能显示为：

```text
TIM8 update global interrupt
TIM8_UP_IRQn
TIM8_UP_TIM13_IRQn
```

建议优先级：

```text
Preemption Priority = 1
Sub Priority = 0
```

### 7.12生成代码

进入：

```text
Project Manager
```

设置：

```text
Toolchain/IDE = MDK-ARM
```

然后：

```text
Generate Code
```

---

## 8.Keil使用方法

### 8.1复制文件

复制到：

```text
Core/Inc/pvinv_project_config.h
Core/Inc/pvinv_pwm_tim8.h
Core/Inc/pvinv_openloop_spwm.h

Core/Src/pvinv_pwm_tim8.c
Core/Src/pvinv_openloop_spwm.c
```

### 8.2加入Keil工程

在Keil左侧工程树中，右键包含`main.c`的Group：

```text
Add Existing Files to Group
```

添加：

```text
Core/Src/pvinv_pwm_tim8.c
Core/Src/pvinv_openloop_spwm.c
```

否则会出现：

```text
Undefined symbol PVINV_OpenLoopSPWM_Init
Undefined symbol PVINV_PWM_TIM8_SetUnipolarD
```

---

## 9.main.c修改方法

只改`USER CODE`区域。

### 9.1Includes

```c
/* USER CODE BEGIN Includes */
#include "pvinv_openloop_spwm.h"
#include "pvinv_pwm_tim8.h"
/* USER CODE END Includes */
```

### 9.2Private variables

```c
/* USER CODE BEGIN PV */
static PVINV_OL_Status_t g_ol_status;
static PVINV_PWM_Status_t g_pwm_status;
static PVINV_PWM_Diag_t g_pwm_diag;
/* USER CODE END PV */
```

### 9.3删除旧固定PWM代码

删除：

```c
HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
TIM8->CCR1 = 10000;
TIM8->CCR2 = 10000;
```

### 9.4USER CODE BEGIN 2

```c
/* USER CODE BEGIN 2 */

g_ol_status = PVINV_OpenLoopSPWM_Init();
if (g_ol_status != PVINV_OL_STATUS_OK)
{
    g_pwm_status = PVINV_PWM_TIM8_GetLastStatus();
    g_pwm_diag = PVINV_PWM_TIM8_GetDiag();

    (void)g_pwm_status;
    (void)g_pwm_diag;

    Error_Handler();
}

PVINV_OpenLoopSPWM_SetSineFrequencyHz(50.0f);
PVINV_OpenLoopSPWM_SetTargetModulation(0.03f);

g_ol_status = PVINV_OpenLoopSPWM_Start();
if (g_ol_status != PVINV_OL_STATUS_OK)
{
    g_pwm_status = PVINV_PWM_TIM8_GetLastStatus();
    g_pwm_diag = PVINV_PWM_TIM8_GetDiag();

    (void)g_pwm_status;
    (void)g_pwm_diag;

    Error_Handler();
}

/* USER CODE END 2 */
```

### 9.5USER CODE BEGIN 4

```c
/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(htim);
}

/* USER CODE END 4 */
```

如果工程已有`HAL_TIM_PeriodElapsedCallback()`，不要重复定义，只合并：

```c
PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(htim);
```

---

## 10.运行流程

```text
HAL_Init()
SystemClock_Config()
MX_GPIO_Init()
MX_GPDMA1_Init()
MX_ADC1_Init()
MX_ADC2_Init()
MX_I2C1_Init()
MX_TIM8_Init()
MX_UART4_Init()
MX_USART3_UART_Init()

PVINV_OpenLoopSPWM_Init()
PVINV_OpenLoopSPWM_SetSineFrequencyHz(50Hz)
PVINV_OpenLoopSPWM_SetTargetModulation(0.03)
PVINV_OpenLoopSPWM_Start()
```

TIM8更新中断后：

```text
TIM8 Update IRQ
-> HAL_TIM_IRQHandler(&htim8)
-> HAL_TIM_PeriodElapsedCallback()
-> PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback()
-> PVINV_OpenLoopSPWM_OnTim8UpdateIrq()
-> D=M*sin(theta)
-> PVINV_PWM_TIM8_SetUnipolarD(D)
-> 更新CCR1/CCR2
```

---

## 11.如何确认Dithering缩放系数

### 11.1看tim.c

如果看到：

```c
htim8.Init.Period = 3000;
HAL_TIMEx_DitheringEnable(&htim8);
__HAL_TIM_SET_AUTORELOAD(&htim8, 48000);
```

则：

```text
48000 / 3000 = 16
```

说明scale约为16。

### 11.2Keil Watch看寄存器

在`MX_TIM8_Init()`之后查看：

```c
TIM8->CR1
TIM8->ARR
TIM8->PSC
TIM8->RCR
```

如果：

```text
TIM8->ARR = 48000
TIM8->ARR >> 4 = 3000
TIM8->ARR & 0xF = 0
```

说明很可能是4bit扩展，即scale=16。

### 11.3示波器反推

公式：

```text
scale ≈ fPWM * 2 * (PSC+1) * ARR / TIM8_CLK
```

若：

```text
fPWM≈20.8kHz
PSC=1
ARR=48000
TIM8_CLK=250MHz
```

则：

```text
scale≈16
```

---

## 12.示波器验证

第一次测试不要接：

```text
高压母线
驱动板
功率桥
MOS/IGBT
负载
```

只测：

```text
PC6
PA7
PC7
PB0
```

### 12.1测PC6

应看到约20kHz PWM，占空比缓慢变化。

### 12.2测PA7

应看到PA7与PC6互补。

### 12.3同时测PC6和PA7

确认二者不同时为高，并且有死区。

### 12.4同时测PC7和PB0

确认二者互补并且有死区。

### 12.5同时测PC6和PC7

确认：

```text
PC6占空比增大时，PC7占空比减小；
PC6占空比减小时，PC7占空比增大。
```

### 12.6测50Hz包络

示波器时间轴设为：

```text
5ms/div或10ms/div
```

包络周期应约20ms。

---

## 13.常见错误

### 13.1`PVINV_PWM_STATUS_ERR_ZERO_DEADTIME`

原因：

```text
DeadTime=0
```

解决：

```text
CubeMX中TIM8 Dead Time设为非零。
```

### 13.2`PVINV_PWM_STATUS_ERR_CARRIER_TOO_LOW`

可能原因：

```text
1.ARR太大；
2.Dithering scale设置错误；
3.TIM8时钟估算错误；
4.PSC太大。
```

如果开启Dithering，重点检查：

```text
PVINV_CFG_TIM8_DITHERING_SCALE是否为16。
```

### 13.3`PVINV_PWM_STATUS_ERR_NOT_CENTER_ALIGNED`

解决：

```text
CubeMX中TIM8 Counter Mode设为Center Aligned Mode 1。
```

### 13.4没有PA7/PB0互补波

检查：

```text
1.Channel1是否为PWM Generation CH1 CH1N；
2.Channel2是否为PWM Generation CH2 CH2N；
3.PA7是否为TIM8_CH1N；
4.PB0是否为TIM8_CH2N；
5.pvinv_pwm_tim8.c是否加入Keil工程。
```

---

## 14.最终安全检查清单

烧录前确认：

```text
1.TIM8使用Internal Clock；
2.PC6=TIM8_CH1；
3.PA7=TIM8_CH1N；
4.PC7=TIM8_CH2；
5.PB0=TIM8_CH2N；
6.Channel1=PWM Generation CH1 CH1N；
7.Channel2=PWM Generation CH2 CH2N；
8.Counter Mode=Center Aligned；
9.DeadTime非零；
10.TIM8 Update interrupt Enable；
11.pvinv_pwm_tim8.c已加入Keil；
12.pvinv_openloop_spwm.c已加入Keil；
13.main.c已删除旧固定CCR代码；
14.没有接高压；
15.没有接功率桥。
```

示波器必须确认：

```text
1.PC6有PWM；
2.PA7有PWM；
3.PC7有PWM；
4.PB0有PWM；
5.PC6/PA7互补且有死区；
6.PC7/PB0互补且有死区；
7.PC6和PC7占空比反向变化；
8.载波约20kHz；
9.包络约50Hz。
```

未全部通过前，绝不能接功率桥。

---

## 15.后续扩展方向

开环发波验证通过后，再按顺序加入：

```text
1.ADC DMA采样；
2.Ud/Id/uREF/iF/uo raw验证；
3.采样比例系数和方向校准；
4.MPPT输入功率计算；
5.电导增量法；
6.uREF同步；
7.PR电流闭环；
8.保护逻辑；
9.低压限流功率桥测试；
10.逐步提高母线电压和负载。
```

---

## 16.最终总结

本代码的目标是：

```text
在STM32H563RGT6上，只使用TIM8，保留Dithering，提高PWM发波精度，输出开环单极性SPWM，并严格不生成CubeMX可自动生成的初始化代码。
```

最终验收标准：

```text
PC6/PA7互补且有死区；
PC7/PB0互补且有死区；
PC6和PC7占空比反向变化；
PWM载波约20kHz；
正弦包络约50Hz。
```
