#include "pvinv_h563_ll.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

volatile uint16_t g_pvinv_adc1_raw[PVINV_ADC1_RAW_NUM];
volatile uint16_t g_pvinv_adc2_raw[PVINV_ADC2_RAW_NUM];

static PVINV_Handle_t g_inv;

static uint16_t s_adc1_snap[PVINV_ADC1_RAW_NUM];
static uint16_t s_adc2_snap[PVINV_ADC2_RAW_NUM];
static volatile uint8_t s_adc1_ready;
static volatile uint8_t s_adc2_ready;
static volatile uint32_t s_adc1_unpaired;
static volatile uint32_t s_adc2_unpaired;

typedef struct
{
    float y;
    float alpha;
    uint8_t initialized;
} LPF1_t;

typedef struct
{
    float e1;
    float e2;
    float y1;
    float y2;
    float b0;
    float b2;
    float a1;
    float a2;
    float last_freq;
} PR_t;

static LPF1_t s_lpf_ud;
static LPF1_t s_lpf_id;
static LPF1_t s_lpf_uref;
static LPF1_t s_lpf_ifb;
static LPF1_t s_lpf_uo;
static PR_t s_pr;

static float s_uref_prev;
static float s_uref_abs_peak;
static uint32_t s_zc_ticks;
static uint32_t s_ref_lost_ticks;
static uint32_t s_mppt_div;
static float s_mppt_prev_v;
static float s_mppt_prev_i;

static void enter_fault(PVINV_State_t fault);

