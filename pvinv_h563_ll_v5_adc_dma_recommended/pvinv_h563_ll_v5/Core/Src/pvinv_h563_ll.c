#include "pvinv_h563_ll.h"

#if PVINV_USE_CMSIS_DSP
#include "arm_math.h"
#define PVINV_SIN_F32(x) arm_sin_f32((x))
#else
#include <math.h>
#define PVINV_SIN_F32(x) sinf((x))
#endif

#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

volatile uint16_t g_pvinv_adc_raw[PVINV_ADC_CH_NUM];

/* ============================================================
 * 1. 基础工具
 * ============================================================ */

static inline float clamp_f(float x, float min_v, float max_v)
{
    if (x > max_v) return max_v;
    if (x < min_v) return min_v;
    return x;
}

static inline float abs_f(float x)
{
    return (x >= 0.0f) ? x : -x;
}

static inline float slew_f(float target, float now, float step)
{
    if (target > now + step) return now + step;
    if (target < now - step) return now - step;
    return target;
}

static inline float wrap_2pi(float x)
{
    while (x >= 2.0f * M_PI) x -= 2.0f * M_PI;
    while (x < 0.0f) x += 2.0f * M_PI;
    return x;
}

/* ============================================================
 * 2. 一阶低通滤波
 * ============================================================ */

typedef struct
{
    float alpha;
    float y;
} LPF1_t;

static void LPF1_Init(LPF1_t *f, float alpha, float init)
{
    f->alpha = alpha;
    f->y = init;
}

static float LPF1_Update(LPF1_t *f, float x)
{
    f->y += f->alpha * (x - f->y);
    return f->y;
}

/* ============================================================
 * 3. uREF 上升过零同步器
 * ============================================================ */

typedef struct
{
    float theta;
    float freq_hz;
    float amp_est;
    float u_prev;
    float sample_counter;
    uint8_t armed;
    uint8_t locked;
    uint32_t lost_counter;
} RefSync_t;

static void RefSync_Init(RefSync_t *s)
{
    s->theta = 0.0f;
    s->freq_hz = PVINV_REF_FREQ_DEFAULT;
    s->amp_est = 0.0f;
    s->u_prev = 0.0f;
    s->sample_counter = 0.0f;
    s->armed = 0u;
    s->locked = 0u;
    s->lost_counter = 0u;
}

static void RefSync_Reset(RefSync_t *s)
{
    RefSync_Init(s);
}

static float RefSync_Update(RefSync_t *s, float u)
{
    const uint32_t lost_limit = (uint32_t)(PVINV_REF_LOST_TIME_S * PVINV_CTRL_FREQ_HZ);

    s->amp_est += 0.002f * (abs_f(u) - s->amp_est);

    if (s->amp_est < PVINV_UREF_AMP_MIN)
    {
        s->locked = 0u;
        s->armed = 0u;
        s->sample_counter = 0.0f;
        s->lost_counter = 0u;
        s->u_prev = u;
        s->freq_hz = PVINV_REF_FREQ_DEFAULT;
        s->theta = wrap_2pi(s->theta + 2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS);
        return s->theta;
    }

    s->sample_counter += 1.0f;
    s->lost_counter++;
    s->theta = wrap_2pi(s->theta + 2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS);

    if (s->lost_counter > lost_limit)
    {
        s->locked = 0u;
        s->armed = 0u;
        s->sample_counter = 0.0f;
        s->lost_counter = 0u;
        s->freq_hz = PVINV_REF_FREQ_DEFAULT;
        s->u_prev = u;
        return s->theta;
    }

    if (!s->armed)
    {
        if (u < -PVINV_UREF_ZC_HYST)
        {
            s->armed = 1u;
        }
    }
    else
    {
        if ((s->u_prev < 0.0f) && (u >= 0.0f))
        {
            float denom = u - s->u_prev;
            float frac = 0.0f;

            if (abs_f(denom) > 1.0e-6f)
            {
                frac = -s->u_prev / denom;
                frac = clamp_f(frac, 0.0f, 1.0f);
            }

            float period_samples = s->sample_counter - 1.0f + frac;
            if (period_samples > 1.0f)
            {
                float f_meas = 1.0f / (period_samples * PVINV_CTRL_TS);
                if ((f_meas >= PVINV_REF_FREQ_MIN) && (f_meas <= PVINV_REF_FREQ_MAX))
                {
                    s->freq_hz = 0.88f * s->freq_hz + 0.12f * f_meas;

                    float elapsed = 1.0f - frac;
                    float theta_zc_now = 2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS * elapsed;
                    float phase_err = theta_zc_now - s->theta;

                    if (phase_err > M_PI) phase_err -= 2.0f * M_PI;
                    else if (phase_err < -M_PI) phase_err += 2.0f * M_PI;

                    s->theta = wrap_2pi(s->theta + 0.45f * phase_err);
                    s->sample_counter = elapsed;
                    s->lost_counter = 0u;
                    s->locked = 1u;
                }
            }
            s->armed = 0u;
        }
    }

    s->u_prev = u;
    return s->theta;
}

