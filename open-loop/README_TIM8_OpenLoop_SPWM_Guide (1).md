# STM32H563RGT6 TIM8开环单极性SPWM使用说明与CubeMX配置教程

> 适用项目：基于STM32H563RGT6的电赛/逆变控制开环发波验证  
> 当前阶段目标：只验证TIM8四路互补PWM、死区、20kHz左右载波、50Hz正弦包络、A/B桥臂反向调制  
> 不包含：MPPT、电导增量法、ADC DMA、uREF同步、PR电流闭环、功率级闭环控制

---

## 0.最重要结论

本开环代码**只使用TIM8**完成发波。

不使用TIM1、TIM2、TIM3、HRTIM或其他定时器参与开环发波。

TIM8承担两个任务：

1.产生四路互补PWM：

```text
TIM8_CH1  -> PC6 -> A桥臂上管驱动输入
TIM8_CH1N -> PA7 -> A桥臂下管驱动输入

TIM8_CH2  -> PC7 -> B桥臂上管驱动输入
TIM8_CH2N -> PB0 -> B桥臂下管驱动输入
```

2.通过TIM8更新中断周期性刷新`CCR1`和`CCR2`，实现开环单极性SPWM：

```c
D = M * sin(theta);

TIM8->CCR1 = center + center * D;
TIM8->CCR2 = center - center * D;
```

其中：

```text
D>0时：CCR1增大，CCR2减小
D<0时：CCR1减小，CCR2增大
D=0时：CCR1和CCR2都约为50%占空比
```

这就是本代码的单极性SPWM核心。

---

## 1.这份代码在测试什么

本代码只测试以下内容：

```text
1.PC6/TIM8_CH1是否能输出PWM
2.PA7/TIM8_CH1N是否能输出PC6的互补PWM
3.PC7/TIM8_CH2是否能输出PWM
4.PB0/TIM8_CH2N是否能输出PC7的互补PWM
5.PC6和PA7之间是否有死区
6.PC7和PB0之间是否有死区
7.PC6和PC7占空比是否反向变化
8.PWM载波频率是否约为20kHz
9.占空比正弦包络是否约为50Hz
```

本代码不测试以下内容：

```text
1.不测试MPPT
2.不测试电导增量法
3.不测试PR电流闭环
4.不测试uREF同步
5.不测试ADC DMA采样
6.不测试输入电压Ud
7.不测试输入电流Id
8.不测试输出电流iF
9.不测试输出电压uo
10.不测试完整保护逻辑
```

当前阶段的定位是：

```text
低风险验证TIM8开环单极性SPWM发波。
```

---

## 2.为什么它是单极性SPWM

单极性SPWM的关键不是“有PWM”，而是全桥两个桥臂的占空比围绕50%反向变化。

本代码中：

```c
CCR1 = center + center * D;
CCR2 = center - center * D;
```

等价于：

```text
A桥臂占空比 = 0.5 + 0.5D
B桥臂占空比 = 0.5 - 0.5D
```

当`D=M*sin(theta)`时：

```text
A桥臂按正弦规律增减
B桥臂按相反方向增减
```

所以它实现的是开环单极性SPWM调制关系。

---

## 3.强制安全说明

第一次测试时，必须遵守以下规则：

```text
1.不要接高压母线
2.不要接功率桥
3.不要接MOS/IGBT驱动板
4.不要接负载
5.只测主控板PC6、PA7、PC7、PB0四个逻辑引脚
6.必须先确认互补关系正确
7.必须先确认死区存在
8.没有死区时绝不能接功率桥
9.PC6/PC7占空比不反向变化时绝不能接功率桥
10.PWM频率明显不对时绝不能接功率桥
```

---

## 4.推荐仓库文件结构

建议GitHub仓库结构如下：

