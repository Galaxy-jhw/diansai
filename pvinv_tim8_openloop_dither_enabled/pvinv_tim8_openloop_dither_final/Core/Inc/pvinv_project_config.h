#ifndef __PVINV_PROJECT_CONFIG_H__
#define __PVINV_PROJECT_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 用户工程配置文件
 *
 * 重要原则：
 * 1.本文件只定义用户层宏，不生成CubeMX可以生成的初始化代码。
 * 2.TIM8、GPIO、NVIC、Clock等仍由CubeMX/.ioc配置。
 * 3.本代码包允许保留TIM8 Dithering，用于提高PWM发波精度。
 */

/* 允许TIM8 Dithering。
 * 你的当前要求是保留Dithering以提高发波精度，所以默认允许。
 */
#define PVINV_CFG_ALLOW_TIM8_DITHERING                 (1u)

/*
 * STM32H5 TIM Dithering等效缩放系数。
 *
 * 你之前工程中出现：
 *   普通等效周期约3000
 *   Dithering后ARR寄存器值约48000
 * 因此默认按48000/3000=16处理。
 *
 * 如果你后续确认CubeMX/HAL生成规则不同，只需要改这个宏。
 */
#define PVINV_CFG_TIM8_DITHERING_SCALE                 (16.0f)

/*
 * 是否允许DeadTime=0。
 * 0：不允许，安全；
 * 1：仅允许主控板空载测逻辑波形，绝不能接驱动板/功率桥。
 */
#define PVINV_CFG_ALLOW_ZERO_DEADTIME_LOGIC_ONLY       (0u)

/*
 * 推荐载波检查范围。
 * 当前目标是20kHz附近，允许一定误差。
 */
#define PVINV_CFG_CARRIER_MIN_HZ                       (18000.0f)
#define PVINV_CFG_CARRIER_MAX_HZ                       (25000.0f)

/*
 * 推荐TIM8更新中断频率范围。
 * 该值用于计算50Hz正弦相位步进。
 */
#define PVINV_CFG_UPDATE_MIN_HZ                        (15000.0f)
#define PVINV_CFG_UPDATE_MAX_HZ                        (50000.0f)

/*
 * 开环默认参数。
 */
#define PVINV_CFG_DEFAULT_SINE_HZ                      (50.0f)
#define PVINV_CFG_DEFAULT_MODULATION                   (0.03f)

/*
 * 调制比软启动速度：每秒调制比变化量。
 * 0.20表示从0爬升到0.03大约需要0.15s。
 */
#define PVINV_CFG_MOD_RAMP_PER_SEC                     (0.20f)

/*
 * 单极性SPWM调制量限幅，避免CCR贴边。
 */
#define PVINV_CFG_D_LIMIT                              (0.98f)

/*
 * ARR寄存器最小值检查。
 */
#define PVINV_CFG_MIN_ARR_REG                          (100u)

#ifdef __cplusplus
}
#endif

#endif
