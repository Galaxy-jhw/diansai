# STM32H563RGT6闭环控制代码V3使用说明与CubeMX保姆级配置教程

> 适用代码包：`pvinv_closedloop_tim8_spwm_mppt_pr_final_v3`  
> 适用芯片：STM32H563RGT6  
> 控制目标：TIM8四路互补PWM + Dithering + 单极性SPWM + ADC DMA采样 + MPPT电导增量法 + PV电压外环PI + PR电流环  
> 重要原则：**能够由CubeMX自动生成的代码，本代码包不生成、不替代、不重写。**

---

## 0.先说结论：这份代码到底是什么

这份代码是一个**闭环控制用户层代码框架**，不是完整CubeMX工程，也不是最终免调试功率级程序。

它做的事情是：

```text
ADC DMA采样
-> Ud、Id、uREF、iF、uo换算
-> ADC预热
-> MPPT电导增量法更新Vmppt_ref
-> PV电压外环PI生成电流幅值i_amp_cmd
-> i_ref = i_amp_cmd * ref_unit
-> PR电流环计算pr_out
-> D = (v_ff + pr_out) / Ud
-> TIM8单极性SPWM更新CCR1/CCR2
```

它不生成这些CubeMX可以生成的代码：

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

这些内容必须仍由CubeMX根据`.ioc`自动生成。

---

## 1.代码包文件结构

推荐放入工程后的结构如下：

```text
Your_Project/
├── Core/
│   ├── Inc/
│   │   ├── pvinv_project_config.h
│   │   ├── pvinv_utils.h
│   │   ├── pvinv_pwm_tim8.h
│   │   ├── pvinv_adc.h
│   │   ├── pvinv_pi.h
│   │   ├── pvinv_pr.h
│   │   ├── pvinv_mppt_inccond.h
│   │   └── pvinv_control.h
│   └── Src/
│       ├── pvinv_pwm_tim8.c
│       ├── pvinv_adc.c
│       ├── pvinv_pi.c
│       ├── pvinv_pr.c
│       ├── pvinv_mppt_inccond.c
│       └── pvinv_control.c
├── examples/
│   └── main_user_code_snippets.c
└── README_CLOSEDLOOP_TIM8_SPWM_MPPT_PR_CubeMX_Guide.md
```

---

## 2.各文件作用说明

### 2.1`pvinv_project_config.h`

这是最重要的配置文件。

它管理：

```text
1.TIM8 Dithering是否允许；
2.Dithering缩放系数；
3.TIM8载波频率允许范围；
4.TIM8控制中断频率允许范围；
5.ADC DMA数组长度；
6.ADC Rank索引；
7.Ud、Id、iF、uo比例系数；
8.uREF偏置和幅值；
9.MPPT参数；
10.PV电压外环PI参数；
11.PR电流环参数；
12.软启动速度；
13.保护阈值。
```

特别注意：

```c
#define PVINV_UD_SCALE (1.0f)
#define PVINV_ID_SCALE (1.0f)
#define PVINV_IF_SCALE (1.0f)
#define PVINV_UO_SCALE (1.0f)
```

这些是**占位值**，不是最终硬件参数。真实上板前必须标定。

---

### 2.2`pvinv_pwm_tim8.h/.c`

TIM8底层发波封装。

负责：

```text
1.检查TIM8是否中心对齐；
2.检查DeadTime是否非零；
3.识别Dithering是否开启；
4.计算PWM载波频率；
5.计算控制中断频率；
6.启动TIM8_CH1/CH1N、TIM8_CH2/CH2N；
7.实现单极性SPWM底层写CCR；
8.故障时快速关断TIM8输出。
```

核心函数：

```c
PVINV_PWM_TIM8_SetUnipolarD(float d);
```

内部执行：

```c
CCR1 = center + center * d;
CCR2 = center - center * d;
```

这就是单极性SPWM的底层核心。

---

### 2.3`pvinv_adc.h/.c`

ADC用户层采样模块。

负责：

```text
1.定义ADC1 DMA raw数组；
2.定义ADC2 DMA raw数组；
3.启动HAL_ADC_Start_DMA；
4.把ADC raw转换成物理量；
5.对Ud、Id、uREF、iF、uo进行滤波；
6.第一次采样直接初始化滤波值，避免启动误欠压。
```

它不生成：