```text
Your_Project/
├── Core/
│   ├── Inc/
│   │   ├── pvinv_openloop_tim8.h
│   │   └── ...
│   └── Src/
│       ├── pvinv_openloop_tim8.c
│       └── ...
├── STM32H563_xxx.ioc
├── MDK-ARM/
│   └── xxx.uvprojx
└── README_TIM8_OpenLoop_SPWM_Guide.md
```

本说明文档建议放在仓库根目录。

---

## 5.CubeMX配置总览

最终推荐TIM8配置如下：

```text
外设：TIM8

Clock Source          = Internal Clock
Channel1              = PWM Generation CH1 CH1N
Channel2              = PWM Generation CH2 CH2N
Channel3              = Disable
Channel4              = Disable

Counter Mode          = Center Aligned Mode 1
Prescaler             = 1
Counter Period        = 3124左右
Repetition Counter    = 1
Dithering             = Disable
Dead Time             = 非零
Break Input           = 初期Disable
Automatic Output      = Disable
TIM8 Update Interrupt = Enable
```

如果你的TIM8时钟约为250MHz，则：

```text
PSC=1
ARR=3124
中心对齐PWM频率约20kHz
RCR=1时，TIM8更新中断频率也约20kHz
```

---

## 6.CubeMX保姆级配置教程

### 6.1打开CubeMX工程

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

### 6.2确认芯片型号

确认芯片型号为：

```text
STM32H563RGT6
```

不要选错成STM32G431、STM32G474或其他STM32H563封装。

---

### 6.3配置系统时钟

进入：

```text
Clock Configuration
```

如果你的硬件使用25MHz外部时钟输入，当前推荐时钟结构可为：

```text
HSE = 25MHz
HSE Mode = Bypass或Crystal，取决于你的板子硬件
PLL Source = HSE
PLLM = 2
PLLN = 40
PLLR = 2
SYSCLK ≈ 250MHz
APB2 Prescaler = 1
TIM8_CLK ≈ 250MHz
```

注意：

```text
HSE到底选Bypass还是Crystal，不要凭空猜。
如果你的板子是外部有源时钟输入，通常选HSE Bypass。
如果你的板子是无源晶振，通常选Crystal/Ceramic Resonator。
必须以原理图为准。
```

---

### 6.4配置TIM8引脚

进入：

```text
Pinout & Configuration
```

在芯片引脚图中配置：

```text
PC6 -> TIM8_CH1
PA7 -> TIM8_CH1N
PC7 -> TIM8_CH2
PB0 -> TIM8_CH2N
```

操作方法：

```text
1.点击PC6，选择TIM8_CH1
2.点击PA7，选择TIM8_CH1N
3.点击PC7，选择TIM8_CH2
4.点击PB0，选择TIM8_CH2N
```

如果某个引脚不能选择对应TIM8功能，检查是否被其他外设占用。

---

### 6.5配置TIM8通道模式

左侧进入：

```text
Timers -> TIM8
```

配置：

```text
Clock Source = Internal Clock

Channel1 = PWM Generation CH1 CH1N
Channel2 = PWM Generation CH2 CH2N
Channel3 = Disable
Channel4 = Disable
```

重点：

```text
必须是CH1 CH1N和CH2 CH2N。
不能只开CH1和CH2。
```

因为全桥需要互补输出：

```text
CH1  对应A桥臂上管
CH1N 对应A桥臂下管
CH2  对应B桥臂上管
CH2N 对应B桥臂下管
```

---

### 6.6配置TIM8基础参数

进入TIM8的：

```text
Parameter Settings
```

推荐配置：

```text
Prescaler = 1
Counter Mode = Center Aligned Mode 1
Counter Period = 3124
Internal Clock Division = No Division
Repetition Counter = 1
Auto Reload Preload = Enable
```

不要使用：

```text
Counter Mode = Up
Counter Mode = Down
Edge Aligned PWM
```

最终代码会检查是否中心对齐。如果不是中心对齐，会报错。

---