/* ============================================================
 * 4. 准PR电流控制器
 * ============================================================ */

typedef struct
{
    float Kp;
    float Kr;
    float wc;
    float w0;
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float e1;
    float e2;
    float y1;
    float y2;
} PR_t;

static void PR_Reset(PR_t *p)
{
    p->e1 = 0.0f;
    p->e2 = 0.0f;
    p->y1 = 0.0f;
    p->y2 = 0.0f;
}

static void PR_UpdateCoeff(PR_t *p, float freq_hz)
{
    freq_hz = clamp_f(freq_hz, PVINV_REF_FREQ_MIN, PVINV_REF_FREQ_MAX);
    p->w0 = 2.0f * M_PI * freq_hz;

    const float T = PVINV_CTRL_TS;
    const float wc = p->wc;
    const float w0 = p->w0;

    float a0 = 4.0f + 4.0f * wc * T + w0 * w0 * T * T;
    p->a1 = (-8.0f + 2.0f * w0 * w0 * T * T) / a0;
    p->a2 = ( 4.0f - 4.0f * wc * T + w0 * w0 * T * T) / a0;
    p->b0 = (4.0f * p->Kr * wc * T) / a0;
    p->b1 = 0.0f;
    p->b2 = -p->b0;
}

static void PR_Init(PR_t *p, float kp, float kr, float wc_hz, float freq_hz)
{
    p->Kp = kp;
    p->Kr = kr;
    p->wc = 2.0f * M_PI * wc_hz;
    PR_Reset(p);
    PR_UpdateCoeff(p, freq_hz);
}

static float PR_Update(PR_t *p, float err, float ff)
{
    float yr =
        -p->a1 * p->y1
        -p->a2 * p->y2
        + p->b0 * err
        + p->b1 * p->e1
        + p->b2 * p->e2;

    yr = clamp_f(yr, -PVINV_PR_RESONANT_LIMIT, PVINV_PR_RESONANT_LIMIT);

    float unsat = ff + p->Kp * err + yr;
    float out = clamp_f(unsat, PVINV_MOD_MIN, PVINV_MOD_MAX);

    float sat_err = out - unsat;
    yr += PVINV_PR_AW_GAIN * sat_err;
    yr = clamp_f(yr, -PVINV_PR_RESONANT_LIMIT, PVINV_PR_RESONANT_LIMIT);

    p->e2 = p->e1;
    p->e1 = err;
    p->y2 = p->y1;
    p->y1 = yr;

    return out;
}

/* ============================================================
 * 5. 电导增量法MPPT
 * ============================================================ */

typedef struct
{
    float v_prev;
    float i_prev;
    float iamp_target;
    int8_t probe_dir;
    uint8_t initialized;
    float g_last;
} MPPT_IncCond_t;

static void MPPT_Init(MPPT_IncCond_t *m)
{
    m->v_prev = 0.0f;
    m->i_prev = 0.0f;
    m->iamp_target = 0.0f;
    m->probe_dir = 1;
    m->initialized = 0u;
    m->g_last = 0.0f;
}

static void MPPT_Reset(MPPT_IncCond_t *m)
{
    MPPT_Init(m);
}

