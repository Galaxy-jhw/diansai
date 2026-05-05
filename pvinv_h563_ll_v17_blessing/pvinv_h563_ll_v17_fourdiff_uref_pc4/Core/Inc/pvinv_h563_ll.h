#ifndef PVINV_H563_LL_H
#define PVINV_H563_LL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * PV Inverter Control Module, STM32H563, LL/register style user module. V17 hardened.
 *
 * V17 default resource mapping based on the user's latest requirement:
 *   - Four differential measurement channels are reserved for the four physical quantities:
 *       ADC1_IN10 differential -> Ud  input DC voltage, default
 *       ADC1_IN12 differential -> Id  input DC current, shunt-resistor model, default
 *       ADC2_IN1  differential -> iF  output current feedback, shunt-resistor model, default
 *       ADC2_IN18 differential -> uo  output voltage, default
 *   - uREF is NOT one of the four differential channels.
 *       PC4 / ADC1_IN4 single-ended -> uREF from signal generator, default
 *
 * CubeMX must generate SystemClock/GPIO/ADC/TIM8/GPDMA initialization.
 * This module intentionally does NOT contain CubeMX-generatable initialization code.
 */

/* ========================= User-adjustable core timing ========================= */
#ifndef PVINV_CTRL_FREQ_HZ
#define PVINV_CTRL_FREQ_HZ                 20000.0f
#endif

#define PVINV_CTRL_TS                      (1.0f / PVINV_CTRL_FREQ_HZ)

/* ========================= ADC DMA raw buffer sizes ========================= */
#define PVINV_ADC1_RAW_NUM                 3u   /* IN10 diff, IN12 diff, IN4 single */
#define PVINV_ADC2_RAW_NUM                 2u   /* IN1 diff, IN18 diff */

extern volatile uint16_t g_pvinv_adc1_raw[PVINV_ADC1_RAW_NUM];
extern volatile uint16_t g_pvinv_adc2_raw[PVINV_ADC2_RAW_NUM];

/*
 * Rank-to-buffer-index mapping.
 * If CubeMX ADC Regular Rank order changes, only modify these macros.
 * Default recommended CubeMX rank order:
 *   ADC1 Rank1 = ADC1_IN10 Differential -> g_pvinv_adc1_raw[0]
 *   ADC1 Rank2 = ADC1_IN12 Differential -> g_pvinv_adc1_raw[1]
 *   ADC1 Rank3 = ADC1_IN4  Single-ended  -> g_pvinv_adc1_raw[2]
 *   ADC2 Rank1 = ADC2_IN1  Differential -> g_pvinv_adc2_raw[0]
 *   ADC2 Rank2 = ADC2_IN18 Differential -> g_pvinv_adc2_raw[1]
 */
#ifndef PVINV_ADC1_IDX_IN10
#define PVINV_ADC1_IDX_IN10                0u
#endif
#ifndef PVINV_ADC1_IDX_IN12
#define PVINV_ADC1_IDX_IN12                1u
#endif
#ifndef PVINV_ADC1_IDX_IN4
#define PVINV_ADC1_IDX_IN4                 2u
#endif
#ifndef PVINV_ADC2_IDX_IN1
#define PVINV_ADC2_IDX_IN1                 0u
#endif
#ifndef PVINV_ADC2_IDX_IN18
#define PVINV_ADC2_IDX_IN18                1u
#endif

/* Logical ADC sources. */
#define PVINV_ADC_SRC_NONE                 0u
#define PVINV_ADC_SRC_ADC1_IN10_DIFF       1u
#define PVINV_ADC_SRC_ADC1_IN12_DIFF       2u
#define PVINV_ADC_SRC_ADC1_IN4_SINGLE      3u
#define PVINV_ADC_SRC_ADC2_IN1_DIFF        4u
#define PVINV_ADC_SRC_ADC2_IN18_DIFF       5u

/* Physical quantity source mapping. Change these if final wiring changes. */
#ifndef PVINV_SRC_UD
#define PVINV_SRC_UD                       PVINV_ADC_SRC_ADC1_IN10_DIFF
#endif
#ifndef PVINV_SRC_ID
#define PVINV_SRC_ID                       PVINV_ADC_SRC_ADC1_IN12_DIFF
#endif
#ifndef PVINV_SRC_IFB
#define PVINV_SRC_IFB                      PVINV_ADC_SRC_ADC2_IN1_DIFF
#endif
#ifndef PVINV_SRC_UO
#define PVINV_SRC_UO                       PVINV_ADC_SRC_ADC2_IN18_DIFF
#endif
#ifndef PVINV_SRC_UREF
#define PVINV_SRC_UREF                     PVINV_ADC_SRC_ADC1_IN4_SINGLE
#endif

/* ========================= ADC conversion format ========================= */
#ifndef PVINV_ADC_VREF
#define PVINV_ADC_VREF                     3.300f
#endif