### 6.7为什么Period推荐3124

中心对齐PWM频率公式：

```text
fPWM = TIM8_CLK / [2 * (PSC + 1) * (ARR + 1)]
```

若：

```text
TIM8_CLK ≈ 250MHz
PSC = 1
ARR = 3124
```

则：

```text
fPWM ≈ 250MHz / [2 * 2 * 3125]
     ≈ 20kHz
```

如果设置：

```text
ARR = 3000
```

则：

```text
fPWM ≈ 250MHz / [2 * 2 * 3001]
     ≈ 20.8kHz
```

所以：

```text
ARR=3000~3124都可以用于开环测试。
更推荐3124，因为更接近20kHz。
```

---

### 6.8为什么不要ARR=48000

如果：

```text
TIM8_CLK ≈ 250MHz
PSC = 1
ARR = 48000
```

则：

```text
fPWM ≈ 250MHz / [2 * 2 * 48001]
     ≈ 1.3kHz
```

这对逆变SPWM来说太低。

问题包括：

```text
1.PWM纹波大
2.滤波电感压力大
3.输出波形粗糙
4.可能进入可闻噪声范围
5.后续PR电流环带宽受限
6.50Hz周期内PWM点数太少
```

所以开环测试阶段不要保留`ARR=48000`。

---

### 6.9关闭Dithering

在TIM8参数中找到：

```text
Dithering
```

设置为：

```text
Disable
```

原因：

```text
1.Dithering会让ARR和实际周期关系变复杂
2.不利于初次上板排查频率
3.不利于示波器验证
4.当前阶段只需要稳定、清晰、可计算的PWM
```

最终代码默认拒绝Dithering。如果Dithering未关闭，`PVOL_CheckCubeMXConfig()`会返回：

```text
PVOL_STATUS_ERR_DITHERING_ENABLED
```

---

### 6.10配置PWM输出极性

TIM8 Channel1和Channel2建议：

```text
PWM Mode = PWM mode 1
Pulse = 0
OC Polarity = High
OCN Polarity = High
OC Fast Mode = Disable
OC Idle State = Reset
OCN Idle State = Reset
```

说明：

```text
Pulse初值为0可以。
真正占空比会在用户代码中由CCR1/CCR2动态更新。
```

---

### 6.11配置Dead Time

进入：

```text
TIM8 -> Break and Dead Time
```

设置：

```text
Dead Time = 非零
```

不要为0。

注意：

```text
Dead Time具体数值不能凭空保证。
它取决于驱动芯片、MOS/IGBT、栅极电阻、母线电压、开关速度等。
```

当前阶段最低要求：

```text
1.DeadTime不能为0
2.示波器能看到PC6/PA7之间有不重叠时间
3.示波器能看到PC7/PB0之间有不重叠时间
4.没有死区时绝不能接功率桥
```

---

### 6.12配置Break

初期开环主控板逻辑测试时，如果没有硬件过流比较器，可以配置：

```text
Break Input = Disable
Break2 Input = Disable
Automatic Output = Disable
```

后续真实功率级建议加入硬件保护：

```text
驱动故障或过流比较器 -> TIM8 BKIN
```

这样可以在硬件层快速关闭PWM，比软件保护更可靠。

---

### 6.13开启TIM8更新中断

进入：

```text
TIM8 -> NVIC Settings
```

勾选：

```text
TIM8 update interrupt
```

不同CubeMX版本可能显示为：

```text
TIM8 update global interrupt
TIM8_UP_IRQn
TIM8_UP_TIM13_IRQn
```

优先级建议：

```text
Preemption Priority = 1
Sub Priority = 0
```

说明：

```text
最终代码使用HAL_TIM_PeriodElapsedCallback方式。
所以必须由CubeMX生成TIM8更新中断入口。
```

---

### 6.14Project Manager配置

进入顶部：

```text
Project Manager
```

配置：

