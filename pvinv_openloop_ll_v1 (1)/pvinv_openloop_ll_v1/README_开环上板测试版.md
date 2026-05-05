# PVINV开环单极性SPWM上板测试版

## 这个版本做什么

这个版本只做开环发波，用于初期上板验证：

- TIM8互补PWM是否出来；
- CH1/CH1N、CH2/CH2N是否正确；
- 死区是否正确；
- 驱动芯片输入是否正常；
- testcore风格CCR更新是否正常。

它不做：

- MPPT；
- ADC采样；
- DMA采样；
- PR电流环；
- 并网闭环；
- 软件过流闭环。

## 默认TIM8引脚

按照你的.ioc和前面方案：

- TIM8_CH1  → PC6；
- TIM8_CH1N → PA7；
- TIM8_CH2  → PC7；
- TIM8_CH2N → PB0。

TIM8_CH3、TIM8_CH4不使用，代码会主动关闭。

## CubeMX配置要求

TIM8：

- Clock Source = Internal Clock；
- Channel1 = PWM Generation CH1 CH1N；
- Channel2 = PWM Generation CH2 CH2N；
- Channel3 = Disable；
- Channel4 = Disable；
- Counter Mode = Center Aligned；
- PWM Mode = PWM mode 1；
- Preload Enable；
- Dead Time必须配置；
- 初期如果没有硬件比较器，Break Input先不要开启，避免BKIN悬空导致无输出。

## 关键公式

```c
TIM8->CCR1 = center + center * m;
TIM8->CCR2 = center - center * m;
```

其中`m = modulation_cmd * sin(theta)`。

## 上板建议

1. 不接功率桥，先只看MCU或驱动输入端PWM。
2. 调制度先设`0.02~0.03`。
3. 确认CH1/CH1N互补且有死区。
4. 确认CH2/CH2N互补且有死区。
5. 确认CCR1和CCR2反向变化。
6. 确认TIM8 Update调用频率等于`PVOL_CTRL_FREQ_HZ`。
7. 再低压、限流、隔离测试驱动链路。

## 如果TIM8 ARR不匹配testcore的6784

代码默认优先使用`center=6784`，但如果`TIM8->ARR < 2*6784`，会自动回退到`ARR/2`，并设置：

```c
p->pwm_center_fallback = 1;
```

如果你当前TIM8频率配置导致ARR较小，这是正常保护行为。
