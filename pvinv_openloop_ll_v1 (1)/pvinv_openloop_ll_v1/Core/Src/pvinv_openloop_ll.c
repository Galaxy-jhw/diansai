#include "pvinv_openloop_ll.h"
#include <math.h>

static PVOL_Handle_t g_ol;

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static uint32_t get_pwm_center_ccr(uint8_t *fallback)
{
    uint32_t arr = PVOL_PWM_TIM->ARR;

#if (PVOL_PWM_USE_FIXED_CENTER != 0u)
    if ((PVOL_PWM_FIXED_CENTER_CCR > 0u) && ((2u * PVOL_PWM_FIXED_CENTER_CCR) <= arr))
    {
        if (fallback) *fallback = 0u;
        return PVOL_PWM_FIXED_CENTER_CCR;
    }
#endif

    if (fallback) *fallback = 1u;
    return arr / 2u;
}

static void pwm_write_center(void)
{
    uint8_t fb = 0u;
    uint32_t center = get_pwm_center_ccr(&fb);

    PVOL_PWM_TIM->CCR1 = center;
    PVOL_PWM_TIM->CCR2 = center;

    g_ol.pwm_center_ccr = center;
    g_ol.pwm_center_fallback = fb;
    g_ol.ccr1_last = center;
    g_ol.ccr2_last = center;
    g_ol.modulation_inst = 0.0f;
}

static void pwm_enable_outputs(void)
{
    /* 只启用CH1/CH1N、CH2/CH2N。 */
#ifdef TIM_CCER_CC1E
    PVOL_PWM_TIM->CCER |= TIM_CCER_CC1E;
#endif
#ifdef TIM_CCER_CC1NE
    PVOL_PWM_TIM->CCER |= TIM_CCER_CC1NE;
#endif
#ifdef TIM_CCER_CC2E
    PVOL_PWM_TIM->CCER |= TIM_CCER_CC2E;
#endif
#ifdef TIM_CCER_CC2NE
    PVOL_PWM_TIM->CCER |= TIM_CCER_CC2NE;
#endif

    /* 主动关闭CH3/CH3N、CH4，防止误驱动。 */
#ifdef TIM_CCER_CC3E
    PVOL_PWM_TIM->CCER &= ~TIM_CCER_CC3E;
#endif
#ifdef TIM_CCER_CC3NE
    PVOL_PWM_TIM->CCER &= ~TIM_CCER_CC3NE;
#endif
#ifdef TIM_CCER_CC4E
    PVOL_PWM_TIM->CCER &= ~TIM_CCER_CC4E;
#endif

#ifdef TIM_BDTR_MOE
    PVOL_PWM_TIM->BDTR |= TIM_BDTR_MOE;
#endif
    g_ol.output_enabled = 1u;
}

static void pwm_disable_outputs(void)
{
#ifdef TIM_BDTR_MOE
    PVOL_PWM_TIM->BDTR &= ~TIM_BDTR_MOE;
#endif
    pwm_write_center();
    g_ol.output_enabled = 0u;
}

static void pwm_write_unipolar(float m)
{
    uint8_t fb = 0u;
    uint32_t center_u = get_pwm_center_ccr(&fb);
    float center = (float)center_u;

    m = clampf(m, -PVOL_MAX_MODULATION, PVOL_MAX_MODULATION);

    float ccr1_f = center + center * m;
    float ccr2_f = center - center * m;

    if (ccr1_f < 0.0f) ccr1_f = 0.0f;
    if (ccr2_f < 0.0f) ccr2_f = 0.0f;
    if (ccr1_f > (float)PVOL_PWM_TIM->ARR) ccr1_f = (float)PVOL_PWM_TIM->ARR;
    if (ccr2_f > (float)PVOL_PWM_TIM->ARR) ccr2_f = (float)PVOL_PWM_TIM->ARR;

    PVOL_PWM_TIM->CCR1 = (uint32_t)(ccr1_f + 0.5f);
    PVOL_PWM_TIM->CCR2 = (uint32_t)(ccr2_f + 0.5f);

    g_ol.pwm_center_ccr = center_u;
    g_ol.pwm_center_fallback = fb;
    g_ol.ccr1_last = PVOL_PWM_TIM->CCR1;
    g_ol.ccr2_last = PVOL_PWM_TIM->CCR2;
    g_ol.modulation_inst = m;
}

