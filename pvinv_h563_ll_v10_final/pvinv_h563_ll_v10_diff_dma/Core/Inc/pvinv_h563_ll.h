#ifndef PVINV_H563_LL_H
#define PVINV_H563_LL_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * STM32H563RGT6 光伏逆变控制模块 V10
 * =============================================================================
 * 版本定位：
 *   1. 用户控制模块，不包含 SystemClock/GPIO/ADC/TIM/GPDMA 等 CubeMX 可生成初始化代码。
 *   2. PWM 严格使用 .ioc 中的 TIM8 两组互补PWM：
 *        PC6 / TIM8_CH1   -> A桥臂上管
 *        PA7 / TIM8_CH1N  -> A桥臂下管
 *        PC7 / TIM8_CH2   -> B桥臂上管
 *        PB0 / TIM8_CH2N  -> B桥臂下管
 *   3. TIM8_CH3/CH3N、TIM8_CH4/CH4N 主动关闭，避免误驱动。
 *   4. 发波代码参考 testcore：
 *        TIM8->CCR1 = center + center * m;
 *        TIM8->CCR2 = center - center * m;
 *   5. ADC 使用“差分ADC raw + DMA直接上传 + 软件索引映射”思想；
 *      ADC Rank顺序可在CubeMX中按工程需要排列，代码通过PVINV_ADCx_IDX_INxx宏解释raw数组位置。
 *
 * 你最新指定的ADC差分输入：
 *   ADC1：IN10 Differential、IN12 Differential
 *   ADC2：IN1  Differential、IN18 Differential
 *
 * 重要说明：
 *   - DMA只负责把ADC转换结果上传到 raw 数组。
 *   - DMA结果不带“这是Ud/Id/uREF/iF”的标签。
 *   - 所以必须通过 PVINV_SRC_UD / PVINV_SRC_ID / PVINV_SRC_UREF / PVINV_SRC_IFB / PVINV_SRC_UO
 *     这些宏，把 raw 数组中的某个位置解释成具体物理量。
 */

/* ========================= 0. 基本选择 ========================= */

#ifndef PVINV_USE_CMSIS_DSP
#define PVINV_USE_CMSIS_DSP                  0   /* 1: arm_sin_f32；0: sinf */
#endif

#ifndef PVINV_TEST_FIXED_IAMP_ENABLE
#define PVINV_TEST_FIXED_IAMP_ENABLE         0   /* 调电流环时可临时改为1 */
#endif

/* ========================= 1. PWM与控制频率 ========================= */

#define PVINV_PWM_TIM                        TIM8

#define PVINV_CTRL_FREQ_HZ                   20000.0f
#define PVINV_CTRL_TS                        (1.0f / PVINV_CTRL_FREQ_HZ)

#define PVINV_PWM_USE_FIXED_CENTER           1u
#define PVINV_PWM_FIXED_CENTER_CCR           6784u

#define PVINV_MOD_MIN                       -0.95f
#define PVINV_MOD_MAX                        0.95f
#define PVINV_MOD_SLEW_PER_ISR               0.012f

/* ========================= 2. ADC差分DMA raw层 ========================= */

#define PVINV_ADC1_DIFF_CH_NUM               2u
#define PVINV_ADC2_DIFF_CH_NUM               2u

/*
 * ADC1和ADC2通过两个DMA分别上传。控制ISR只在两边都完成一轮后执行一次。
 * 如果某一边DMA连续到来而另一边长时间不来，说明双ADC触发/ DMA中断有问题，进入ADC_INVALID保护。
 */
#ifndef PVINV_ADC_PAIR_TIMEOUT_EVENTS
#define PVINV_ADC_PAIR_TIMEOUT_EVENTS        8u
#endif

#ifndef PVINV_ADC_PENDING_OVERWRITE_FAULT_LIMIT
#define PVINV_ADC_PENDING_OVERWRITE_FAULT_LIMIT 4u
#endif

#ifndef PVINV_ADC_ALLOW_DUPLICATE_SOURCES
#define PVINV_ADC_ALLOW_DUPLICATE_SOURCES    0u
#endif

#ifndef PVINV_REQUIRE_UO_SAMPLE
#define PVINV_REQUIRE_UO_SAMPLE              0u
#endif