```text
MX_ADC1_Init()
MX_ADC2_Init()
MX_GPDMA1_Init()
```

但它会调用：

```c
HAL_ADC_Start_DMA(&hadc1, ...);
HAL_ADC_Start_DMA(&hadc2, ...);
```

这是用户层启动DMA，不是CubeMX初始化代码。

---

### 2.4`pvinv_mppt_inccond.h/.c`

MPPT电导增量法模块。

核心思想：

```text
dP/dV = I + V * dI/dV
```

代码中等价判断为：

```text
slope = dI/dV + I/V
```

判断逻辑：

```text
slope > 0：在MPP左侧，升高Vref；
slope < 0：在MPP右侧，降低Vref；
slope ≈ 0：接近MPP，保持Vref。
```

然后PV电压外环PI根据：

```text
error = Ud - Vmppt_ref
```

生成输出电流幅值命令：

```text
i_amp_cmd
```

---

### 2.5`pvinv_pr.h/.c`

PR电流环模块。

当前实现的是准PR控制器：

```text
Gres(s) = Kr * 2wc*s / (s^2 + 2wc*s + w0^2)
```

并用Tustin方式离散。

默认参数：

```c
#define PVINV_PR_KP    (0.10f)
#define PVINV_PR_KR    (20.0f)
#define PVINV_PR_F0_HZ (50.0f)
#define PVINV_PR_WC_HZ (5.0f)
```

这些只是初始值，最终必须根据你的电感、采样频率、负载、电流采样比例调试。

---

### 2.6`pvinv_control.h/.c`

闭环主控制模块。

状态机包括：

```text
IDLE
READY
ADC_WARMUP
SOFTSTART
CLOSED_LOOP
FAULT
```

控制流程：

```text
PVINV_Control_Init()
-> READY

PVINV_Control_Start()
-> 启动ADC DMA
-> 启动TIM8四路PWM
-> 进入ADC_WARMUP

ADC_WARMUP
-> 预热ADC和滤波
-> 结束后进入SOFTSTART

SOFTSTART
-> 电流幅值逐渐爬升
-> 完成后进入CLOSED_LOOP

CLOSED_LOOP
-> 正常MPPT + PR电流闭环
```

---

## 3.CubeMX配置总览

你需要在CubeMX中配置：

```text
1.系统时钟；
2.TIM8四路互补PWM；
3.TIM8 Update interrupt；
4.TIM8 DeadTime；
5.TIM8 Dithering；
6.ADC1 Regular Conversion；
7.ADC2 Regular Conversion；
8.ADC1/ADC2 DMA Circular；
9.GPIO模拟输入；
10.Keil工程生成设置。
```

本教程下面一步一步讲。

---

# 4.CubeMX保姆级配置：工程与芯片

## 4.1打开工程

打开STM32CubeMX。

选择：

```text
File -> Open Project
```

打开你的`.ioc`文件。

注意：

```text
.uvprojx是Keil工程文件，不是CubeMX配置文件。
CubeMX要打开的是.ioc文件。
```

---

## 4.2确认芯片型号

确认芯片为：

```text
STM32H563RGT6
```

不要选错成：

```text
STM32G431
STM32G474
STM32H563其他封装
```

芯片型号和封装错误会导致引脚复用、TIM8通道、ADC通道都可能不同。

---

# 5.CubeMX保姆级配置：系统时钟

## 5.1进入Clock Configuration

点击顶部：

```text
Clock Configuration
```

如果你的板子使用25MHz外部时钟，常见配置可以是：

```text
HSE = 25MHz
PLL Source = HSE
SYSCLK ≈ 250MHz
APB2 Prescaler = 1
TIM8_CLK ≈ 250MHz
```

具体PLL参数必须以你的工程和板级时钟为准。

---

## 5.2HSE模式选择

如果你的硬件是外部有源晶振输入：

```text
HSE = Bypass Clock Source
```

如果你的硬件是普通无源晶振：

```text
HSE = Crystal/Ceramic Resonator
```

这个必须看原理图，不能随便选。

---

## 5.3为什么TIM8时钟重要

代码里会估算TIM8时钟：

```c
HAL_RCC_GetPCLK2Freq()
```

然后计算：

```text
PWM载波频率
TIM8控制中断频率
PR控制器采样周期
MPPT执行周期
```

