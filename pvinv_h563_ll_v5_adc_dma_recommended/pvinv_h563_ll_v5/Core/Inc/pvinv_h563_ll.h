#ifndef PVINV_H563_LL_H
#define PVINV_H563_LL_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * STM32H563RGT6 光伏逆变控制模块 V5，推荐方案：ADC1 5路Regular + DMA Circular完成回调触发控制
 * =============================================================================
 * 代码定位：用户控制模块，不包含 CubeMX 可生成的初始化代码。
 * CubeMX 仍负责 SystemClock_Config / MX_GPIO_Init / MX_DMA_Init / MX_ADC1_Init / MX_TIM8_Init。
 *
 * PWM严格使用你.ioc中的TIM8两组互补PWM：
 *   PC6 / TIM8_CH1   -> A桥臂上管
 *   PA7 / TIM8_CH1N  -> A桥臂下管
 *   PC7 / TIM8_CH2   -> B桥臂上管
 *   PB0 / TIM8_CH2N  -> B桥臂下管
 *
 * .ioc中TIM8_CH3/CH3N、TIM8_CH4/CH4N虽然也配置为PWM，
 * 本模块初始化时会主动关闭它们，避免误驱动额外桥臂。
 *
 * 推荐ADC方案：
 *   TIM8 TRGO/TRGO2触发ADC1一次采5路Regular Rank；
 *   DMA Circular将5个raw搬到g_pvinv_adc_raw[0..4]；
 *   HAL_ADC_ConvCpltCallback中调用PVINV_LL_OnAdcDmaCompleteIrq()；
 *   该Hook内部调用PVINV_LL_ControlISR()。
 *
 * 重要：本代码不假装.ioc已完成5路ADC。必须你在CubeMX中补齐ADC1 5-rank + DMA circular。
 */

/* ========================= 0. 基本选择 ========================= */

#ifndef PVINV_USE_CMSIS_DSP
#define PVINV_USE_CMSIS_DSP                  0   /* 1: arm_sin_f32；0: sinf */
#endif

#define PVINV_TICK_EXTERNAL_ONLY             0u
#define PVINV_TICK_TIM8_UPDATE_HOOK          1u
#define PVINV_TICK_ADC_DMA_HOOK              2u

/* V5默认采用最推荐的ADC DMA完成Hook，避免TIM8 Update频率不确定带来的风险。 */
#ifndef PVINV_CONTROL_TICK_SOURCE
#define PVINV_CONTROL_TICK_SOURCE            PVINV_TICK_ADC_DMA_HOOK
#endif

#if !((PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_EXTERNAL_ONLY) || \
      (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_TIM8_UPDATE_HOOK) || \
      (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_ADC_DMA_HOOK))
#error "PVINV_CONTROL_TICK_SOURCE must be EXTERNAL_ONLY, TIM8_UPDATE_HOOK, or ADC_DMA_HOOK."
#endif

#ifndef PVINV_TEST_FIXED_IAMP_ENABLE
#define PVINV_TEST_FIXED_IAMP_ENABLE         0   /* 0:启用电导增量法MPPT；调试电流环时可临时改为1 */
#endif

/* ========================= 1. PWM与控制频率 ========================= */

#define PVINV_PWM_TIM                        TIM8

/* 必须实测：ADC DMA完成回调触发PVINV_LL_ControlISR()的实际频率应为20kHz。 */
#define PVINV_CTRL_FREQ_HZ                   20000.0f
#define PVINV_CTRL_TS                        (1.0f / PVINV_CTRL_FREQ_HZ)

/* 仅当你改用TIM8 Update Hook时才使用；推荐ADC DMA Hook时无效。 */
#define PVINV_TIM8_UPDATE_ISR_DIV            1u

/* testcore风格发波中心：TIM8->CCR1=center+center*m, CCR2=center-center*m */
#define PVINV_PWM_USE_FIXED_CENTER           1u
#define PVINV_PWM_FIXED_CENTER_CCR           6784u

#define PVINV_MOD_MIN                       -0.95f
#define PVINV_MOD_MAX                        0.95f
#define PVINV_MOD_SLEW_PER_ISR               0.012f

/* ========================= 2. ADC raw顺序 ========================= */

#define PVINV_ADC_CH_NUM                     5u
#define PVINV_ADC_IDX_UD                     0u
#define PVINV_ADC_IDX_ID                     1u
#define PVINV_ADC_IDX_UREF                   2u
#define PVINV_ADC_IDX_IFB                    3u
#define PVINV_ADC_IDX_UO                     4u