```text
Toolchain/IDE = MDK-ARM
```

然后点击：

```text
Generate Code
```

注意：

```text
不要手写CubeMX能生成的初始化。
例如MX_TIM8_Init、MX_GPIO_Init、SystemClock_Config等都由CubeMX生成。
```

---

## 7.Keil工程使用方法

### 7.1放置用户文件

把用户代码文件放到：

```text
Core/Inc/pvinv_openloop_tim8.h
Core/Src/pvinv_openloop_tim8.c
```

---

### 7.2把.c文件加入Keil工程

打开Keil。

在左侧Project窗口中，找到包含`main.c`的Group。

右键：

```text
Add Existing Files to Group
```

选择：

```text
Core/Src/pvinv_openloop_tim8.c
```

点击：

```text
Add
```

如果不做这一步，可能出现：

```text
Undefined symbol PVOL_Init
Undefined symbol PVOL_Start
Undefined symbol PVOL_CheckCubeMXConfig
```

---

## 8.main.c修改方法

只改`USER CODE`区域。

---

### 8.1添加头文件

找到：

```c
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */
```

改成：

```c
/* USER CODE BEGIN Includes */
#include "pvinv_openloop_tim8.h"
/* USER CODE END Includes */
```

---

### 8.2添加全局变量

找到：

```c
/* USER CODE BEGIN PV */

/* USER CODE END PV */
```

改成：

```c
/* USER CODE BEGIN PV */
static PVOL_Status_t g_pvol_status;
/* USER CODE END PV */
```

---

### 8.3删除旧固定PWM代码

如果main.c中有以下代码，必须删除：

```c
HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
TIM8->CCR1 = 10000;
TIM8->CCR2 = 10000;
```

原因：

```text
1.它只启动CH1和CH2
2.它没有启动CH1N和CH2N
3.它只是固定占空比PWM
4.它不是正弦SPWM
5.它不是单极性SPWM
```

---

### 8.4在USER CODE BEGIN 2中添加启动流程

找到：

```c
/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
```

加入：

```c
/* USER CODE BEGIN 2 */

g_pvol_status = PVOL_Init();
if (g_pvol_status != PVOL_STATUS_OK)
{
    Error_Handler();
}

g_pvol_status = PVOL_CheckCubeMXConfig();
if (g_pvol_status != PVOL_STATUS_OK)
{
    /*
     * 调试时可查看：
     * PVOL_StatusToString(g_pvol_status)
     *
     * 常见错误：
     * PVOL_STATUS_ERR_DITHERING_ENABLED：CubeMX里关闭TIM8 Dithering
     * PVOL_STATUS_ERR_ZERO_DEADTIME：CubeMX里设置非零DeadTime
     * PVOL_STATUS_ERR_CARRIER_TOO_LOW：ARR太大，例如不要保留48000
     * PVOL_STATUS_ERR_NOT_CENTER_ALIGNED：CubeMX里设置Center Aligned
     */
    Error_Handler();
}

PVOL_SetSineFrequencyHz(50.0f);
PVOL_SetTargetModulation(0.03f);

g_pvol_status = PVOL_Start();
if (g_pvol_status != PVOL_STATUS_OK)
{
    Error_Handler();
}

/* USER CODE END 2 */
```

---

### 8.5添加HAL定时器回调

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
    PVOL_TIM_PeriodElapsedCallback(htim);
}

/* USER CODE END 4 */
```

如果工程里已经有`HAL_TIM_PeriodElapsedCallback()`，不要重复定义。把下面这一句合并到已有函数中：

```c
PVOL_TIM_PeriodElapsedCallback(htim);
```

---

## 9.代码运行流程

上电后执行：

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

PVOL_Init()
PVOL_CheckCubeMXConfig()
PVOL_SetSineFrequencyHz(50.0f)
PVOL_SetTargetModulation(0.03f)
PVOL_Start()
```