如果TIM8实际时钟和代码估算不一致，闭环参数都会偏。

如果你实测发现不一致，可以在`pvinv_project_config.h`里打开手动TIM8时钟：

```c
#define PVINV_CFG_USE_MANUAL_TIM8_CLK_HZ (1u)
#define PVINV_CFG_MANUAL_TIM8_CLK_HZ     (250000000.0f)
```

---

# 6.CubeMX保姆级配置：TIM8四路互补PWM

## 6.1配置TIM8引脚

在Pinout界面设置：

```text
PC6 -> TIM8_CH1
PA7 -> TIM8_CH1N
PC7 -> TIM8_CH2
PB0 -> TIM8_CH2N
```

对应关系：

```text
TIM8_CH1  -> PC6 -> A桥臂上管
TIM8_CH1N -> PA7 -> A桥臂下管

TIM8_CH2  -> PC7 -> B桥臂上管
TIM8_CH2N -> PB0 -> B桥臂下管
```

---

## 6.2启用TIM8

左侧选择：

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

一定要选择：

```text
PWM Generation CH1 CH1N
PWM Generation CH2 CH2N
```

不能只选：

```text
PWM Generation CH1
PWM Generation CH2
```

否则互补输出`PA7`和`PB0`可能没有波形。

---

## 6.3TIM8基础参数推荐

进入：

```text
TIM8 -> Parameter Settings
```

推荐设置：

```text
Prescaler = 1
Counter Mode = Center Aligned Mode 1
Counter Period = 根据Dithering/目标载波设置
Repetition Counter = 1
Auto Reload Preload = Enable
```

推荐目标：

```text
PWM载波约20kHz
控制中断约20kHz
正弦基波50Hz
```

---

## 6.4普通模式下的Period示例

如果不启用Dithering，中心对齐PWM频率为：

```text
fPWM = TIM8_CLK / [2 * (PSC+1) * (ARR+1)]
```

例如：

```text
TIM8_CLK = 250MHz
PSC = 1
ARR = 3124
```

则：

```text
fPWM = 250MHz / [2 * 2 * 3125]
     = 20kHz
```

---

## 6.5Dithering模式下的Period示例

如果启用Dithering，代码默认认为：

```text
Dithering scale = 16
```

如果你看到：

```text
TIM8->ARR = 48000
```

则等效周期为：

```text
effective_period_ticks = 48000 / 16 = 3000
```

中心对齐载波频率：

```text
fPWM = 250MHz / [2 * 2 * 3000]
     ≈ 20.83kHz
```

所以：

```text
ARR=48000不是普通意义的48000；
它是Dithering扩展后的寄存器单位。
```

---

## 6.6Dithering设置

CubeMX中如果有Dithering选项：

```text
Dithering = Enable
```

代码中必须保持：

```c
#define PVINV_CFG_ALLOW_TIM8_DITHERING (1u)
#define PVINV_CFG_TIM8_DITHERING_SCALE (16.0f)
```

如果实测发现scale不是16，只改：

```c
#define PVINV_CFG_TIM8_DITHERING_SCALE (...)
```

不要随便改CCR算法。

---

## 6.7DeadTime配置

进入：

```text
TIM8 -> Break and Dead Time
```

必须设置：

```text
Dead Time = 非零
```

当前代码默认不允许DeadTime为0：

```c
#define PVINV_CFG_ALLOW_ZERO_DEADTIME_LOGIC_ONLY (0u)
```

如果CubeMX里DeadTime=0，代码会报错，不会正常启动。

原因：

```text
上下桥臂没有死区时，接功率桥可能直通烧管。
```

---

## 6.8Break配置

初期主控板逻辑验证阶段，如果没有硬件过流比较器，可以暂时：

```text
Break Input = Disable
Break2 Input = Disable
Automatic Output = Disable
```

正式功率级建议加入硬件保护：

```text
驱动故障/硬件过流 -> TIM8 BKIN
```

软件保护不能完全替代硬件Break。

---

## 6.9TIM8 NVIC中断配置

进入：

```text
TIM8 -> NVIC Settings
```

勾选：

```text
TIM8 Update interrupt
```

不同CubeMX版本可能显示为：

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

TIM8更新中断必须进入HAL回调：

```c
HAL_TIM_PeriodElapsedCallback()
```

然后在回调中调用：