/* Set these exactly the same as CubeMX ADC resolution. */
#ifndef PVINV_ADC_DIFF_RES_BITS
#define PVINV_ADC_DIFF_RES_BITS            14u
#endif
#ifndef PVINV_ADC_SINGLE_RES_BITS
#define PVINV_ADC_SINGLE_RES_BITS          14u
#endif

#define PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT  0u
#define PVINV_ADC_DIFF_FORMAT_OFFSET_BINARY    1u

#ifndef PVINV_ADC_DIFF_FORMAT
#define PVINV_ADC_DIFF_FORMAT              PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT
#endif

#ifndef PVINV_ADC_DIFF_ZERO_CODE
#define PVINV_ADC_DIFF_ZERO_CODE           (1u << (PVINV_ADC_DIFF_RES_BITS - 1u))
#endif


/* Compile-time configuration checks. */
#if (PVINV_ADC1_IDX_IN10 >= PVINV_ADC1_RAW_NUM) || (PVINV_ADC1_IDX_IN12 >= PVINV_ADC1_RAW_NUM) || (PVINV_ADC1_IDX_IN4 >= PVINV_ADC1_RAW_NUM)
#error "ADC1 index mapping exceeds PVINV_ADC1_RAW_NUM"
#endif
#if (PVINV_ADC2_IDX_IN1 >= PVINV_ADC2_RAW_NUM) || (PVINV_ADC2_IDX_IN18 >= PVINV_ADC2_RAW_NUM)
#error "ADC2 index mapping exceeds PVINV_ADC2_RAW_NUM"
#endif
#if (PVINV_SRC_UD == PVINV_ADC_SRC_NONE) || (PVINV_SRC_ID == PVINV_ADC_SRC_NONE) || (PVINV_SRC_IFB == PVINV_ADC_SRC_NONE) || (PVINV_SRC_UREF == PVINV_ADC_SRC_NONE)
#error "Ud, Id, iF and uREF must be mapped to valid ADC sources"
#endif
#if (PVINV_SRC_UD == PVINV_SRC_ID) || (PVINV_SRC_UD == PVINV_SRC_IFB) || (PVINV_SRC_UD == PVINV_SRC_UREF) || (PVINV_SRC_ID == PVINV_SRC_IFB) || (PVINV_SRC_ID == PVINV_SRC_UREF) || (PVINV_SRC_IFB == PVINV_SRC_UREF)
#error "Ud, Id, iF and uREF must not share the same ADC source"
#endif
#if (PVINV_SRC_UO == PVINV_ADC_SRC_NONE)
#error "V17 requires uo to be mapped to the fourth differential ADC source"
#endif
#if (PVINV_SRC_UO != PVINV_ADC_SRC_NONE) && ((PVINV_SRC_UO == PVINV_SRC_UD) || (PVINV_SRC_UO == PVINV_SRC_ID) || (PVINV_SRC_UO == PVINV_SRC_IFB) || (PVINV_SRC_UO == PVINV_SRC_UREF))
#error "uo ADC source must not duplicate another physical source"
#endif
#if (PVINV_SRC_UREF != PVINV_ADC_SRC_ADC1_IN4_SINGLE)
#error "V17 expects uREF to use PC4 / ADC1_IN4 single-ended input"
#endif
#if (PVINV_SRC_UD == PVINV_ADC_SRC_ADC1_IN4_SINGLE) || (PVINV_SRC_ID == PVINV_ADC_SRC_ADC1_IN4_SINGLE) || (PVINV_SRC_IFB == PVINV_ADC_SRC_ADC1_IN4_SINGLE) || (PVINV_SRC_UO == PVINV_ADC_SRC_ADC1_IN4_SINGLE)
#error "The four measured physical quantities Ud/Id/iF/uo must use differential ADC sources, not ADC1_IN4 single-ended"
#endif
#if (PVINV_ADC_DIFF_RES_BITS < 8u) || (PVINV_ADC_DIFF_RES_BITS > 16u)
#error "PVINV_ADC_DIFF_RES_BITS must be in 8..16"
#endif
#if (PVINV_ADC_SINGLE_RES_BITS < 8u) || (PVINV_ADC_SINGLE_RES_BITS > 16u)
#error "PVINV_ADC_SINGLE_RES_BITS must be in 8..16"
#endif

/* ========================= Signal scaling / calibration ========================= */
/* Differential voltage measurement channels. Physical = vdiff * GAIN + OFFSET. */
#ifndef PVINV_UD_GAIN
#define PVINV_UD_GAIN                      20.0f
#endif
#ifndef PVINV_UD_OFFSET
#define PVINV_UD_OFFSET                    0.0f
#endif
#ifndef PVINV_UO_GAIN
#define PVINV_UO_GAIN                      20.0f
#endif
#ifndef PVINV_UO_OFFSET
#define PVINV_UO_OFFSET                    0.0f
#endif