static float MPPT_Update(MPPT_IncCond_t *m, float v, float i, float *g_out)
{
    if (g_out != 0) *g_out = m->g_last;

    if (i < 0.0f) i = 0.0f;
    if (v < 1.0f) return m->iamp_target;

    if (!m->initialized)
    {
        m->v_prev = v;
        m->i_prev = i;
        m->iamp_target = PVINV_SOFTSTART_IAMP;
        m->probe_dir = 1;
        m->initialized = 1u;
        m->g_last = 0.0f;
        return m->iamp_target;
    }

    float dV = v - m->v_prev;
    float dI = i - m->i_prev;

    if (abs_f(dV) < PVINV_MPPT_DV_EPS)
    {
        if (abs_f(dI) < PVINV_MPPT_DI_EPS)
        {
            m->iamp_target += (float)m->probe_dir * PVINV_MPPT_PROBE_STEP;
        }
        else
        {
            if (dI > 0.0f)
            {
                m->iamp_target += (float)m->probe_dir * PVINV_MPPT_PROBE_STEP;
            }
            else
            {
                m->probe_dir = (int8_t)(-m->probe_dir);
                m->iamp_target += (float)m->probe_dir * PVINV_MPPT_PROBE_STEP;
            }
        }
    }
    else
    {
        float inc_cond = dI / dV;
        float inst_cond = i / v;
        float g = inc_cond + inst_cond;
        m->g_last = g;
        if (g_out != 0) *g_out = g;

        float gain = 1.0f + 18.0f * abs_f(g);
        gain = clamp_f(gain, 1.0f, PVINV_MPPT_STEP_GAIN_MAX);
        float step = PVINV_MPPT_BASE_STEP * gain;

        if (g > PVINV_MPPT_G_EPS)
        {
            /* MPP左侧，Ud偏低，需要Ud升高；本系统减小Iamp。 */
            m->iamp_target -= step;
            m->probe_dir = -1;
        }
        else if (g < -PVINV_MPPT_G_EPS)
        {
            /* MPP右侧，Ud偏高，需要Ud降低；本系统增大Iamp。 */
            m->iamp_target += step;
            m->probe_dir = 1;
        }
    }

    m->iamp_target = clamp_f(m->iamp_target, PVINV_IAMP_MIN, PVINV_IAMP_MAX);
    m->v_prev = v;
    m->i_prev = i;
    return m->iamp_target;
}

/* ============================================================
 * 6. 全局对象
 * ============================================================ */

static PVINV_Handle_t g_inv;

static LPF1_t g_lpf_ud;
static LPF1_t g_lpf_id;
static LPF1_t g_lpf_uref;
static LPF1_t g_lpf_ifb;
static LPF1_t g_lpf_uo;

static RefSync_t g_refsync;
static PR_t g_pr;
static MPPT_IncCond_t g_mppt;

static uint32_t g_pr_update_cnt = 0u;
static float g_mppt_v_sum = 0.0f;
static float g_mppt_i_sum = 0.0f;
static uint32_t g_mppt_sample_count = 0u;

/* ============================================================
 * 7. ADC换算
 * ============================================================ */

static inline float adc_to_ud(uint16_t raw)
{
    return (float)raw * PVINV_UD_SCALE + PVINV_UD_OFFSET;
}

static inline float adc_to_id(uint16_t raw)
{
    return PVINV_ID_SIGN * ((float)raw * PVINV_ID_SCALE + PVINV_ID_OFFSET);
}

static inline float adc_to_uref(uint16_t raw)
{
    return (float)raw * PVINV_UREF_SCALE + PVINV_UREF_OFFSET;
}

static inline float adc_to_ifb(uint16_t raw)
{
    return PVINV_IFB_SIGN * ((float)raw * PVINV_IFB_SCALE + PVINV_IFB_OFFSET);
}

static inline float adc_to_uo(uint16_t raw)
{
    return (float)raw * PVINV_UO_SCALE + PVINV_UO_OFFSET;
}

/* ============================================================
 * 8. TIM8 PWM/单极性SPWM
 * ============================================================ */