```c
PVINV_Control_TIM_PeriodElapsedCallback(htim);
```

---

# 7.CubeMX保姆级配置：ADC1

## 7.1ADC1用途

ADC1用于采集：

```text
Rank1 -> Ud
Rank2 -> Id
Rank3 -> uREF
```

代码中对应：

```c
#define PVINV_ADC1_DMA_LEN  (3u)

#define PVINV_ADC1_IDX_UD   (0u)
#define PVINV_ADC1_IDX_ID   (1u)
#define PVINV_ADC1_IDX_UREF (2u)
```

所以CubeMX里的ADC1 Rank顺序必须严格对应。

---

## 7.2ADC1通道建议

根据前面代码约定：

```text
ADC1_IN10 Differential -> Ud
ADC1_IN12 Differential -> Id
ADC1_IN4  Single-ended -> uREF
```

如果你的`.ioc`实际通道不同，代码可以用，但你必须保证Rank索引和物理意义一致。

---

## 7.3ADC1参数设置

进入：

```text
Analog -> ADC1
```

建议：

```text
Resolution = 12 bits
Data Alignment = Right alignment
Scan Conversion Mode = Enable
Continuous Conversion Mode = Enable
DMA Continuous Requests = Enable
End Of Conversion Selection = End of sequence
Number Of Conversion = 3
Overrun = Overrun data overwritten 或 preserve按工程需要
```

初期建议使用：

```text
Continuous Conversion Mode = Enable
DMA Circular
```

这样ADC DMA一直刷新，TIM8 ISR读取最近一次采样结果。

---

## 7.4ADC1 Regular Rank设置

设置：

```text
Rank 1 = Ud通道
Rank 2 = Id通道
Rank 3 = uREF通道
```

例如：

```text
Rank1: ADC1_IN10 Differential
Rank2: ADC1_IN12 Differential
Rank3: ADC1_IN4 Single-ended
```

必须确保：

```text
ADC1 DMA buffer[0] = Ud
ADC1 DMA buffer[1] = Id
ADC1 DMA buffer[2] = uREF
```

---

## 7.5ADC1 DMA设置

在ADC1的DMA Settings中添加DMA。

推荐：

```text
DMA Request = ADC1
Direction = Peripheral to Memory
Mode = Circular
Peripheral Increment = Disable
Memory Increment = Enable
Peripheral Data Width = Word 或 Half Word
Memory Data Width = Word 或 Half Word
Priority = High
```

由于代码buffer定义为：

```c
volatile uint32_t g_adc1_raw[3];
```

如果DMA宽度设置为Word最直接；如果你使用Half Word，也通常可以，但要确认HAL DMA搬运结果和数组读取正确。

保守建议：

```text
Peripheral Data Width = Word
Memory Data Width = Word
```

---

# 8.CubeMX保姆级配置：ADC2

## 8.1ADC2用途

ADC2用于采集：

```text
Rank1 -> iF
Rank2 -> uo
```

代码中对应：

```c
#define PVINV_ADC2_DMA_LEN (2u)

#define PVINV_ADC2_IDX_IF  (0u)
#define PVINV_ADC2_IDX_UO  (1u)
```

---

## 8.2ADC2通道建议

根据前面代码约定：

```text
ADC2_IN1  Differential -> iF
ADC2_IN18 Differential -> uo
```

如果你的实际`.ioc`不同，必须保证Rank索引和物理意义一致。

---

## 8.3ADC2参数设置

进入：

```text
Analog -> ADC2
```

建议：

```text
Resolution = 12 bits
Data Alignment = Right alignment
Scan Conversion Mode = Enable
Continuous Conversion Mode = Enable
DMA Continuous Requests = Enable
End Of Conversion Selection = End of sequence
Number Of Conversion = 2
```

---

## 8.4ADC2 Regular Rank设置

设置：

```text
Rank 1 = iF通道
Rank 2 = uo通道
```

例如：

```text
Rank1: ADC2_IN1 Differential
Rank2: ADC2_IN18 Differential
```

必须确保：

```text
ADC2 DMA buffer[0] = iF
ADC2 DMA buffer[1] = uo
```

---

## 8.5ADC2 DMA设置

推荐：

```text
DMA Request = ADC2
Direction = Peripheral to Memory
Mode = Circular
Peripheral Increment = Disable
Memory Increment = Enable
Peripheral Data Width = Word
Memory Data Width = Word
Priority = High
```