/*
 * CubeMX中你指定的差分通道：
 *   ADC1：IN10 Differential、IN12 Differential
 *   ADC2：IN1  Differential、IN18 Differential
 *
 * 重要：DMA只会按Regular Rank顺序把结果写进raw数组，不会携带“通道名字”。
 * 所以V10保留“通道->数组下标”的宏，并新增源映射查重、ADC1/ADC2成对完成保护和调试原始码输出。只要CubeMX Rank顺序变了，改下面四个宏即可，
 * 控制算法主体不需要改。
 *
 * 默认推荐Rank：
 *   ADC1 Rank1 = ADC1_IN10 Differential -> g_pvinv_adc1_raw[0]
 *   ADC1 Rank2 = ADC1_IN12 Differential -> g_pvinv_adc1_raw[1]
 *   ADC2 Rank1 = ADC2_IN1  Differential -> g_pvinv_adc2_raw[0]
 *   ADC2 Rank2 = ADC2_IN18 Differential -> g_pvinv_adc2_raw[1]
 */
#ifndef PVINV_ADC1_IDX_IN10
#define PVINV_ADC1_IDX_IN10                  0u
#endif
#ifndef PVINV_ADC1_IDX_IN12
#define PVINV_ADC1_IDX_IN12                  1u
#endif
#ifndef PVINV_ADC2_IDX_IN1
#define PVINV_ADC2_IDX_IN1                   0u
#endif
#ifndef PVINV_ADC2_IDX_IN18
#define PVINV_ADC2_IDX_IN18                  1u
#endif

extern volatile uint16_t g_pvinv_adc1_raw[PVINV_ADC1_DIFF_CH_NUM];
extern volatile uint16_t g_pvinv_adc2_raw[PVINV_ADC2_DIFF_CH_NUM];

/*
 * g_pvinv_adc1_raw/g_pvinv_adc2_raw 是GPDMA直接写入的原始缓冲区。
 * ISR会先把它们锁存到模块内部快照；控制ISR只读锁存快照，
 * 避免DMA下一轮覆盖时读到半新半旧数据。
 */

/* ========================= 3. raw来源映射层 ========================= */

#define PVINV_ADC_SRC_NONE                   0u
#define PVINV_ADC_SRC_ADC1_IN10              1u
#define PVINV_ADC_SRC_ADC1_IN12              2u
#define PVINV_ADC_SRC_ADC2_IN1               3u
#define PVINV_ADC_SRC_ADC2_IN18              4u

/*
 * 默认映射只是为了让工程可编译、可跑通控制框架。
 * 你按硬件最终接线修改这里即可；控制算法主体不需要改。
 *
 * 如果某个量暂时没有采样通道，例如uo暂时不用，就设为PVINV_ADC_SRC_NONE。
 */
#ifndef PVINV_SRC_UD
#define PVINV_SRC_UD                         PVINV_ADC_SRC_ADC1_IN10
#endif
#ifndef PVINV_SRC_ID
#define PVINV_SRC_ID                         PVINV_ADC_SRC_ADC1_IN12
#endif
#ifndef PVINV_SRC_UREF
#define PVINV_SRC_UREF                       PVINV_ADC_SRC_ADC2_IN1
#endif
#ifndef PVINV_SRC_IFB
#define PVINV_SRC_IFB                        PVINV_ADC_SRC_ADC2_IN18
#endif
#ifndef PVINV_SRC_UO
#define PVINV_SRC_UO                         PVINV_ADC_SRC_NONE
#endif

/* ========================= 4. 差分ADC数据格式与标定参数 ========================= */

#define PVINV_ADC_VREF                       3.300f

/*
 * 差分ADC原始码格式。
 * 你必须上板验证：INP>INN时解释结果为正，INP<INN时为负，INP=INN时接近0。
 *
 * V10不再把差分ADC强行写死为int16_t满量程32768，而是把“ADC差分结果位宽”单独做成宏。
 * 这个宏必须与你CubeMX里的ADC分辨率一致。STM32H5常见可选分辨率包括12/14/16等，
 * 如果CubeMX选择14-bit，就保持14；如果选择12-bit或16-bit，就改成12或16。
 *
 * PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT：raw按PVINV_ADC_DIFF_RES_BITS位二补码右对齐解释。
 * PVINV_ADC_DIFF_FORMAT_OFFSET_BINARY：raw先减去PVINV_ADC_DIFF_ZERO_CODE再解释。
 */
#define PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT 0u
#define PVINV_ADC_DIFF_FORMAT_OFFSET_BINARY   1u

#ifndef PVINV_ADC_DIFF_FORMAT
#define PVINV_ADC_DIFF_FORMAT                PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT
#endif

#ifndef PVINV_ADC_DIFF_RES_BITS
#define PVINV_ADC_DIFF_RES_BITS              14u
#endif