/* Shunt-current channels. I = Vdiff / (Rshunt * TotalGain), then sign/offset. */
#ifndef PVINV_ID_SHUNT_R_OHM
#define PVINV_ID_SHUNT_R_OHM               0.010f
#endif
#ifndef PVINV_ID_TOTAL_GAIN
#define PVINV_ID_TOTAL_GAIN                20.0f
#endif
#ifndef PVINV_ID_SIGN
#define PVINV_ID_SIGN                      1.0f
#endif
#ifndef PVINV_ID_OFFSET_A
#define PVINV_ID_OFFSET_A                  0.0f
#endif

#ifndef PVINV_IFB_SHUNT_R_OHM
#define PVINV_IFB_SHUNT_R_OHM              0.010f
#endif
#ifndef PVINV_IFB_TOTAL_GAIN
#define PVINV_IFB_TOTAL_GAIN               20.0f
#endif
#ifndef PVINV_IFB_SIGN
#define PVINV_IFB_SIGN                     1.0f
#endif
#ifndef PVINV_IFB_OFFSET_A
#define PVINV_IFB_OFFSET_A                 0.0f
#endif

/* uREF from signal generator, single-ended ADC, must be biased into 0~3.3V. */
#ifndef PVINV_UREF_ZERO_VOLT
#define PVINV_UREF_ZERO_VOLT               1.650f
#endif
#ifndef PVINV_UREF_GAIN
#define PVINV_UREF_GAIN                    1.0f
#endif
#ifndef PVINV_UREF_OFFSET
#define PVINV_UREF_OFFSET                  0.0f
#endif
#ifndef PVINV_UREF_SIGN
#define PVINV_UREF_SIGN                    1.0f
#endif

/* ========================= uREF synchronization ========================= */
#ifndef PVINV_REF_FREQ_DEFAULT_HZ
#define PVINV_REF_FREQ_DEFAULT_HZ          50.0f
#endif
#ifndef PVINV_REF_FREQ_MIN_HZ
#define PVINV_REF_FREQ_MIN_HZ              40.0f
#endif
#ifndef PVINV_REF_FREQ_MAX_HZ
#define PVINV_REF_FREQ_MAX_HZ              65.0f
#endif
#ifndef PVINV_UREF_ZC_HYST
#define PVINV_UREF_ZC_HYST                 0.03f
#endif
#ifndef PVINV_UREF_AMP_MIN
#define PVINV_UREF_AMP_MIN                 0.10f
#endif
#ifndef PVINV_REF_LOST_TIME_S
#define PVINV_REF_LOST_TIME_S              0.080f
#endif
#ifndef PVINV_PHASE_COMP_RAD
#define PVINV_PHASE_COMP_RAD               0.0f
#endif

/* ========================= Protection thresholds ========================= */
#ifndef PVINV_UD_UNDER_TH
#define PVINV_UD_UNDER_TH                  25.0f
#endif
#ifndef PVINV_UD_RECOVER_TH
#define PVINV_UD_RECOVER_TH                27.0f
#endif
#ifndef PVINV_UD_OVER_TH
#define PVINV_UD_OVER_TH                   80.0f
#endif
#ifndef PVINV_UD_OVER_RECOVER_TH
#define PVINV_UD_OVER_RECOVER_TH           75.0f
#endif
#ifndef PVINV_IFB_OC_TH
#define PVINV_IFB_OC_TH                    3.0f
#endif
#ifndef PVINV_IFB_OC_RECOVER_TH
#define PVINV_IFB_OC_RECOVER_TH            1.0f
#endif

/* ========================= Current command and MPPT ========================= */
#ifndef PVINV_IAMP_MIN
#define PVINV_IAMP_MIN                     0.0f
#endif
#ifndef PVINV_IAMP_MAX
#define PVINV_IAMP_MAX                     3.0f
#endif
#ifndef PVINV_IAMP_SLEW_PER_ISR
#define PVINV_IAMP_SLEW_PER_ISR            0.00008f
#endif
#ifndef PVINV_SOFTSTART_IAMP_A
#define PVINV_SOFTSTART_IAMP_A             0.15f
#endif

#ifndef PVINV_MPPT_DIV
#define PVINV_MPPT_DIV                     100u
#endif
#ifndef PVINV_MPPT_BASE_STEP
#define PVINV_MPPT_BASE_STEP               0.004f
#endif
#ifndef PVINV_MPPT_G_EPS
#define PVINV_MPPT_G_EPS                   0.002f
#endif
#ifndef PVINV_MPPT_DV_EPS
#define PVINV_MPPT_DV_EPS                  0.05f
#endif
#ifndef PVINV_MPPT_DI_EPS
#define PVINV_MPPT_DI_EPS                  0.01f
#endif
#ifndef PVINV_TEST_FIXED_IAMP_ENABLE
#define PVINV_TEST_FIXED_IAMP_ENABLE       0u
#endif
#ifndef PVINV_TEST_FIXED_IAMP_A
#define PVINV_TEST_FIXED_IAMP_A            0.20f
#endif