---

# 9.CubeMX保姆级配置：GPIO模拟输入

所有ADC引脚必须配置为：

```text
Analog
No Pull
```

如果ADC引脚被错误配置成普通GPIO、复用输出、上拉/下拉，会导致采样错误。

你需要检查：

```text
Ud采样引脚
Id采样引脚
uREF采样引脚
iF采样引脚
uo采样引脚
```

---

# 10.CubeMX代码生成设置

进入：

```text
Project Manager
```

设置：

```text
Toolchain/IDE = MDK-ARM
```

推荐勾选：

```text
Generate peripheral initialization as a pair of .c/.h files per peripheral
```

然后点击：

```text
Generate Code
```

---

# 11.Keil工程添加文件

## 11.1复制文件

把这些头文件复制到：

```text
Core/Inc/
```

```text
pvinv_project_config.h
pvinv_utils.h
pvinv_pwm_tim8.h
pvinv_adc.h
pvinv_pi.h
pvinv_pr.h
pvinv_mppt_inccond.h
pvinv_control.h
```

把这些源文件复制到：

```text
Core/Src/
```

```text
pvinv_pwm_tim8.c
pvinv_adc.c
pvinv_pi.c
pvinv_pr.c
pvinv_mppt_inccond.c
pvinv_control.c
```

---

## 11.2在Keil中添加.c文件

打开Keil工程。

在左侧工程树中找到：

```text
Application/User/Core
```

或包含`main.c`的Group。

右键：

```text
Add Existing Files to Group
```

添加：

```text
pvinv_pwm_tim8.c
pvinv_adc.c
pvinv_pi.c
pvinv_pr.c
pvinv_mppt_inccond.c
pvinv_control.c
```

如果忘记添加，会出现类似：

```text
Undefined symbol PVINV_Control_Init
Undefined symbol PVINV_PWM_TIM8_SetUnipolarD
Undefined symbol PVINV_ADC_Update
```

---

# 12.main.c使用方法

只修改`USER CODE`区域。

---

## 12.1添加头文件

找到：

```c
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */
```

改为：

```c
/* USER CODE BEGIN Includes */
#include "pvinv_control.h"
/* USER CODE END Includes */
```

---

## 12.2添加变量

找到：

```c
/* USER CODE BEGIN PV */

/* USER CODE END PV */
```

加入：

```c
/* USER CODE BEGIN PV */
static PVINV_Status_t g_pvinv_status;
static PVINV_Diag_t g_pvinv_diag;
/* USER CODE END PV */
```

---

## 12.3初始化闭环控制

找到：

```c
/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
```

加入：

```c
/* USER CODE BEGIN 2 */

g_pvinv_status = PVINV_Control_Init();
if (g_pvinv_status != PVINV_OK)
{
    g_pvinv_diag = PVINV_Control_GetDiag();
    (void)g_pvinv_diag;
    Error_Handler();
}

/*
 * 第一次真实上板建议先不要启动闭环。
 * 推荐先完成开环PWM验证、ADC raw验证和比例标定。
 */
g_pvinv_status = PVINV_Control_Start();
if (g_pvinv_status != PVINV_OK)
{
    g_pvinv_diag = PVINV_Control_GetDiag();
    (void)g_pvinv_diag;
    Error_Handler();
}

/* USER CODE END 2 */
```

---

## 12.4主循环加入Service

找到：

```c
while (1)
{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
}
```

改为：

```c
while (1)
{
    PVINV_Control_Service();

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
}
```

`PVINV_Control_Service()`用于故障后在主循环中执行HAL Stop清理，避免在高频中断中完整调用HAL Stop。

---

## 12.5添加TIM回调

找到：

```c
/* USER CODE BEGIN 4 */

/* USER CODE END 4 */
```

加入：

```c
/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_Control_TIM_PeriodElapsedCallback(htim);
}

/* USER CODE END 4 */
```

如果你的工程里已经存在`HAL_TIM_PeriodElapsedCallback()`，不要重复定义。只把下面这句合并进去：

```c
PVINV_Control_TIM_PeriodElapsedCallback(htim);
```

---

# 13.运行状态机详细说明

## 13.1`PVINV_Control_Init()`

执行内容：