/*
 * 推荐CubeMX ADC1 Regular Rank顺序：
 *   raw[0] Ud   <- PA2 / ADC1_IN14
 *   raw[1] Id   <- PA3 / ADC1_IN15
 *   raw[2] uREF <- PA4 / ADC1_IN18
 *   raw[3] iF   <- PA5 / ADC1_IN19
 *   raw[4] uo   <- PA6 / ADC1_IN3
 */
extern volatile uint16_t g_pvinv_adc_raw[PVINV_ADC_CH_NUM];

/* ========================= 3. ADC标定参数：必须实测后修改 ========================= */

#define PVINV_UD_SCALE                       0.0200f
#define PVINV_UD_OFFSET                      0.0f

#define PVINV_ID_SCALE                       0.0050f
#define PVINV_ID_OFFSET                      0.0f
#define PVINV_ID_SIGN                        1.0f

#define PVINV_UREF_SCALE                     0.0016117f
#define PVINV_UREF_OFFSET                   -3.3000f

#define PVINV_IFB_SCALE                      0.0030f
#define PVINV_IFB_OFFSET                    -6.144f
#define PVINV_IFB_SIGN                       1.0f

#define PVINV_UO_SCALE                       0.0200f
#define PVINV_UO_OFFSET                      0.0f

/* ========================= 4. 题目与保护参数 ========================= */

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

/* ========================= 5. MPPT：电导增量法参数 ========================= */

#define PVINV_MPPT_DIV                       100u
#define PVINV_MPPT_BASE_STEP                 0.004f
#define PVINV_MPPT_STEP_GAIN_MAX             3.0f
#define PVINV_MPPT_DV_EPS                    0.04f
#define PVINV_MPPT_DI_EPS                    0.01f
#define PVINV_MPPT_G_EPS                     0.002f
#define PVINV_MPPT_PROBE_STEP                0.0015f

#define PVINV_IAMP_MIN                       0.00f
#define PVINV_IAMP_MAX                       0.50f  /* 调试初期建议0.50A；硬件验证后再提高 */
#define PVINV_IAMP_SLEW_PER_ISR              0.00008f
#define PVINV_TEST_FIXED_IAMP                0.20f
#define PVINV_SOFTSTART_IAMP                 0.12f

/* ========================= 6. 准PR电流环参数 ========================= */

#define PVINV_PR_KP                          0.045f
#define PVINV_PR_KR                          45.0f
#define PVINV_PR_WC_HZ                       6.0f
#define PVINV_PR_AW_GAIN                     0.20f
#define PVINV_PR_RESONANT_LIMIT              0.80f

#define PVINV_PHASE_COMP_RAD                 0.0f
#define PVINV_UO_FF_GAIN                     0.0f

/* ========================= 7. 编译期检查 ========================= */

#if (PVINV_ADC_CH_NUM != 5u)
#error "PVINV_ADC_CH_NUM must be 5 for Ud/Id/uREF/iF/uo."
#endif
#if (PVINV_MPPT_DIV == 0u)
#error "PVINV_MPPT_DIV must be > 0."
#endif
#if (PVINV_TIM8_UPDATE_ISR_DIV == 0u)
#error "PVINV_TIM8_UPDATE_ISR_DIV must be > 0."
#endif

/* ========================= 8. 状态机和状态数据 ========================= */

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
    uint8_t adc_samples_valid;

    PVINV_State_t state;

    uint32_t mppt_cnt;
    uint32_t fault_cnt;
    uint32_t isr_count;
    uint32_t tim8_update_div_count;

    uint8_t pwm_output_enabled;
} PVINV_Handle_t;

void PVINV_LL_Init(void);
void PVINV_LL_Start(void);
void PVINV_LL_Stop(void);
void PVINV_LL_ControlISR(void);

void PVINV_LL_SetRawSamples(uint16_t ud, uint16_t id, uint16_t uref, uint16_t ifb, uint16_t uo);
void PVINV_LL_MarkAdcSamplesValid(uint8_t valid);

#if (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_TIM8_UPDATE_HOOK)
void PVINV_LL_OnTim8UpdateIrq(void);
#endif

#if (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_ADC_DMA_HOOK)
void PVINV_LL_OnAdcDmaCompleteIrq(void);
#endif

void PVINV_LL_ClearPwmBreakFaultAfterCheck(void);
const PVINV_Handle_t *PVINV_LL_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* PVINV_H563_LL_H */