static uint32_t PWM_GetCenterCCR(void)
{
    uint32_t arr = PVINV_PWM_TIM->ARR;
    uint32_t center = arr / 2u;
    g_inv.pwm_center_fallback = 0u;

#if PVINV_PWM_USE_FIXED_CENTER
    if (arr > (2u * PVINV_PWM_FIXED_CENTER_CCR + 4u))
    {
        center = PVINV_PWM_FIXED_CENTER_CCR;
    }
    else
    {
        center = arr / 2u;
        g_inv.pwm_center_fallback = 1u;
    }
#endif

    if (center < 4u)
    {
        center = 4u;
        g_inv.pwm_center_fallback = 1u;
    }

    g_inv.pwm_center_ccr = center;
    return center;
}

static void PWM_DisableUnusedChannels(void)
{
    uint32_t mask = 0u;
#ifdef TIM_CCER_CC3E
    mask |= TIM_CCER_CC3E;
#endif
#ifdef TIM_CCER_CC3NE
    mask |= TIM_CCER_CC3NE;
#endif
#ifdef TIM_CCER_CC4E
    mask |= TIM_CCER_CC4E;
#endif
#ifdef TIM_CCER_CC4NE
    mask |= TIM_CCER_CC4NE;
#endif
    PVINV_PWM_TIM->CCER &= ~mask;
}

static void PWM_EnableUsedChannels(void)
{
    uint32_t mask = 0u;
#ifdef TIM_CCER_CC1E
    mask |= TIM_CCER_CC1E;
#endif
#ifdef TIM_CCER_CC1NE
    mask |= TIM_CCER_CC1NE;
#endif
#ifdef TIM_CCER_CC2E
    mask |= TIM_CCER_CC2E;
#endif
#ifdef TIM_CCER_CC2NE
    mask |= TIM_CCER_CC2NE;
#endif
    PVINV_PWM_TIM->CCER |= mask;
}

static void PWM_SetZeroDuty(void)
{
    uint32_t center = PWM_GetCenterCCR();
    PVINV_PWM_TIM->CCR1 = center;
    PVINV_PWM_TIM->CCR2 = center;
}

static void PWM_EnableOutputFast(void)
{
    PWM_DisableUnusedChannels();
    PWM_EnableUsedChannels();
    PWM_SetZeroDuty();
    PVINV_PWM_TIM->BDTR |= TIM_BDTR_MOE;
    g_inv.pwm_output_enabled = 1u;
}

static void PWM_DisableOutputFast(void)
{
    PWM_SetZeroDuty();
    PVINV_PWM_TIM->BDTR &= ~TIM_BDTR_MOE;
    g_inv.pwm_output_enabled = 0u;
}

static void PWM_ClearBreakFlags(void)
{
#ifdef TIM_SR_BIF
    PVINV_PWM_TIM->SR &= ~TIM_SR_BIF;
#endif
#ifdef TIM_SR_B2IF
    PVINV_PWM_TIM->SR &= ~TIM_SR_B2IF;
#endif
}

static void SPWM_Unipolar_Update(float m)
{
    m = clamp_f(m, PVINV_MOD_MIN, PVINV_MOD_MAX);

    uint32_t arr = PVINV_PWM_TIM->ARR;
    uint32_t center = PWM_GetCenterCCR();

    float ccr1_f = (float)center + (float)center * m;
    float ccr2_f = (float)center - (float)center * m;

    ccr1_f = clamp_f(ccr1_f, 2.0f, (float)arr - 2.0f);
    ccr2_f = clamp_f(ccr2_f, 2.0f, (float)arr - 2.0f);

    PVINV_PWM_TIM->CCR1 = (uint32_t)ccr1_f;
    PVINV_PWM_TIM->CCR2 = (uint32_t)ccr2_f;
}

/* ============================================================
 * 9. 保护
 * ============================================================ */

static void EnterFault(PVINV_State_t fault)
{
    g_inv.state = fault;
    g_inv.fault_cnt = 0u;

    g_inv.iamp_target = 0.0f;
    g_inv.iamp_cmd = 0.0f;
    g_inv.i_ref = 0.0f;
    g_inv.current_err = 0.0f;
    g_inv.modulation = 0.0f;

    PWM_DisableOutputFast();
    PR_Reset(&g_pr);
    MPPT_Reset(&g_mppt);
}