```text
1.初始化ADC用户层缓冲；
2.检查TIM8配置；
3.计算TIM8载波频率；
4.计算控制ISR频率；
5.初始化MPPT；
6.初始化PR控制器；
7.进入READY状态。
```

如果失败，可能原因：

```text
TIM8不是中心对齐；
TIM8 DeadTime为0；
TIM8载波频率超范围；
TIM8 Update频率超范围；
Dithering不允许但CubeMX开启了Dithering。
```

---

## 13.2`PVINV_Control_Start()`

执行内容：

```text
1.只允许READY状态启动；
2.启动ADC1 DMA；
3.启动ADC2 DMA；
4.启动TIM8四路互补PWM；
5.复位MPPT；
6.复位PR；
7.复位ADC滤波；
8.使能TIM8 Update中断；
9.进入ADC_WARMUP状态。
```

---

## 13.3`ADC_WARMUP`

目的：

```text
让ADC DMA数据和滤波值稳定，避免滤波初值为0导致启动欠压误故障。
```

配置：

```c
#define PVINV_ADC_WARMUP_ISR_TICKS (200u)
```

如果控制ISR约20kHz，则预热时间约：

```text
200 / 20000 = 10ms
```

---

## 13.4`SOFTSTART`

软启动期间：

```text
MPPT和PR开始工作；
电流幅值i_amp_cmd会乘以softstart_gain；
softstart_gain从0逐渐增加到1。
```

配置：

```c
#define PVINV_SOFTSTART_RAMP_PER_SEC (0.50f)
```

如果为`0.50`，表示大约2秒从0爬升到1。

---

## 13.5`CLOSED_LOOP`

进入正常闭环：

```text
MPPT更新Vmppt_ref；
PV电压PI生成i_amp_cmd；
PR电流环控制iF跟踪i_ref；
TIM8单极性SPWM输出D。
```

---

## 13.6`FAULT`

故障来源包括：

```text
PWM配置错误
ADC启动错误
输入欠压
输入过压
输入过流
输出过流
输出过压
PWM更新错误
```

故障发生后：

```text
ISR中快速关闭TIM8 MOE；
主循环中PVINV_Control_Service()执行HAL Stop清理。
```

---

# 14.闭环控制数学关系

## 14.1MPPT功率计算

```text
Ppv = Ud * Id
```

因此必须保证：

```text
光伏向系统供电时，Id为正。
```

如果方向反了，修改：

```c
#define PVINV_ID_SIGN (-1.0f)
```

---

## 14.2电导增量法

```text
dP/dV = I + V*dI/dV
```

代码中使用：

```text
slope = dI/dV + I/V
```

判断：

```text
slope > 0：升高Vref；
slope < 0：降低Vref；
slope ≈ 0：保持Vref。
```

---

## 14.3PV电压外环

```text
error = Ud - Vmppt_ref
```

逻辑：

```text
Ud高于目标 -> 增加输出电流幅值；
Ud低于目标 -> 降低输出电流幅值。
```

输出：

```text
i_amp_cmd
```

---

## 14.4电流参考

如果使用外部uREF：

```c
#define PVINV_CFG_USE_EXTERNAL_UREF (1u)
```

则：

```text
ref_unit = uREF_norm
i_ref = i_amp_soft * ref_unit
```

其中：

```text
uREF_norm ≈ [-1, +1]
```

---

## 14.5PR电流环

```text
i_err = i_ref - iF
pr_out = PR(i_err)
```

---

## 14.6单极性SPWM调制量

```text
D = (v_ff + pr_out) / Ud
```

然后底层：

```text
CCR1 = center + center*D
CCR2 = center - center*D
```

---

# 15.必须标定的参数

在`pvinv_project_config.h`中，以下参数必须标定：

```c
#define PVINV_UD_SCALE  (1.0f)
#define PVINV_ID_SCALE  (1.0f)
#define PVINV_IF_SCALE  (1.0f)
#define PVINV_UO_SCALE  (1.0f)

#define PVINV_UD_SIGN   (1.0f)
#define PVINV_ID_SIGN   (1.0f)
#define PVINV_IF_SIGN   (1.0f)
#define PVINV_UO_SIGN   (1.0f)

#define PVINV_UREF_BIAS_V      (1.650f)
#define PVINV_UREF_AMPLITUDE_V (1.500f)
```

标定前，不能直接认为：