然后进入主循环。

TIM8更新中断发生后：

```text
TIM8 Update IRQ
-> HAL_TIM_IRQHandler()
-> HAL_TIM_PeriodElapsedCallback()
-> PVOL_TIM_PeriodElapsedCallback()
-> PVOL_OnTim8UpdateIrq()
-> 更新theta
-> 更新D=M*sin(theta)
-> 更新CCR1
-> 更新CCR2
```

---

## 10.频率控制逻辑

### 10.1PWM载波频率

由CubeMX配置决定：

```text
fPWM = TIM8_CLK / [2 * (PSC + 1) * (ARR + 1)]
```

推荐：

```text
TIM8_CLK ≈ 250MHz
PSC = 1
ARR = 3124
fPWM ≈ 20kHz
```

---

### 10.2TIM8更新中断频率

代码估算：

```text
raw_update_hz = TIM8_CLK / [(PSC + 1) * (ARR + 1)]
update_irq_hz = raw_update_hz / (RCR + 1)
```

推荐：

```text
TIM8_CLK ≈ 250MHz
PSC = 1
ARR = 3124
RCR = 1
raw_update_hz ≈ 40kHz
update_irq_hz ≈ 20kHz
```

---

### 10.3正弦频率

代码中：

```c
PVOL_SetSineFrequencyHz(50.0f);
```

设置的是：

```text
正弦调制频率 = 50Hz
```

不是PWM载波频率。

相位步进：

```text
phase_step = 2*pi*sine_hz/update_irq_hz
```

若：

```text
sine_hz = 50Hz
update_irq_hz = 20kHz
```

则：

```text
每个正弦周期约400个更新点
```

---

## 11.示波器验证步骤

### 11.1测试前准备

不要连接：

```text
高压母线
驱动板
功率桥
MOS/IGBT
负载
```

只测主控板引脚：

```text
PC6
PA7
PC7
PB0
```

---

### 11.2测PC6

示波器测`PC6`。

应该看到：

```text
约20kHz PWM
占空比缓慢变化
```

---

### 11.3测PA7

示波器测`PA7`。

应该看到：

```text
PA7与PC6互补
```

---

### 11.4同时测PC6和PA7

必须确认：

```text
PC6和PA7不会同时为高
二者切换之间存在死区
```

没有死区时：

```text
立即停止测试
回CubeMX修改Dead Time
不能接功率桥
```

---

### 11.5同时测PC7和PB0

必须确认：

```text
PC7和PB0互补
二者之间有死区
```

---

### 11.6同时测PC6和PC7

必须确认：

```text
PC6占空比增大时，PC7占空比减小
PC6占空比减小时，PC7占空比增大
```

这说明单极性SPWM反向调制关系正确。

---

### 11.7测50Hz包络

示波器时间轴设置为：

```text
5ms/div或10ms/div
```

观察占空比包络变化。

目标：

```text
约20ms一个周期
即50Hz
```

---

## 12.常见错误排查

### 12.1进入Error_Handler：Dithering错误

现象：

```text
PVOL_STATUS_ERR_DITHERING_ENABLED
```

原因：

```text
TIM8 Dithering没有关闭
```

解决：

```text
CubeMX -> TIM8 -> Parameter Settings -> Dithering Disable
重新Generate Code
重新编译烧录
```

---

### 12.2进入Error_Handler：死区为0

现象：

```text
PVOL_STATUS_ERR_ZERO_DEADTIME
```

原因：

```text
Dead Time = 0
```

解决：

```text
CubeMX -> TIM8 -> Break and Dead Time -> Dead Time设为非零
重新Generate Code
重新编译烧录
```

---

### 12.3进入Error_Handler：载波频率太低

现象：

```text
PVOL_STATUS_ERR_CARRIER_TOO_LOW
```

常见原因：

```text
ARR太大，例如ARR=48000
```

解决：