static void Protection_CheckFast(void)
{
    if ((g_inv.state != PVINV_STATE_SOFT_START) &&
        (g_inv.state != PVINV_STATE_RUN))
    {
        return;
    }

    if (!g_inv.adc_samples_valid)
    {
        EnterFault(PVINV_STATE_FAULT_ADC_INVALID);
        return;
    }

    if (g_inv.ud_f < PVINV_UD_UNDER_TH)
    {
        EnterFault(PVINV_STATE_FAULT_UNDERVOLTAGE);
        return;
    }

    if (g_inv.ud_f > PVINV_UD_OVER_TH)
    {
        EnterFault(PVINV_STATE_FAULT_OVERVOLTAGE);
        return;
    }

    if (abs_f(g_inv.ifb) > PVINV_IFB_OC_TH)
    {
        EnterFault(PVINV_STATE_FAULT_OVERCURRENT);
        return;
    }

    if (g_inv.pwm_output_enabled && ((PVINV_PWM_TIM->BDTR & TIM_BDTR_MOE) == 0u))
    {
        EnterFault(PVINV_STATE_FAULT_PWM_BREAK);
        return;
    }

    if (!g_inv.ref_locked && (g_inv.state == PVINV_STATE_RUN))
    {
        EnterFault(PVINV_STATE_FAULT_REF_LOST);
        return;
    }
}

static void FaultRecover_Task(void)
{
    uint8_t recover_ok = 0u;

    switch (g_inv.state)
    {
        case PVINV_STATE_FAULT_ADC_INVALID:
            recover_ok = g_inv.adc_samples_valid;
            break;

        case PVINV_STATE_FAULT_UNDERVOLTAGE:
            recover_ok = (g_inv.ud_f > PVINV_UD_RECOVER_TH);
            break;

        case PVINV_STATE_FAULT_OVERVOLTAGE:
            recover_ok = (g_inv.ud_f < PVINV_UD_OVER_RECOVER_TH);
            break;

        case PVINV_STATE_FAULT_OVERCURRENT:
            recover_ok = (abs_f(g_inv.ifb_f) < PVINV_IFB_OC_RECOVER_TH);
            break;

        case PVINV_STATE_FAULT_REF_LOST:
            recover_ok = g_inv.ref_locked;
            break;

        case PVINV_STATE_FAULT_PWM_BREAK:
            recover_ok = 0u;
            break;

        default:
            return;
    }

    if (recover_ok) g_inv.fault_cnt++;
    else g_inv.fault_cnt = 0u;

    if (g_inv.fault_cnt > (uint32_t)(0.5f * PVINV_CTRL_FREQ_HZ))
    {
        g_inv.fault_cnt = 0u;
        g_inv.iamp_target = 0.0f;
        g_inv.iamp_cmd = 0.0f;
        g_inv.i_ref = 0.0f;
        g_inv.modulation = 0.0f;
        PR_Reset(&g_pr);
        MPPT_Reset(&g_mppt);
        g_inv.state = PVINV_STATE_WAIT_REF;
    }
}

/* ============================================================
 * 10. 对外接口
 * ============================================================ */

void PVINV_LL_SetRawSamples(uint16_t ud, uint16_t id, uint16_t uref, uint16_t ifb, uint16_t uo)
{
    g_pvinv_adc_raw[PVINV_ADC_IDX_UD] = ud;
    g_pvinv_adc_raw[PVINV_ADC_IDX_ID] = id;
    g_pvinv_adc_raw[PVINV_ADC_IDX_UREF] = uref;
    g_pvinv_adc_raw[PVINV_ADC_IDX_IFB] = ifb;
    g_pvinv_adc_raw[PVINV_ADC_IDX_UO] = uo;
    g_inv.adc_samples_valid = 1u;
}

void PVINV_LL_MarkAdcSamplesValid(uint8_t valid)
{
    g_inv.adc_samples_valid = valid ? 1u : 0u;
}

