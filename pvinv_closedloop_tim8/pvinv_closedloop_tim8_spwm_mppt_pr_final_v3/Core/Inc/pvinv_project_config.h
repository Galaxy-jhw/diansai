#ifndef __PVINV_PROJECT_CONFIG_H__
#define __PVINV_PROJECT_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32H563RGT6光伏逆变闭环控制最终版用户配置V3
 *
 * 严格边界：
 * 1.本代码包不生成CubeMX可以自动生成的初始化代码；
 * 2.TIM8、ADC1、ADC2、GPDMA、GPIO、NVIC、Clock、Dithering、DeadTime仍由CubeMX/.ioc配置；
 * 3.本代码包只做用户层闭环控制逻辑；
 * 4.发波使用TIM8四路互补PWM；
 * 5.调制方式使用单极性SPWM；
 * 6.MPPT使用电导增量法；
 * 7.电流闭环使用准PR控制器。
 */

/* ========================= TIM8 PWM与Dithering配置 ========================= */

#define PVINV_CFG_ALLOW_TIM8_DITHERING                  (1u)

/*
 * Dithering缩放系数：
 * 若普通等效Period约3000，Dithering后TIM8->ARR约48000，则scale=16。
 * 若示波器实测载波频率不对，优先修改该宏。
 */
#define PVINV_CFG_TIM8_DITHERING_SCALE                  (16.0f)

#define PVINV_CFG_ALLOW_ZERO_DEADTIME_LOGIC_ONLY        (0u)

#define PVINV_CFG_D_LIMIT                               (0.98f)
#define PVINV_CFG_MIN_ARR_REG                           (100u)

#define PVINV_CFG_CARRIER_MIN_HZ                        (18000.0f)
#define PVINV_CFG_CARRIER_MAX_HZ                        (25000.0f)

#define PVINV_CFG_UPDATE_MIN_HZ                         (15000.0f)
#define PVINV_CFG_UPDATE_MAX_HZ                         (50000.0f)

#define PVINV_CFG_USE_MANUAL_TIM8_CLK_HZ                (0u)
#define PVINV_CFG_MANUAL_TIM8_CLK_HZ                    (250000000.0f)

#define PVINV_CFG_USE_MANUAL_CONTROL_HZ                 (0u)
#define PVINV_CFG_MANUAL_CONTROL_HZ                     (20000.0f)

/* ========================= ADC DMA配置 ========================= */

#define PVINV_CFG_START_ADC_DMA_IN_USER_CODE            (1u)

/*
 * ADC1建议Regular Rank：
 * Rank1 -> Ud
 * Rank2 -> Id
 * Rank3 -> uREF
 */
#define PVINV_ADC1_DMA_LEN                              (3u)
#define PVINV_ADC1_IDX_UD                               (0u)
#define PVINV_ADC1_IDX_ID                               (1u)
#define PVINV_ADC1_IDX_UREF                             (2u)

/*
 * ADC2建议Regular Rank：
 * Rank1 -> iF
 * Rank2 -> uo
 */
#define PVINV_ADC2_DMA_LEN                              (2u)
#define PVINV_ADC2_IDX_IF                               (0u)
#define PVINV_ADC2_IDX_UO                               (1u)

#define PVINV_ADC_BITS                                  (12u)
#define PVINV_ADC_VREF                                  (3.300f)
#define PVINV_ADC_SINGLE_FULL_SCALE_CODE                (4095.0f)
#define PVINV_ADC_DIFF_FULL_SCALE_CODE                  (2048.0f)

/*
 * 差分通道默认按12bit二进制补码解释。
 * 如果实测不符，只修改ADC转换函数或该宏相关逻辑。
 */
#define PVINV_ADC_DIFF_TWOS_COMPLEMENT                  (1u)

/*
 * physical = sign * adc_voltage * scale + offset
 * 注意：以下默认值是占位值，必须通过实物标定修改。
 */
#define PVINV_UD_SIGN                                   (1.0f)
#define PVINV_ID_SIGN                                   (1.0f)
#define PVINV_IF_SIGN                                   (1.0f)
#define PVINV_UO_SIGN                                   (1.0f)