/* ========================= PR current loop ========================= */
#ifndef PVINV_PR_KP
#define PVINV_PR_KP                        0.045f
#endif
#ifndef PVINV_PR_KR
#define PVINV_PR_KR                        45.0f
#endif
#ifndef PVINV_PR_WC_HZ
#define PVINV_PR_WC_HZ                     6.0f
#endif

/* ========================= PWM / TIM8 ========================= */
#ifndef PVINV_PWM_TIM
#define PVINV_PWM_TIM                      TIM8
#endif

#ifndef PVINV_PWM_USE_FIXED_CENTER
#define PVINV_PWM_USE_FIXED_CENTER         1u
#endif
#ifndef PVINV_PWM_FIXED_CENTER_CCR
#define PVINV_PWM_FIXED_CENTER_CCR         6784u
#endif
#ifndef PVINV_MOD_MIN
#define PVINV_MOD_MIN                      (-0.95f)
#endif
#ifndef PVINV_MOD_MAX
#define PVINV_MOD_MAX                      (0.95f)
#endif
#ifndef PVINV_MOD_SLEW_PER_ISR
#define PVINV_MOD_SLEW_PER_ISR             0.012f
#endif

/* ADC pairing protection. */
#ifndef PVINV_ADC_PAIR_MAX_UNPAIRED
#define PVINV_ADC_PAIR_MAX_UNPAIRED        4u
#endif

/* If a DMA error IRQ is reported through PVINV_LL_OnAdcXDmaErrorIrq(), the module
 * enters ADC_INVALID fault immediately. Keep this enabled for power-stage safety. */
#ifndef PVINV_ENABLE_DMA_ERROR_FAULT
#define PVINV_ENABLE_DMA_ERROR_FAULT       1u
#endif

/* ========================= Types ========================= */
typedef enum
{
    PVINV_STATE_STOP = 0,
    PVINV_STATE_WAIT_REF,
    PVINV_STATE_SOFT_START,
    PVINV_STATE_RUN,
    PVINV_STATE_FAULT_ADC_INVALID,
    PVINV_STATE_FAULT_REF_LOST,
    PVINV_STATE_FAULT_UNDER_VOLT,
    PVINV_STATE_FAULT_OVER_VOLT,
    PVINV_STATE_FAULT_OVER_CURRENT,
    PVINV_STATE_FAULT_PWM_BREAK
} PVINV_State_t;

typedef struct
{
    PVINV_State_t state;

    uint32_t isr_count;
    uint32_t adc1_done_count;
    uint32_t adc2_done_count;
    uint32_t adc_pair_count;
    uint32_t adc_pair_miss_count;
    uint32_t adc_pair_reset_count;
    uint32_t adc_dma_error_count;
    uint32_t fault_count;
    PVINV_State_t last_fault;
    uint8_t  adc_samples_valid;
    uint8_t  pwm_center_fallback;

    uint16_t raw_ud;
    uint16_t raw_id;
    uint16_t raw_uref;
    uint16_t raw_ifb;
    uint16_t raw_uo;

    int32_t code_ud;
    int32_t code_id;
    int32_t code_ifb;
    int32_t code_uo;

    float vdiff_ud;
    float vdiff_id;
    float vdiff_ifb;
    float vdiff_uo;
    float v_uref_adc;

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

    float ref_freq;
    float ref_amp;
    float theta;
    uint8_t ref_locked;

    float iamp_target;
    float iamp_cmd;
    float i_ref;
    float current_err;
    float modulation;
    float pr_out;

    float mppt_v_avg;
    float mppt_i_avg;
    float mppt_g;

    uint32_t pwm_center_ccr;
} PVINV_Handle_t;

/* ========================= Public APIs ========================= */
void PVINV_LL_Init(void);
void PVINV_LL_Start(void);
void PVINV_LL_Stop(void);
void PVINV_LL_ControlISR(void);

void PVINV_LL_OnAdc1DmaCompleteIrq(void);
void PVINV_LL_OnAdc2DmaCompleteIrq(void);
void PVINV_LL_OnAdc1DmaErrorIrq(void);
void PVINV_LL_OnAdc2DmaErrorIrq(void);
void PVINV_LL_ResetAdcPairSync(void);

void PVINV_LL_ClearPwmBreakFaultAfterCheck(void);
void PVINV_LL_ResetFaultToWaitRef(void);
const PVINV_Handle_t *PVINV_LL_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* PVINV_H563_LL_H */