void PVINV_LL_Init(void)
{
    memset(&g_inv, 0, sizeof(g_inv));
    g_inv.state = PVINV_STATE_STOP;
    g_inv.adc_samples_valid = 0u;

    LPF1_Init(&g_lpf_ud,   0.035f, 0.0f);
    LPF1_Init(&g_lpf_id,   0.035f, 0.0f);
    LPF1_Init(&g_lpf_uref, 0.12f,  0.0f);
    LPF1_Init(&g_lpf_ifb,  0.20f,  0.0f);
    LPF1_Init(&g_lpf_uo,   0.08f,  0.0f);

    RefSync_Init(&g_refsync);
    PR_Init(&g_pr, PVINV_PR_KP, PVINV_PR_KR, PVINV_PR_WC_HZ, PVINV_REF_FREQ_DEFAULT);
    MPPT_Init(&g_mppt);

    PWM_DisableUnusedChannels();
    PWM_EnableUsedChannels();
    PWM_SetZeroDuty();
    PWM_ClearBreakFlags();
    PWM_DisableOutputFast();
}

void PVINV_LL_Start(void)
{
    if (g_inv.state == PVINV_STATE_STOP)
    {
        g_inv.state = PVINV_STATE_WAIT_REF;
        g_inv.iamp_target = 0.0f;
        g_inv.iamp_cmd = 0.0f;
        g_inv.i_ref = 0.0f;
        g_inv.modulation = 0.0f;
        g_inv.mppt_cnt = 0u;
        g_inv.fault_cnt = 0u;
        g_inv.tim8_update_div_count = 0u;

        g_mppt_v_sum = 0.0f;
        g_mppt_i_sum = 0.0f;
        g_mppt_sample_count = 0u;

        RefSync_Reset(&g_refsync);
        PR_Reset(&g_pr);
        MPPT_Reset(&g_mppt);
    }
}

void PVINV_LL_Stop(void)
{
    g_inv.state = PVINV_STATE_STOP;
    g_inv.iamp_target = 0.0f;
    g_inv.iamp_cmd = 0.0f;
    g_inv.i_ref = 0.0f;
    g_inv.modulation = 0.0f;

    PWM_DisableOutputFast();
    RefSync_Reset(&g_refsync);
    PR_Reset(&g_pr);
    MPPT_Reset(&g_mppt);
}

const PVINV_Handle_t *PVINV_LL_GetHandle(void)
{
    return &g_inv;
}

void PVINV_LL_ClearPwmBreakFaultAfterCheck(void)
{
    if (g_inv.state != PVINV_STATE_FAULT_PWM_BREAK)
    {
        return;
    }

    PWM_ClearBreakFlags();
    g_inv.fault_cnt = 0u;
    g_inv.iamp_target = 0.0f;
    g_inv.iamp_cmd = 0.0f;
    g_inv.i_ref = 0.0f;
    g_inv.modulation = 0.0f;
    PR_Reset(&g_pr);
    MPPT_Reset(&g_mppt);
    g_inv.state = PVINV_STATE_WAIT_REF;
}

/* ============================================================
 * 11. 主控制ISR：必须固定频率调用
 * ============================================================ */