static inline float clamp_f(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float slew_f(float current, float target, float step)
{
    if (current < target - step) return current + step;
    if (current > target + step) return current - step;
    return target;
}

static float wrap_2pi(float x)
{
    const float two_pi = 2.0f * (float)M_PI;
    while (x >= two_pi) x -= two_pi;
    while (x < 0.0f) x += two_pi;
    return x;
}

static void lpf_init(LPF1_t *f, float alpha)
{
    f->y = 0.0f;
    f->alpha = clamp_f(alpha, 0.0f, 1.0f);
    f->initialized = 0u;
}

static float lpf_update(LPF1_t *f, float x)
{
    if (!f->initialized)
    {
        f->y = x;
        f->initialized = 1u;
        return f->y;
    }
    f->y += f->alpha * (x - f->y);
    return f->y;
}

static uint32_t adc_diff_mask(void)
{
    if (PVINV_ADC_DIFF_RES_BITS >= 16u) return 0xFFFFu;
    return (1u << PVINV_ADC_DIFF_RES_BITS) - 1u;
}

static int32_t adc_diff_raw_to_code(uint16_t raw)
{
    uint32_t mask = adc_diff_mask();
    uint32_t v = ((uint32_t)raw) & mask;

#if (PVINV_ADC_DIFF_FORMAT == PVINV_ADC_DIFF_FORMAT_TWOS_COMPLEMENT)
    if (PVINV_ADC_DIFF_RES_BITS >= 16u)
    {
        return (int16_t)v;
    }
    else
    {
        uint32_t sign = 1u << (PVINV_ADC_DIFF_RES_BITS - 1u);
        if ((v & sign) != 0u)
        {
            return (int32_t)v - (int32_t)(1u << PVINV_ADC_DIFF_RES_BITS);
        }
        return (int32_t)v;
    }
#else
    return (int32_t)v - (int32_t)PVINV_ADC_DIFF_ZERO_CODE;
#endif
}

static float adc_diff_code_to_voltage(int32_t code)
{
    float fs = (float)(1u << (PVINV_ADC_DIFF_RES_BITS - 1u));
    return ((float)code) * PVINV_ADC_VREF / fs;
}

static float adc_single_raw_to_voltage(uint16_t raw)
{
    uint32_t max_code;
    if (PVINV_ADC_SINGLE_RES_BITS >= 16u)
    {
        max_code = 65535u;
    }
    else
    {
        max_code = (1u << PVINV_ADC_SINGLE_RES_BITS) - 1u;
    }
    return ((float)raw) * PVINV_ADC_VREF / (float)max_code;
}

static uint16_t source_raw(uint32_t src)
{
    switch (src)
    {
        case PVINV_ADC_SRC_ADC1_IN10_DIFF:  return s_adc1_snap[PVINV_ADC1_IDX_IN10];
        case PVINV_ADC_SRC_ADC1_IN12_DIFF:  return s_adc1_snap[PVINV_ADC1_IDX_IN12];
        case PVINV_ADC_SRC_ADC1_IN4_SINGLE: return s_adc1_snap[PVINV_ADC1_IDX_IN4];
        case PVINV_ADC_SRC_ADC2_IN1_DIFF:   return s_adc2_snap[PVINV_ADC2_IDX_IN1];
        case PVINV_ADC_SRC_ADC2_IN18_DIFF:  return s_adc2_snap[PVINV_ADC2_IDX_IN18];
        default: return 0u;
    }
}

static float source_to_diff_voltage(uint32_t src, uint16_t raw, int32_t *code_out)
{
    (void)src;
    int32_t code = adc_diff_raw_to_code(raw);
    if (code_out != NULL) *code_out = code;
    return adc_diff_code_to_voltage(code);
}

static float adc_to_ud(uint16_t raw)
{
    int32_t code;
    float vdiff = source_to_diff_voltage(PVINV_SRC_UD, raw, &code);
    g_inv.code_ud = code;
    g_inv.vdiff_ud = vdiff;
    return vdiff * PVINV_UD_GAIN + PVINV_UD_OFFSET;
}

static float adc_to_uo(uint16_t raw)
{
    if (PVINV_SRC_UO == PVINV_ADC_SRC_NONE) return 0.0f;
    int32_t code;
    float vdiff = source_to_diff_voltage(PVINV_SRC_UO, raw, &code);
    g_inv.code_uo = code;
    g_inv.vdiff_uo = vdiff;
    return vdiff * PVINV_UO_GAIN + PVINV_UO_OFFSET;
}

static float adc_to_id(uint16_t raw)
{
    int32_t code;
    float vdiff = source_to_diff_voltage(PVINV_SRC_ID, raw, &code);
    g_inv.code_id = code;
    g_inv.vdiff_id = vdiff;
    float denom = PVINV_ID_SHUNT_R_OHM * PVINV_ID_TOTAL_GAIN;
    if (denom < 1.0e-9f)
    {
        enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
        return 0.0f;
    }
    float i = vdiff / denom;
    return PVINV_ID_SIGN * (i + PVINV_ID_OFFSET_A);
}

static float adc_to_ifb(uint16_t raw)
{
    int32_t code;
    float vdiff = source_to_diff_voltage(PVINV_SRC_IFB, raw, &code);
    g_inv.code_ifb = code;
    g_inv.vdiff_ifb = vdiff;
    float denom = PVINV_IFB_SHUNT_R_OHM * PVINV_IFB_TOTAL_GAIN;
    if (denom < 1.0e-9f)
    {
        enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
        return 0.0f;
    }
    float i = vdiff / denom;
    return PVINV_IFB_SIGN * (i + PVINV_IFB_OFFSET_A);
}

static float adc_to_uref(uint16_t raw)
{
    float v = adc_single_raw_to_voltage(raw);
    g_inv.v_uref_adc = v;
    return PVINV_UREF_SIGN * ((v - PVINV_UREF_ZERO_VOLT) * PVINV_UREF_GAIN + PVINV_UREF_OFFSET);
}

static void pr_reset(void)
{
    memset(&s_pr, 0, sizeof(s_pr));
    s_pr.last_freq = 0.0f;
}

static void pr_update_coeff(float freq_hz)
{
    float f = clamp_f(freq_hz, PVINV_REF_FREQ_MIN_HZ, PVINV_REF_FREQ_MAX_HZ);
    if (fabsf(f - s_pr.last_freq) < 0.05f) return;

    float T = PVINV_CTRL_TS;
    float w0 = 2.0f * (float)M_PI * f;
    float wc = 2.0f * (float)M_PI * PVINV_PR_WC_HZ;

    float A0 = 4.0f + 4.0f * wc * T + w0 * w0 * T * T;
    float A1 = 2.0f * w0 * w0 * T * T - 8.0f;
    float A2 = 4.0f - 4.0f * wc * T + w0 * w0 * T * T;
    float B0 = 4.0f * PVINV_PR_KR * wc * T;
    float B2 = -B0;

    s_pr.a1 = A1 / A0;
    s_pr.a2 = A2 / A0;
    s_pr.b0 = B0 / A0;
    s_pr.b2 = B2 / A0;
    s_pr.last_freq = f;
}

static float pr_update(float e, float freq_hz)
{
    pr_update_coeff(freq_hz);

    float yr = -s_pr.a1 * s_pr.y1 - s_pr.a2 * s_pr.y2 + s_pr.b0 * e + s_pr.b2 * s_pr.e2;

    s_pr.e2 = s_pr.e1;
    s_pr.e1 = e;
    s_pr.y2 = s_pr.y1;
    s_pr.y1 = yr;

    return PVINV_PR_KP * e + yr;
}

static uint32_t pwm_get_center(void)
{
    uint32_t arr = PVINV_PWM_TIM->ARR;
    uint32_t center = arr / 2u;
    g_inv.pwm_center_fallback = 0u;

#if (PVINV_PWM_USE_FIXED_CENTER != 0u)
    if (arr > (PVINV_PWM_FIXED_CENTER_CCR + 4u))
    {
        center = PVINV_PWM_FIXED_CENTER_CCR;
    }
    else
    {
        center = arr / 2u;
        g_inv.pwm_center_fallback = 1u;
    }
#endif

    if (center < 4u) center = 4u;
    g_inv.pwm_center_ccr = center;
    return center;
}

static void pwm_set_zero(void)
{
    uint32_t center = pwm_get_center();
    PVINV_PWM_TIM->CCR1 = center;
    PVINV_PWM_TIM->CCR2 = center;
}

static void pwm_disable_output(void)
{
#ifdef TIM_BDTR_MOE
    PVINV_PWM_TIM->BDTR &= ~TIM_BDTR_MOE;
#endif
    pwm_set_zero();
}

static void pwm_enable_output(void)
{
    /* Only CH1/CH1N and CH2/CH2N are enabled. CH3/CH4 remain disabled. */
#ifdef TIM_CCER_CC1E
    PVINV_PWM_TIM->CCER |= TIM_CCER_CC1E;
#endif
#ifdef TIM_CCER_CC1NE
    PVINV_PWM_TIM->CCER |= TIM_CCER_CC1NE;
#endif
#ifdef TIM_CCER_CC2E
    PVINV_PWM_TIM->CCER |= TIM_CCER_CC2E;
#endif
#ifdef TIM_CCER_CC2NE
    PVINV_PWM_TIM->CCER |= TIM_CCER_CC2NE;
#endif
#ifdef TIM_CCER_CC3E
    PVINV_PWM_TIM->CCER &= ~TIM_CCER_CC3E;
#endif
#ifdef TIM_CCER_CC3NE
    PVINV_PWM_TIM->CCER &= ~TIM_CCER_CC3NE;
#endif
#ifdef TIM_CCER_CC4E
    PVINV_PWM_TIM->CCER &= ~TIM_CCER_CC4E;
#endif
#ifdef TIM_CCER_CC4NE
    PVINV_PWM_TIM->CCER &= ~TIM_CCER_CC4NE;
#endif
#ifdef TIM_BDTR_MOE
    PVINV_PWM_TIM->BDTR |= TIM_BDTR_MOE;
#endif
}

static uint8_t pwm_moe_is_enabled(void)
{
#ifdef TIM_BDTR_MOE
    return ((PVINV_PWM_TIM->BDTR & TIM_BDTR_MOE) != 0u) ? 1u : 0u;
#else
    return 1u;
#endif
}

static void spwm_unipolar_update(float m)
{
    m = clamp_f(m, PVINV_MOD_MIN, PVINV_MOD_MAX);

    uint32_t center = pwm_get_center();
    uint32_t arr = PVINV_PWM_TIM->ARR;

    float ccr1_f = (float)center + (float)center * m;
    float ccr2_f = (float)center - (float)center * m;

    ccr1_f = clamp_f(ccr1_f, 2.0f, (float)arr - 2.0f);
    ccr2_f = clamp_f(ccr2_f, 2.0f, (float)arr - 2.0f);

    PVINV_PWM_TIM->CCR1 = (uint32_t)(ccr1_f + 0.5f);
    PVINV_PWM_TIM->CCR2 = (uint32_t)(ccr2_f + 0.5f);
}

static void adc_pair_reset_internal(void)
{
    s_adc1_ready = 0u;
    s_adc2_ready = 0u;
    s_adc1_unpaired = 0u;
    s_adc2_unpaired = 0u;
    g_inv.adc_pair_reset_count++;
}

static void ref_sync_reset_internal(void)
{
    g_inv.ref_locked = 0u;
    g_inv.theta = 0.0f;
    g_inv.ref_freq = PVINV_REF_FREQ_DEFAULT_HZ;
    g_inv.ref_amp = 0.0f;
    s_uref_prev = 0.0f;
    s_uref_abs_peak = 0.0f;
    s_zc_ticks = 0u;
    s_ref_lost_ticks = 0u;
}

static void enter_fault(PVINV_State_t fault)
{
    g_inv.last_fault = fault;
    if (fault != PVINV_STATE_STOP)
    {
        g_inv.fault_count++;
    }
    g_inv.state = fault;
    g_inv.iamp_target = 0.0f;
    g_inv.iamp_cmd = 0.0f;
    g_inv.i_ref = 0.0f;
    g_inv.modulation = 0.0f;
    pwm_disable_output();
    if (fault == PVINV_STATE_FAULT_ADC_INVALID)
    {
        g_inv.adc_samples_valid = 0u;
        adc_pair_reset_internal();
    }
}

static void update_ref_sync(float uref)
{
    float abs_u = fabsf(uref);
    if (abs_u > s_uref_abs_peak) s_uref_abs_peak = abs_u;
    s_zc_ticks++;

    uint8_t rising_zc = 0u;
    if ((s_uref_prev < -PVINV_UREF_ZC_HYST) && (uref >= PVINV_UREF_ZC_HYST))
    {
        rising_zc = 1u;
    }

    if (rising_zc)
    {
        if (s_zc_ticks > 0u)
        {
            float freq = PVINV_CTRL_FREQ_HZ / (float)s_zc_ticks;
            if ((freq >= PVINV_REF_FREQ_MIN_HZ) && (freq <= PVINV_REF_FREQ_MAX_HZ) && (s_uref_abs_peak >= PVINV_UREF_AMP_MIN))
            {
                g_inv.ref_freq = freq;
                g_inv.ref_locked = 1u;
                s_ref_lost_ticks = 0u;
                g_inv.ref_amp = s_uref_abs_peak;
                g_inv.theta = 0.0f;
            }
        }
        s_zc_ticks = 0u;
        s_uref_abs_peak = 0.0f;
    }
    else
    {
        if (g_inv.ref_locked)
        {
            g_inv.theta = wrap_2pi(g_inv.theta + 2.0f * (float)M_PI * g_inv.ref_freq * PVINV_CTRL_TS);
        }
        s_ref_lost_ticks++;
        if ((float)s_ref_lost_ticks * PVINV_CTRL_TS > PVINV_REF_LOST_TIME_S)
        {
            g_inv.ref_locked = 0u;
        }
    }

    s_uref_prev = uref;
}

static uint8_t protection_ok(void)
{
    if (!g_inv.adc_samples_valid)
    {
        enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
        return 0u;
    }

    /* In WAIT_REF, undervoltage is not latched as a fault; the controller simply waits. */
    if ((g_inv.state == PVINV_STATE_RUN || g_inv.state == PVINV_STATE_SOFT_START) && (g_inv.ud_f < PVINV_UD_UNDER_TH))
    {
        enter_fault(PVINV_STATE_FAULT_UNDER_VOLT);
        return 0u;
    }

    if (g_inv.ud_f > PVINV_UD_OVER_TH)
    {
        enter_fault(PVINV_STATE_FAULT_OVER_VOLT);
        return 0u;
    }

    if (fabsf(g_inv.ifb_f) > PVINV_IFB_OC_TH)
    {
        enter_fault(PVINV_STATE_FAULT_OVER_CURRENT);
        return 0u;
    }

    if ((g_inv.state == PVINV_STATE_RUN || g_inv.state == PVINV_STATE_SOFT_START) && !g_inv.ref_locked)
    {
        enter_fault(PVINV_STATE_FAULT_REF_LOST);
        return 0u;
    }

    if ((g_inv.state == PVINV_STATE_RUN || g_inv.state == PVINV_STATE_SOFT_START) && !pwm_moe_is_enabled())
    {
        enter_fault(PVINV_STATE_FAULT_PWM_BREAK);
        return 0u;
    }

    return 1u;
}

static void mppt_update(void)
{
#if (PVINV_TEST_FIXED_IAMP_ENABLE != 0u)
    g_inv.iamp_target = clamp_f(PVINV_TEST_FIXED_IAMP_A, PVINV_IAMP_MIN, PVINV_IAMP_MAX);
    return;
#else
    s_mppt_div++;
    if (s_mppt_div < PVINV_MPPT_DIV) return;
    s_mppt_div = 0u;

    float v = g_inv.ud_f;
    float i = g_inv.id_f;
    g_inv.mppt_v_avg = v;
    g_inv.mppt_i_avg = i;

    if (v < 1.0f)
    {
        g_inv.iamp_target = PVINV_IAMP_MIN;
        s_mppt_prev_v = v;
        s_mppt_prev_i = i;
        return;
    }

    float dv = v - s_mppt_prev_v;
    float di = i - s_mppt_prev_i;
    float g = 0.0f;

    if (fabsf(dv) > PVINV_MPPT_DV_EPS)
    {
        g = di / dv + i / v;
    }
    else if (fabsf(di) > PVINV_MPPT_DI_EPS)
    {
        g = (di > 0.0f) ? 1.0f : -1.0f;
    }
    else
    {
        g = 0.0f;
    }

    g_inv.mppt_g = g;

    if (g > PVINV_MPPT_G_EPS)
    {
        g_inv.iamp_target -= PVINV_MPPT_BASE_STEP;
    }
    else if (g < -PVINV_MPPT_G_EPS)
    {
        g_inv.iamp_target += PVINV_MPPT_BASE_STEP;
    }

    g_inv.iamp_target = clamp_f(g_inv.iamp_target, PVINV_IAMP_MIN, PVINV_IAMP_MAX);
    s_mppt_prev_v = v;
    s_mppt_prev_i = i;
#endif
}

static void read_and_filter_adc_snapshot(void)
{
    g_inv.raw_ud = source_raw(PVINV_SRC_UD);
    g_inv.raw_id = source_raw(PVINV_SRC_ID);
    g_inv.raw_uref = source_raw(PVINV_SRC_UREF);
    g_inv.raw_ifb = source_raw(PVINV_SRC_IFB);
    g_inv.raw_uo = source_raw(PVINV_SRC_UO);

    g_inv.ud = adc_to_ud(g_inv.raw_ud);
    g_inv.id = adc_to_id(g_inv.raw_id);
    g_inv.uref = adc_to_uref(g_inv.raw_uref);
    g_inv.ifb = adc_to_ifb(g_inv.raw_ifb);
    g_inv.uo = adc_to_uo(g_inv.raw_uo);

    g_inv.ud_f = lpf_update(&s_lpf_ud, g_inv.ud);
    g_inv.id_f = lpf_update(&s_lpf_id, g_inv.id);
    g_inv.uref_f = lpf_update(&s_lpf_uref, g_inv.uref);
    g_inv.ifb_f = lpf_update(&s_lpf_ifb, g_inv.ifb);
    g_inv.uo_f = lpf_update(&s_lpf_uo, g_inv.uo);
    g_inv.pv_power = g_inv.ud_f * g_inv.id_f;
}

void PVINV_LL_Init(void)
{
    memset(&g_inv, 0, sizeof(g_inv));
    memset((void *)g_pvinv_adc1_raw, 0, sizeof(g_pvinv_adc1_raw));
    memset((void *)g_pvinv_adc2_raw, 0, sizeof(g_pvinv_adc2_raw));
    memset(s_adc1_snap, 0, sizeof(s_adc1_snap));
    memset(s_adc2_snap, 0, sizeof(s_adc2_snap));

    lpf_init(&s_lpf_ud, 0.03f);
    lpf_init(&s_lpf_id, 0.03f);
    lpf_init(&s_lpf_uref, 0.08f);
    lpf_init(&s_lpf_ifb, 0.08f);
    lpf_init(&s_lpf_uo, 0.03f);
    pr_reset();

    g_inv.state = PVINV_STATE_STOP;
    g_inv.last_fault = PVINV_STATE_STOP;
    g_inv.iamp_target = 0.0f;
    g_inv.iamp_cmd = 0.0f;
    g_inv.modulation = 0.0f;

    s_mppt_div = 0u;
    s_mppt_prev_v = 0.0f;
    s_mppt_prev_i = 0.0f;
    ref_sync_reset_internal();
    adc_pair_reset_internal();

    pwm_disable_output();
}

void PVINV_LL_Start(void)
{
    if (g_inv.state == PVINV_STATE_STOP)
    {
        pwm_disable_output();
        ref_sync_reset_internal();
        adc_pair_reset_internal();
        g_inv.iamp_target = 0.0f;
        g_inv.iamp_cmd = 0.0f;
        g_inv.modulation = 0.0f;
        g_inv.state = PVINV_STATE_WAIT_REF;
    }
}

void PVINV_LL_Stop(void)
{
    enter_fault(PVINV_STATE_STOP);
    adc_pair_reset_internal();
    ref_sync_reset_internal();
    g_inv.adc_samples_valid = 0u;
    g_inv.state = PVINV_STATE_STOP;
}

void PVINV_LL_ControlISR(void)
{
    g_inv.isr_count++;

    if (g_inv.state == PVINV_STATE_STOP)
    {
        pwm_disable_output();
        return;
    }

    if (!g_inv.adc_samples_valid)
    {
        enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
        return;
    }

    read_and_filter_adc_snapshot();
    if (g_inv.state == PVINV_STATE_FAULT_ADC_INVALID)
    {
        return;
    }
    update_ref_sync(g_inv.uref_f);

    if (!protection_ok()) return;

    switch (g_inv.state)
    {
        case PVINV_STATE_WAIT_REF:
            pwm_disable_output();
            if (g_inv.ref_locked && g_inv.ud_f > PVINV_UD_RECOVER_TH)
            {
                g_inv.iamp_target = PVINV_SOFTSTART_IAMP_A;
                g_inv.iamp_cmd = 0.0f;
                pr_reset();
                pwm_set_zero();
                pwm_enable_output();
                g_inv.state = PVINV_STATE_SOFT_START;
            }
            break;

        case PVINV_STATE_SOFT_START:
            g_inv.iamp_cmd = slew_f(g_inv.iamp_cmd, g_inv.iamp_target, PVINV_IAMP_SLEW_PER_ISR);
            if (fabsf(g_inv.iamp_cmd - g_inv.iamp_target) < 0.005f)
            {
                g_inv.state = PVINV_STATE_RUN;
            }
            break;

        case PVINV_STATE_RUN:
            mppt_update();
            g_inv.iamp_cmd = slew_f(g_inv.iamp_cmd, g_inv.iamp_target, PVINV_IAMP_SLEW_PER_ISR);
            break;

        default:
            pwm_disable_output();
            return;
    }

    float theta = wrap_2pi(g_inv.theta + PVINV_PHASE_COMP_RAD);
    g_inv.i_ref = g_inv.iamp_cmd * sinf(theta);
    g_inv.current_err = g_inv.i_ref - g_inv.ifb_f;
    g_inv.pr_out = pr_update(g_inv.current_err, g_inv.ref_freq);

    float target_mod = clamp_f(g_inv.pr_out, PVINV_MOD_MIN, PVINV_MOD_MAX);
    g_inv.modulation = slew_f(g_inv.modulation, target_mod, PVINV_MOD_SLEW_PER_ISR);
    spwm_unipolar_update(g_inv.modulation);
}

void PVINV_LL_OnAdc1DmaCompleteIrq(void)
{
    for (uint32_t i = 0; i < PVINV_ADC1_RAW_NUM; ++i)
    {
        s_adc1_snap[i] = g_pvinv_adc1_raw[i];
    }
    g_inv.adc1_done_count++;

    if (s_adc1_ready && !s_adc2_ready)
    {
        s_adc1_unpaired++;
        if (s_adc1_unpaired > PVINV_ADC_PAIR_MAX_UNPAIRED)
        {
            g_inv.adc_pair_miss_count++;
            enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
            return;
        }
    }
    s_adc1_ready = 1u;

    if (s_adc1_ready && s_adc2_ready)
    {
        s_adc1_ready = 0u;
        s_adc2_ready = 0u;
        s_adc1_unpaired = 0u;
        s_adc2_unpaired = 0u;
        g_inv.adc_pair_count++;
        g_inv.adc_samples_valid = 1u;
        PVINV_LL_ControlISR();
    }
}

void PVINV_LL_OnAdc2DmaCompleteIrq(void)
{
    for (uint32_t i = 0; i < PVINV_ADC2_RAW_NUM; ++i)
    {
        s_adc2_snap[i] = g_pvinv_adc2_raw[i];
    }
    g_inv.adc2_done_count++;

    if (s_adc2_ready && !s_adc1_ready)
    {
        s_adc2_unpaired++;
        if (s_adc2_unpaired > PVINV_ADC_PAIR_MAX_UNPAIRED)
        {
            g_inv.adc_pair_miss_count++;
            enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
            return;
        }
    }
    s_adc2_ready = 1u;

    if (s_adc1_ready && s_adc2_ready)
    {
        s_adc1_ready = 0u;
        s_adc2_ready = 0u;
        s_adc1_unpaired = 0u;
        s_adc2_unpaired = 0u;
        g_inv.adc_pair_count++;
        g_inv.adc_samples_valid = 1u;
        PVINV_LL_ControlISR();
    }
}

void PVINV_LL_OnAdc1DmaErrorIrq(void)
{
#if (PVINV_ENABLE_DMA_ERROR_FAULT != 0u)
    g_inv.adc_dma_error_count++;
    enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
#endif
}

void PVINV_LL_OnAdc2DmaErrorIrq(void)
{
#if (PVINV_ENABLE_DMA_ERROR_FAULT != 0u)
    g_inv.adc_dma_error_count++;
    enter_fault(PVINV_STATE_FAULT_ADC_INVALID);
#endif
}

void PVINV_LL_ResetAdcPairSync(void)
{
    adc_pair_reset_internal();
    g_inv.adc_samples_valid = 0u;
}

void PVINV_LL_ClearPwmBreakFaultAfterCheck(void)
{
    if (g_inv.state == PVINV_STATE_FAULT_PWM_BREAK)
    {
#ifdef TIM_SR_BIF
        PVINV_PWM_TIM->SR &= ~TIM_SR_BIF;
#endif
#ifdef TIM_SR_B2IF
        PVINV_PWM_TIM->SR &= ~TIM_SR_B2IF;
#endif
        pwm_disable_output();
        g_inv.state = PVINV_STATE_WAIT_REF;
    }
}

void PVINV_LL_ResetFaultToWaitRef(void)
{
    if ((g_inv.state == PVINV_STATE_FAULT_ADC_INVALID) ||
        (g_inv.state == PVINV_STATE_FAULT_REF_LOST) ||
        (g_inv.state == PVINV_STATE_FAULT_UNDER_VOLT) ||
        (g_inv.state == PVINV_STATE_FAULT_OVER_VOLT) ||
        (g_inv.state == PVINV_STATE_FAULT_OVER_CURRENT))
    {
        pwm_disable_output();
        g_inv.iamp_target = 0.0f;
        g_inv.iamp_cmd = 0.0f;
        g_inv.modulation = 0.0f;
        pr_reset();
        ref_sync_reset_internal();
        adc_pair_reset_internal();
        g_inv.adc_samples_valid = 0u;
        g_inv.state = PVINV_STATE_WAIT_REF;
    }
}

const PVINV_Handle_t *PVINV_LL_GetHandle(void)
{
    return &g_inv;
}