#if (PVINV_ADC_DIFF_RES_BITS < 2u) || (PVINV_ADC_DIFF_RES_BITS > 16u)
#error "PVINV_ADC_DIFF_RES_BITS must be in [2,16]. Set it to CubeMX ADC resolution."
#endif

#ifndef PVINV_ADC_DIFF_ZERO_CODE
#define PVINV_ADC_DIFF_ZERO_CODE             (1u << (PVINV_ADC_DIFF_RES_BITS - 1u))
#endif

#define PVINV_ADC_DIFF_CODE_MASK             ((uint32_t)((1u << PVINV_ADC_DIFF_RES_BITS) - 1u))
#define PVINV_ADC_DIFF_SIGN_BIT              ((uint32_t)(1u << (PVINV_ADC_DIFF_RES_BITS - 1u)))
#define PVINV_ADC_DIFF_FULL_SCALE            ((float)(1u << (PVINV_ADC_DIFF_RES_BITS - 1u)))

/* Ud/uREF/uo：把差分ADC电压转换成实际物理量：physical = vdiff * GAIN + OFFSET */
#define PVINV_UD_GAIN                        20.0f
#define PVINV_UD_OFFSET                      0.0f
#define PVINV_UD_SIGN                        1.0f

#define PVINV_UREF_GAIN                      1.0f
#define PVINV_UREF_OFFSET                    0.0f
#define PVINV_UREF_SIGN                      1.0f

#define PVINV_UO_GAIN                        20.0f
#define PVINV_UO_OFFSET                      0.0f
#define PVINV_UO_SIGN                        1.0f

/*
 * Id/iF：微小电阻采样模型。
 * current = Vadc_diff / (Rshunt * TotalGain) + offset
 * TotalGain = INA240增益 × OPA后级等效差分增益。
 * 下面数值都是占位值，必须按你的采样电阻、INA240型号和OPA后级实测/计算修改。
 */
#define PVINV_ID_SHUNT_R_OHM                 0.010f
#define PVINV_ID_TOTAL_GAIN                  20.0f
#define PVINV_ID_OFFSET_A                    0.0f
#define PVINV_ID_SIGN                        1.0f

#define PVINV_IFB_SHUNT_R_OHM                0.010f
#define PVINV_IFB_TOTAL_GAIN                 20.0f
#define PVINV_IFB_OFFSET_A                   0.0f
#define PVINV_IFB_SIGN                       1.0f

/* ========================= 5. 题目与保护参数 ========================= */

#define PVINV_US_NOMINAL                     60.0f
#define PVINV_UD_MPP_DISPLAY                 30.0f

#define PVINV_UD_UNDER_TH                    25.0f
#define PVINV_UD_RECOVER_TH                  27.0f
#define PVINV_UD_OVER_TH                     65.0f
#define PVINV_UD_OVER_RECOVER_TH             62.0f

#define PVINV_IFB_OC_TH                      3.2f
#define PVINV_IFB_OC_RECOVER_TH              1.5f

#define PVINV_REF_FREQ_MIN                   45.0f
#define PVINV_REF_FREQ_MAX                   55.0f
#define PVINV_REF_FREQ_DEFAULT               50.0f
#define PVINV_UREF_ZC_HYST                   0.03f
#define PVINV_UREF_AMP_MIN                   0.15f
#define PVINV_REF_LOST_TIME_S                0.060f

/* ========================= 6. MPPT：电导增量法参数 ========================= */

#define PVINV_MPPT_DIV                       100u
#define PVINV_MPPT_BASE_STEP                 0.004f
#define PVINV_MPPT_STEP_GAIN_MAX             3.0f
#define PVINV_MPPT_DV_EPS                    0.04f
#define PVINV_MPPT_DI_EPS                    0.01f
#define PVINV_MPPT_G_EPS                     0.002f
#define PVINV_MPPT_PROBE_STEP                0.0015f

#define PVINV_IAMP_MIN                       0.00f
#define PVINV_IAMP_MAX                       0.50f
#define PVINV_IAMP_SLEW_PER_ISR              0.00008f
#define PVINV_TEST_FIXED_IAMP                0.20f
#define PVINV_SOFTSTART_IAMP                 0.12f

/* ========================= 7. 准PR电流环参数 ========================= */

#define PVINV_PR_KP                          0.045f
#define PVINV_PR_KR                          45.0f
#define PVINV_PR_WC_HZ                       6.0f
#define PVINV_PR_AW_GAIN                     0.20f
#define PVINV_PR_RESONANT_LIMIT              0.80f

#define PVINV_PHASE_COMP_RAD                 0.0f
#define PVINV_UO_FF_GAIN                     0.0f

/* ========================= 8. 编译期检查 ========================= */