void PVINV_LL_ControlISR(void)
{
    g_inv.isr_count++;

    if (!g_inv.adc_samples_valid)
    {
        if ((g_inv.state == PVINV_STATE_SOFT_START) || (g_inv.state == PVINV_STATE_RUN))
        {
            EnterFault(PVINV_STATE_FAULT_ADC_INVALID);
        }
        else
        {
            PWM_SetZeroDuty();
        }
        return;
    }

    /* 将volatile DMA缓冲区拷贝到局部变量，减少DMA循环覆盖带来的读数不一致风险。 */
    const uint16_t raw_ud   = g_pvinv_adc_raw[PVINV_ADC_IDX_UD];
    const uint16_t raw_id   = g_pvinv_adc_raw[PVINV_ADC_IDX_ID];
    const uint16_t raw_uref = g_pvinv_adc_raw[PVINV_ADC_IDX_UREF];
    const uint16_t raw_ifb  = g_pvinv_adc_raw[PVINV_ADC_IDX_IFB];
    const uint16_t raw_uo   = g_pvinv_adc_raw[PVINV_ADC_IDX_UO];

    /* 1. ADC raw -> 物理量 */
    g_inv.ud   = adc_to_ud(raw_ud);
    g_inv.id   = adc_to_id(raw_id);
    g_inv.uref = adc_to_uref(raw_uref);
    g_inv.ifb  = adc_to_ifb(raw_ifb);
    g_inv.uo   = adc_to_uo(raw_uo);

    /* 2. 滤波 */
    g_inv.ud_f   = LPF1_Update(&g_lpf_ud,   g_inv.ud);
    g_inv.id_f   = LPF1_Update(&g_lpf_id,   g_inv.id);
    g_inv.uref_f = LPF1_Update(&g_lpf_uref, g_inv.uref);
    g_inv.ifb_f  = LPF1_Update(&g_lpf_ifb,  g_inv.ifb);
    g_inv.uo_f   = LPF1_Update(&g_lpf_uo,   g_inv.uo);

    g_inv.pv_power = (g_inv.id_f > 0.0f) ? (g_inv.ud_f * g_inv.id_f) : 0.0f;

    /* 3. MPPT周期平均 */
    g_mppt_v_sum += g_inv.ud_f;
    g_mppt_i_sum += g_inv.id_f;
    g_mppt_sample_count++;

    /* 4. uREF同步 */
    g_inv.theta = RefSync_Update(&g_refsync, g_inv.uref_f);
    g_inv.ref_freq = g_refsync.freq_hz;
    g_inv.ref_amp = g_refsync.amp_est;
    g_inv.ref_locked = g_refsync.locked;

    /* 5. 快速保护 */
    Protection_CheckFast();

    /* 6. 状态机 */
    switch (g_inv.state)
    {
        case PVINV_STATE_STOP:
        {
            g_inv.iamp_target = 0.0f;
            g_inv.iamp_cmd = 0.0f;
            g_inv.i_ref = 0.0f;
            g_inv.modulation = 0.0f;
            PWM_SetZeroDuty();
            break;
        }

        case PVINV_STATE_WAIT_REF:
        {
            PWM_SetZeroDuty();
            if (g_inv.ref_locked && (g_inv.ud_f > PVINV_UD_RECOVER_TH))
            {
                PR_Reset(&g_pr);
                MPPT_Reset(&g_mppt);
                g_mppt_v_sum = 0.0f;
                g_mppt_i_sum = 0.0f;
                g_mppt_sample_count = 0u;
                g_inv.iamp_target = 0.0f;
                g_inv.iamp_cmd = 0.0f;
                g_inv.modulation = 0.0f;
                PWM_EnableOutputFast();
                g_inv.state = PVINV_STATE_SOFT_START;
            }
            break;
        }

        case PVINV_STATE_SOFT_START:
        {
            g_inv.iamp_target = PVINV_SOFTSTART_IAMP;
            g_inv.iamp_cmd = slew_f(g_inv.iamp_target, g_inv.iamp_cmd, PVINV_IAMP_SLEW_PER_ISR);

            if (abs_f(g_inv.iamp_cmd - g_inv.iamp_target) < 0.003f)
            {
                g_mppt.iamp_target = g_inv.iamp_cmd;
                g_mppt_v_sum = 0.0f;
                g_mppt_i_sum = 0.0f;
                g_mppt_sample_count = 0u;
                g_inv.state = PVINV_STATE_RUN;
            }

            g_pr_update_cnt++;
            if (g_pr_update_cnt >= 20u)
            {
                g_pr_update_cnt = 0u;
                PR_UpdateCoeff(&g_pr, g_inv.ref_freq);
            }

            float theta = wrap_2pi(g_inv.theta + PVINV_PHASE_COMP_RAD);
            g_inv.i_ref = g_inv.iamp_cmd * PVINV_SIN_F32(theta);
            g_inv.current_err = g_inv.i_ref - g_inv.ifb_f;

            float mod_ff = 0.0f;
            if (g_inv.ud_f > 5.0f)
            {
                mod_ff = PVINV_UO_FF_GAIN * (g_inv.uo_f / g_inv.ud_f);
            }

            float mod_target = PR_Update(&g_pr, g_inv.current_err, mod_ff);
            g_inv.modulation = slew_f(mod_target, g_inv.modulation, PVINV_MOD_SLEW_PER_ISR);
            SPWM_Unipolar_Update(g_inv.modulation);
            break;
        }

        case PVINV_STATE_RUN:
        {
            g_inv.mppt_cnt++;
            if (g_inv.mppt_cnt >= PVINV_MPPT_DIV)
            {
                g_inv.mppt_cnt = 0u;

                float v_avg = g_inv.ud_f;
                float i_avg = g_inv.id_f;
                if (g_mppt_sample_count > 0u)
                {
                    v_avg = g_mppt_v_sum / (float)g_mppt_sample_count;
                    i_avg = g_mppt_i_sum / (float)g_mppt_sample_count;
                }

                g_mppt_v_sum = 0.0f;
                g_mppt_i_sum = 0.0f;
                g_mppt_sample_count = 0u;

                g_inv.mppt_v_avg = v_avg;
                g_inv.mppt_i_avg = i_avg;

#if PVINV_TEST_FIXED_IAMP_ENABLE
                g_inv.iamp_target = PVINV_TEST_FIXED_IAMP;
                g_inv.mppt_g = 0.0f;
#else
                g_inv.iamp_target = MPPT_Update(&g_mppt, v_avg, i_avg, &g_inv.mppt_g);
#endif
            }

            g_inv.iamp_cmd = slew_f(g_inv.iamp_target, g_inv.iamp_cmd, PVINV_IAMP_SLEW_PER_ISR);

            g_pr_update_cnt++;
            if (g_pr_update_cnt >= 20u)
            {
                g_pr_update_cnt = 0u;
                PR_UpdateCoeff(&g_pr, g_inv.ref_freq);
            }

            float theta = wrap_2pi(g_inv.theta + PVINV_PHASE_COMP_RAD);
            g_inv.i_ref = g_inv.iamp_cmd * PVINV_SIN_F32(theta);
            g_inv.current_err = g_inv.i_ref - g_inv.ifb_f;

            float mod_ff = 0.0f;
            if (g_inv.ud_f > 5.0f)
            {
                mod_ff = PVINV_UO_FF_GAIN * (g_inv.uo_f / g_inv.ud_f);
            }

            float mod_target = PR_Update(&g_pr, g_inv.current_err, mod_ff);
            g_inv.modulation = slew_f(mod_target, g_inv.modulation, PVINV_MOD_SLEW_PER_ISR);
            SPWM_Unipolar_Update(g_inv.modulation);
            break;
        }

        case PVINV_STATE_FAULT_ADC_INVALID:
        case PVINV_STATE_FAULT_UNDERVOLTAGE:
        case PVINV_STATE_FAULT_OVERVOLTAGE:
        case PVINV_STATE_FAULT_OVERCURRENT:
        case PVINV_STATE_FAULT_REF_LOST:
        case PVINV_STATE_FAULT_PWM_BREAK:
        default:
        {
            g_inv.iamp_target = 0.0f;
            g_inv.iamp_cmd = 0.0f;
            g_inv.i_ref = 0.0f;
            g_inv.modulation = 0.0f;
            PWM_SetZeroDuty();
            FaultRecover_Task();
            break;
        }
    }
}

#if (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_TIM8_UPDATE_HOOK)
void PVINV_LL_OnTim8UpdateIrq(void)
{
#ifdef TIM_SR_UIF
    if ((PVINV_PWM_TIM->SR & TIM_SR_UIF) != 0u)
    {
        PVINV_PWM_TIM->SR &= ~TIM_SR_UIF;
        g_inv.tim8_update_div_count++;
        if (g_inv.tim8_update_div_count >= PVINV_TIM8_UPDATE_ISR_DIV)
        {
            g_inv.tim8_update_div_count = 0u;
            PVINV_LL_ControlISR();
        }
    }
#endif
}
#endif

#if (PVINV_CONTROL_TICK_SOURCE == PVINV_TICK_ADC_DMA_HOOK)
void PVINV_LL_OnAdcDmaCompleteIrq(void)
{
    g_inv.adc_samples_valid = 1u;
    PVINV_LL_ControlISR();
}
#endif