```text
关闭Dithering
Period改成3000~3124
重新Generate Code
重新编译烧录
```

---

### 12.4进入Error_Handler：不是中心对齐

现象：

```text
PVOL_STATUS_ERR_NOT_CENTER_ALIGNED
```

解决：

```text
CubeMX -> TIM8 -> Counter Mode -> Center Aligned Mode 1
```

---

### 12.5只有PC6/PC7有波，PA7/PB0没有波

可能原因：

```text
1.CubeMX没有开启CH1N/CH2N
2.代码没有调用HAL_TIMEx_PWMN_Start
3.引脚复用不对
4.引脚被其他功能占用
```

解决：

```text
确认TIM8 Channel1 = PWM Generation CH1 CH1N
确认TIM8 Channel2 = PWM Generation CH2 CH2N
确认PA7 = TIM8_CH1N
确认PB0 = TIM8_CH2N
```

---

### 12.6PC6和PC7同向变化

问题：

```text
不是正确单极性SPWM
```

正确关系应为：

```text
PC6占空比增大时，PC7减小
PC6占空比减小时，PC7增大
```

检查代码里是否为：

```c
TIM8->CCR1 = center + center * D;
TIM8->CCR2 = center - center * D;
```

---

### 12.7PWM载波不是20kHz

检查：

```text
1.TIM8_CLK是否真的是250MHz
2.APB2分频是否为1
3.Prescaler是否为1
4.Period是否为3124左右
5.Dithering是否关闭
6.示波器测量是否正确
```

---

### 12.850Hz包络不对

检查：

```text
1.TIM8 Update interrupt是否开启
2.HAL_TIM_PeriodElapsedCallback是否正确调用PVOL_TIM_PeriodElapsedCallback
3.RCR是否与代码估算一致
4.update_irq_hz估算是否准确
5.PVOL_SetSineFrequencyHz是否设置为50.0f
```

---

## 13.烧录流程简述

在Keil中：

```text
1.打开.uvprojx工程
2.确认pvinv_openloop_tim8.c已加入工程
3.编译工程
4.Options for Target -> Debug -> ST-Link Debugger
5.确认Target Voltage不为0
6.Flash Download中确认有对应STM32H5xx Flash算法
7.点击Download烧录
```

烧录后：

```text
不要接功率桥
先测PC6/PA7/PC7/PB0
确认全部波形正确
```

---

## 14.最终验收标准

只有同时满足以下条件，才算开环TIM8单极性SPWM发波验证通过：

```text
1.PC6有PWM
2.PA7有PWM
3.PC7有PWM
4.PB0有PWM
5.PC6和PA7互补
6.PC7和PB0互补
7.PC6/PA7之间有死区
8.PC7/PB0之间有死区
9.PC6和PC7占空比反向变化
10.PWM载波约20kHz
11.正弦包络约50Hz
12.调制比0.03时波形变化幅度较小、没有异常跳变
```

未通过以上任意一项时，不要进入功率级测试。

---

## 15.后续扩展方向

开环发波验证通过后，再逐步加入：

```text
第一阶段：ADC DMA采样
第二阶段：Ud/Id/uREF/iF/uo原始数据验证
第三阶段：采样比例系数和符号校准
第四阶段：MPPT输入功率计算
第五阶段：电导增量法
第六阶段：uREF同步
第七阶段：PR电流闭环
第八阶段：保护逻辑
第九阶段：低压限流功率桥测试
第十阶段：逐步提高母线电压和负载
```

不要跳过开环波形验证直接做闭环。

---

## 16.一句话总结

这份开环代码的目标不是完成整个逆变器，而是先用TIM8严格验证：

```text
四路互补PWM正确
死区正确
载波频率正确
50Hz包络正确
A/B桥臂按单极性SPWM反向调制正确
```

只有这一步完全正确，后续的ADC、MPPT、电导增量法和PR电流闭环才有意义。