#if (PVINV_ADC1_IDX_IN10 >= PVINV_ADC1_DIFF_CH_NUM)
#error "PVINV_ADC1_IDX_IN10 out of g_pvinv_adc1_raw[] range."
#endif
#if (PVINV_ADC1_IDX_IN12 >= PVINV_ADC1_DIFF_CH_NUM)
#error "PVINV_ADC1_IDX_IN12 out of g_pvinv_adc1_raw[] range."
#endif
#if (PVINV_ADC2_IDX_IN1 >= PVINV_ADC2_DIFF_CH_NUM)
#error "PVINV_ADC2_IDX_IN1 out of g_pvinv_adc2_raw[] range."
#endif
#if (PVINV_ADC2_IDX_IN18 >= PVINV_ADC2_DIFF_CH_NUM)
#error "PVINV_ADC2_IDX_IN18 out of g_pvinv_adc2_raw[] range."
#endif

#if (PVINV_MPPT_DIV == 0u)
#error "PVINV_MPPT_DIV must be > 0."
#endif

/* ========================= 9. 状态机和状态数据 ========================= */

typedef enum
{
    PVINV_STATE_STOP = 0,
    PVINV_STATE_WAIT_REF,
    PVINV_STATE_SOFT_START,
    PVINV_STATE_RUN,

    PVINV_STATE_FAULT_ADC_INVALID,
    PVINV_STATE_FAULT_UNDERVOLTAGE,
    PVINV_STATE_FAULT_OVERVOLTAGE,
    PVINV_STATE_FAULT_OVERCURRENT,
    PVINV_STATE_FAULT_REF_LOST,
    PVINV_STATE_FAULT_PWM_BREAK
} PVINV_State_t;

typedef struct
{
    float ud;
    float id;
    float uref;
    float ifb;
    float uo;

    float ud_f;
    float id_f;
    float uref_f;
    float ifb_f;
    float uo_f;

    float pv_power;

    float theta;
    float ref_freq;
    float ref_amp;
    uint8_t ref_locked;

    float iamp_target;
    float iamp_cmd;
    float i_ref;

    float current_err;
    float modulation;

    float mppt_v_avg;
    float mppt_i_avg;
    float mppt_g;

    uint32_t pwm_center_ccr;
    uint8_t pwm_center_fallback;

    uint8_t adc1_samples_valid;
    uint8_t adc2_samples_valid;
    uint8_t adc_samples_valid;
    uint8_t adc_source_config_error;
    uint8_t adc_source_duplicate_error;

    uint16_t raw_ud;
    uint16_t raw_id;
    uint16_t raw_uref;
    uint16_t raw_ifb;
    uint16_t raw_uo;

    int32_t code_ud;
    int32_t code_id;
    int32_t code_uref;
    int32_t code_ifb;
    int32_t code_uo;

    float vdiff_ud;
    float vdiff_id;
    float vdiff_uref;
    float vdiff_ifb;
    float vdiff_uo;

    PVINV_State_t state;

    uint32_t mppt_cnt;
    uint32_t fault_cnt;
    uint32_t isr_count;

    uint32_t adc1_dma_count;
    uint32_t adc2_dma_count;
    uint32_t control_from_adc_pair_count;
    uint32_t adc_pair_wait_count;
    uint32_t adc_pair_timeout_count;
    uint32_t adc1_pending_overwrite_count;
    uint32_t adc2_pending_overwrite_count;
    uint32_t adc_latched_pair_count;

    uint8_t pwm_output_enabled;
} PVINV_Handle_t;

void PVINV_LL_Init(void);
void PVINV_LL_Start(void);
void PVINV_LL_Stop(void);
void PVINV_LL_ControlISR(void);

/* 两个ADC DMA都完成后，本模块才执行一次控制ISR。 */
void PVINV_LL_OnAdc1DmaCompleteIrq(void);
void PVINV_LL_OnAdc2DmaCompleteIrq(void);
void PVINV_LL_OnBothAdcDmaCompleteIrq(void);

/* 手动喂数：用于无DMA时调试。参数顺序固定为ADC1_IN10、ADC1_IN12、ADC2_IN1、ADC2_IN18。 */
void PVINV_LL_SetRawDiffSamples(uint16_t adc1_in10, uint16_t adc1_in12,
                                uint16_t adc2_in1, uint16_t adc2_in18);

void PVINV_LL_MarkAdcSamplesValid(uint8_t valid);
void PVINV_LL_ClearPwmBreakFaultAfterCheck(void);
const PVINV_Handle_t *PVINV_LL_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* PVINV_H563_LL_H */