```text
Ud是真实母线电压；
Id是真实输入电流；
iF是真实输出电流；
uo是真实输出电压。
```

---

# 16.如何标定ADC比例

## 16.1标定Ud

给输入端施加已知电压，例如：

```text
Ud实际 = 20.0V
```

在Keil Watch中看：

```text
g_pvinv_diag.meas.ud
g_pvinv_diag.meas.ud_f
g_pvinv_diag.meas.ud_code
```

如果当前`ud_f=0.8`，说明比例不对。

比例系数应改为：

```text
新的PVINV_UD_SCALE = 旧PVINV_UD_SCALE * 实际电压 / 当前显示电压
```

例如：

```text
旧scale=1.0
实际=20.0
显示=0.8
新scale=25.0
```

修改：

```c
#define PVINV_UD_SCALE (25.0f)
```

---

## 16.2标定Id

给输入端一个已知电流，例如：

```text
Id实际 = 1.0A
```

观察：

```text
g_pvinv_diag.meas.id_f
```

如果显示方向反了，先改：

```c
#define PVINV_ID_SIGN (-1.0f)
```

再按比例修正：

```text
新scale = 旧scale * 实际电流 / 显示电流
```

---

## 16.3标定iF

让输出电流传感器通过已知电流。

观察：

```text
g_pvinv_diag.meas.iF_f
```

方向反了就改：

```c
#define PVINV_IF_SIGN (-1.0f)
```

比例不对就改：

```c
#define PVINV_IF_SCALE (...)
```

---

## 16.4标定uo

给输出电压采样通道已知电压。

观察：

```text
g_pvinv_diag.meas.uo_f
```

方向反了就改：

```c
#define PVINV_UO_SIGN (-1.0f)
```

比例不对就改：

```c
#define PVINV_UO_SCALE (...)
```

---

## 16.5标定uREF

uREF是单端输入，默认：

```c
#define PVINV_UREF_BIAS_V      (1.650f)
#define PVINV_UREF_AMPLITUDE_V (1.500f)
```

如果你的信号发生器输出是：

```text
1.65V偏置
3.0Vpp正弦
```

则幅值为：

```text
1.5V
```

此时默认值合理。

观察：

```text
g_pvinv_diag.meas.uref_v
g_pvinv_diag.meas.uref_norm_f
```

目标：

```text
uREF_norm_f大约在-1到+1之间变化。
```

如果偏置不是1.65V，修改：

```c
#define PVINV_UREF_BIAS_V (...)
```

如果幅值不是1.5V，修改：

```c
#define PVINV_UREF_AMPLITUDE_V (...)
```

---

# 17.Dithering实测确认方法

## 17.1看寄存器

在Keil Watch里看：

```c
g_pvinv_diag.pwm.arr_reg
g_pvinv_diag.pwm.effective_period_ticks
g_pvinv_diag.pwm.carrier_hz
g_pvinv_diag.pwm.dithering_enabled
```

如果：

```text
ARR≈48000
effective_period_ticks≈3000
carrier_hz≈20kHz
```

说明`scale=16`大概率正确。

---

## 17.2示波器反推

测PC6的PWM载波频率。

公式：

```text
scale = ARR / effective_period_ticks
effective_period_ticks = TIM8_CLK / [2*(PSC+1)*fPWM]
```

如果：

```text
TIM8_CLK=250MHz
PSC=1
ARR=48000
fPWM≈20.8kHz
```

则：

```text
scale≈16
```

---

# 18.示波器验证顺序

## 18.1第一阶段：不接功率桥

只测主控板引脚：

```text
PC6
PA7
PC7
PB0
```

确认：

```text
PC6有PWM；
PA7与PC6互补；
PC7有PWM；
PB0与PC7互补；
PC6/PA7有死区；
PC7/PB0有死区；
PC6和PC7占空比反向变化。
```

---

## 18.2第二阶段：ADC raw验证

只验证采样，不接高压功率级。

Watch查看：

```c
g_pvinv_diag.meas.adc1_raw[0]
g_pvinv_diag.meas.adc1_raw[1]
g_pvinv_diag.meas.adc1_raw[2]
g_pvinv_diag.meas.adc2_raw[0]
g_pvinv_diag.meas.adc2_raw[1]
```

确认它们分别对应：

```text
Ud
Id
uREF
iF
uo
```

---