#define PVINV_UD_SCALE                                  (1.0f)
#define PVINV_ID_SCALE                                  (1.0f)
#define PVINV_IF_SCALE                                  (1.0f)
#define PVINV_UO_SCALE                                  (1.0f)

#define PVINV_UD_OFFSET                                 (0.0f)
#define PVINV_ID_OFFSET                                 (0.0f)
#define PVINV_IF_OFFSET                                 (0.0f)
#define PVINV_UO_OFFSET                                 (0.0f)

/*
 * uREF单端输入，默认1.65V偏置，幅值1.5V。
 * uref_norm = (uref_adc_v - bias) / amplitude。
 */
#define PVINV_UREF_BIAS_V                               (1.650f)
#define PVINV_UREF_AMPLITUDE_V                          (1.500f)
#define PVINV_UREF_SIGN                                 (1.0f)

/* 测量滤波系数。 */
#define PVINV_LPF_ALPHA_UD                              (0.02f)
#define PVINV_LPF_ALPHA_ID                              (0.02f)
#define PVINV_LPF_ALPHA_UREF                            (0.10f)
#define PVINV_LPF_ALPHA_IF                              (0.10f)
#define PVINV_LPF_ALPHA_UO                              (0.10f)

/*
 * ADC预热样本数。
 * 启动后先更新ADC和滤波值，不启用闭环和保护，避免滤波初值为0导致误欠压。
 */
#define PVINV_ADC_WARMUP_ISR_TICKS                      (200u)

/* ========================= 参考正弦来源 ========================= */

#define PVINV_CFG_USE_EXTERNAL_UREF                     (1u)
#define PVINV_CFG_INTERNAL_REF_HZ                       (50.0f)

/* ========================= MPPT电导增量法配置 ========================= */

#define PVINV_MPPT_ENABLE_DEFAULT                       (1u)

#define PVINV_MPPT_PERIOD_ISR_TICKS                     (200u)

#define PVINV_MPPT_VREF_INIT                            (18.0f)
#define PVINV_MPPT_VREF_MIN                             (5.0f)
#define PVINV_MPPT_VREF_MAX                             (80.0f)
#define PVINV_MPPT_VREF_STEP                            (0.05f)

#define PVINV_MPPT_DV_EPS                               (0.001f)
#define PVINV_MPPT_SLOPE_DEADBAND                       (0.0005f)

/*
 * PV电压外环PI：
 * error = Ud - Vmppt_ref
 * Ud高于目标：增加输出电流幅值；
 * Ud低于目标：降低输出电流幅值。
 */
#define PVINV_VPV_PI_KP                                 (0.02f)
#define PVINV_VPV_PI_KI                                 (1.00f)

#define PVINV_IAMP_MIN                                  (0.0f)
#define PVINV_IAMP_MAX                                  (2.0f)

/* ========================= PR电流环配置 ========================= */

#define PVINV_PR_KP                                     (0.10f)
#define PVINV_PR_KR                                     (20.0f)
#define PVINV_PR_F0_HZ                                  (50.0f)
#define PVINV_PR_WC_HZ                                  (5.0f)

#define PVINV_PR_OUT_MIN                                (-50.0f)
#define PVINV_PR_OUT_MAX                                (50.0f)

/*
 * 电压前馈幅值，默认0。
 * 如果需要根据uREF加入电压前馈，按硬件实际标定修改。
 */
#define PVINV_VFF_AMP                                   (0.0f)

/* ========================= 软启动与保护配置 ========================= */

#define PVINV_SOFTSTART_RAMP_PER_SEC                    (0.50f)

#define PVINV_PROT_UD_MIN                               (1.0f)
#define PVINV_PROT_UD_MAX                               (120.0f)
#define PVINV_PROT_ID_ABS_MAX                           (5.0f)
#define PVINV_PROT_IF_ABS_MAX                           (5.0f)
#define PVINV_PROT_UO_ABS_MAX                           (120.0f)

#ifdef __cplusplus
}
#endif

#endif