void PVOL_Init(void)
{
    g_ol.state = PVOL_STATE_READY;
    g_ol.ref_freq_hz = PVOL_DEFAULT_REF_FREQ_HZ;
    g_ol.target_modulation = clampf(PVOL_DEFAULT_TARGET_MODULATION, 0.0f, PVOL_MAX_MODULATION);
    g_ol.modulation_cmd = 0.0f;
    g_ol.modulation_inst = 0.0f;
    g_ol.phase_rad = 0.0f;
    g_ol.isr_count = 0u;
    g_ol.tim8_update_count = 0u;
    g_ol.output_enabled = 0u;

    pwm_disable_outputs();
}

void PVOL_Start(void)
{
    if (g_ol.state == PVOL_STATE_FAULT_PARAM)
    {
        return;
    }

    g_ol.phase_rad = 0.0f;
    g_ol.modulation_cmd = 0.0f;
    pwm_write_center();
    pwm_enable_outputs();
    g_ol.state = PVOL_STATE_RUN;
}

void PVOL_Stop(void)
{
    pwm_disable_outputs();
    g_ol.modulation_cmd = 0.0f;
    g_ol.phase_rad = 0.0f;
    g_ol.state = PVOL_STATE_READY;
}

void PVOL_SetFrequencyHz(float freq_hz)
{
    if ((freq_hz < 1.0f) || (freq_hz > 200.0f))
    {
        g_ol.state = PVOL_STATE_FAULT_PARAM;
        pwm_disable_outputs();
        return;
    }
    g_ol.ref_freq_hz = freq_hz;
}

void PVOL_SetTargetModulation(float modulation)
{
    g_ol.target_modulation = clampf(modulation, 0.0f, PVOL_MAX_MODULATION);
}

void PVOL_ControlISR(void)
{
    g_ol.isr_count++;

    if (g_ol.state != PVOL_STATE_RUN)
    {
        pwm_write_center();
        return;
    }

    /* 软启动：逐步从0爬升到target_modulation。 */
    float step = PVOL_MAX_MODULATION / (PVOL_SOFTSTART_TIME_S * PVOL_CTRL_FREQ_HZ);
    if (step < 0.0000001f) step = 0.0000001f;

    if (g_ol.modulation_cmd < g_ol.target_modulation)
    {
        g_ol.modulation_cmd += step;
        if (g_ol.modulation_cmd > g_ol.target_modulation)
        {
            g_ol.modulation_cmd = g_ol.target_modulation;
        }
    }
    else if (g_ol.modulation_cmd > g_ol.target_modulation)
    {
        g_ol.modulation_cmd -= step;
        if (g_ol.modulation_cmd < g_ol.target_modulation)
        {
            g_ol.modulation_cmd = g_ol.target_modulation;
        }
    }

    float dtheta = 2.0f * PVOL_PI_F * g_ol.ref_freq_hz / PVOL_CTRL_FREQ_HZ;
    g_ol.phase_rad += dtheta;
    if (g_ol.phase_rad >= (2.0f * PVOL_PI_F))
    {
        g_ol.phase_rad -= (2.0f * PVOL_PI_F);
    }

    float m = g_ol.modulation_cmd * sinf(g_ol.phase_rad);

    /* 单极性SPWM：CCR1=center+center*m，CCR2=center-center*m。 */
    pwm_write_unipolar(m);
}

void PVOL_OnTim8UpdateIrq(void)
{
    g_ol.tim8_update_count++;

#if (PVOL_TIM8_UPDATE_ISR_DIV <= 1u)
    PVOL_ControlISR();
#else
    static uint32_t div_cnt = 0u;
    div_cnt++;
    if (div_cnt >= PVOL_TIM8_UPDATE_ISR_DIV)
    {
        div_cnt = 0u;
        PVOL_ControlISR();
    }
#endif
}

const PVOL_Handle_t *PVOL_GetHandle(void)
{
    return &g_ol;
}