## 18.3第三阶段：低压限流闭环

只在低压、限流、可控负载下测试。

建议：

```text
低母线电压
限流电源
假负载
示波器监测PWM和电流
逐步提高i_amp限制
```

---

# 19.常见错误与排查

## 19.1报`PWM_CONFIG`

可能原因：

```text
TIM8不是中心对齐；
DeadTime=0；
Dithering不允许；
载波频率不在范围；
更新中断频率不在范围。
```

查看：

```c
g_pvinv_diag.pwm
```

---

## 19.2一启动就欠压

V3已经加入ADC_WARMUP，但如果仍欠压，检查：

```text
Ud比例系数是否错误；
Ud通道Rank是否错误；
PVINV_PROT_UD_MIN是否设置过高；
ADC DMA是否没有刷新。
```

---

## 19.3PWM有载波但闭环不动

检查：

```text
TIM8 Update interrupt是否开启；
HAL_TIM_PeriodElapsedCallback是否调用PVINV_Control_TIM_PeriodElapsedCallback；
g_pvinv_diag.state是否停在ADC_WARMUP或FAULT。
```

---

## 19.4ADC值全是0

检查：

```text
ADC DMA是否Circular；
DMA请求是否正确；
HAL_ADC_Start_DMA是否成功；
ADC通道是否配置为Analog；
ADC Clock是否开启；
DMA中断/请求是否被CubeMX正确生成。
```

---

## 19.5MPPT方向不对

检查：

```text
Id方向是否为正；
PVINV_ID_SIGN是否需要改成-1；
PVINV_MPPT_VREF_STEP是否过大；
PVINV_MPPT_VREF_MIN/MAX是否合理。
```

---

## 19.6电流环发散

检查：

```text
iF方向是否正确；
PVINV_IF_SIGN是否需要取反；
PR参数是否过大；
电流采样比例是否错误；
D限幅是否频繁触发；
硬件滤波和采样噪声是否过大。
```

---

# 20.最终安全检查清单

烧录前确认：

```text
1.TIM8_CH1=PC6；
2.TIM8_CH1N=PA7；
3.TIM8_CH2=PC7；
4.TIM8_CH2N=PB0；
5.TIM8为中心对齐；
6.TIM8 DeadTime非零；
7.TIM8 Update interrupt开启；
8.ADC1 DMA长度=3；
9.ADC2 DMA长度=2；
10.ADC1 Rank顺序=Ud、Id、uREF；
11.ADC2 Rank顺序=iF、uo；
12.pvinv所有.c文件已加入Keil工程；
13.main.c已加入PVINV_Control_Init；
14.main.c已加入PVINV_Control_Start；
15.main while中加入PVINV_Control_Service；
16.TIM回调中加入PVINV_Control_TIM_PeriodElapsedCallback。
```

上功率前确认：

```text
1.开环PWM已经验证；
2.Dithering scale已经确认；
3.20kHz载波已经确认；
4.单极性SPWM反向调制已经确认；
5.ADC raw顺序已经确认；
6.Ud比例已经标定；
7.Id比例和方向已经标定；
8.iF比例和方向已经标定；
9.uo比例和方向已经标定；
10.uREF归一化已经标定；
11.PR参数已经从小到大调试；
12.MPPT步长设置保守；
13.保护阈值合理；
14.使用限流电源；
15.不直接接高压母线。
```

---

# 21.上传GitHub建议

建议GitHub仓库结构：

```text
PV-Inverter-Control/
├── Core/
│   ├── Inc/
│   └── Src/
├── examples/
├── docs/
│   └── README_CLOSEDLOOP_TIM8_SPWM_MPPT_PR_CubeMX_Guide.md
├── README.md
└── LICENSE
```

建议在仓库README最上方写明：

```text
本项目为STM32H563RGT6光伏逆变闭环控制用户层代码。
CubeMX生成的底层初始化代码不包含在本代码包中。
真实功率级测试前必须完成开环PWM验证、ADC标定和低压限流闭环测试。
```

---

# 22.一句话总结

这份闭环代码的使用逻辑是：

```text
CubeMX负责生成底层外设初始化；
本代码包负责TIM8单极性SPWM、ADC测量、MPPT、PV电压外环PI、PR电流环和状态机；
真实上板前必须完成Dithering、ADC比例、方向、死区和低压限流验证。
```
