#include "pv_inv_control.h"
#include "adc.h"
#include "tim.h"
#include "arm_math.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

extern ADC_HandleTypeDef PVINV_ADC_HANDLE;
extern TIM_HandleTypeDef PVINV_PWM_HANDLE;

/* ============================================================
 * 1. ADC DMA 缓冲区
 * ============================================================ */

volatile uint16_t g_pvinv_adc_dma[PVINV_ADC_CH_NUM];

/* ============================================================
 * 2. 工具函数
 * ============================================================ */

static inline float clamp_f(float x, float min_v, float max_v)
{
    if (x > max_v) return max_v;
    if (x < min_v) return min_v;
    return x;
}

static inline float abs_f(float x)
{
    return x >= 0.0f ? x : -x;
}

static inline float slew_f(float target, float now, float step)
{
    if (target > now + step) return now + step;
    if (target < now - step) return now - step;
    return target;
}

static inline float wrap_2pi(float x)
{
    while (x >= 2.0f * M_PI)
    {
        x -= 2.0f * M_PI;
    }

    while (x < 0.0f)
    {
        x += 2.0f * M_PI;
    }

    return x;
}

/* ============================================================
 * 3. 一阶低通
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
 * 4. uREF 过零同步器
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
    const uint32_t lost_limit =
        (uint32_t)(PVINV_REF_LOST_TIME_S * PVINV_CTRL_FREQ_HZ);

    /*
     * 幅值估计。
     * abs(sin) 平均值约为 0.637A，这里只用于判断 uREF 是否存在。
     */
    s->amp_est += 0.002f * (abs_f(u) - s->amp_est);

    /*
     * 幅值太低，不允许锁定。
     */
    if (s->amp_est < PVINV_UREF_AMP_MIN)
    {
        s->locked = 0u;
        s->armed = 0u;
        s->sample_counter = 0.0f;
        s->lost_counter = 0u;
        s->u_prev = u;

        s->freq_hz = PVINV_REF_FREQ_DEFAULT;
        s->theta += 2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS;
        s->theta = wrap_2pi(s->theta);

        return s->theta;
    }

    s->sample_counter += 1.0f;
    s->lost_counter++;

    /*
     * 相位连续积分。
     */
    s->theta += 2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS;
    s->theta = wrap_2pi(s->theta);

    /*
     * 长时间没有有效过零，认为参考丢失。
     */
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

    /*
     * 滞回上升过零。
     */
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

            if (abs_f(denom) > 1e-6f)
            {
                frac = -s->u_prev / denom;
                frac = clamp_f(frac, 0.0f, 1.0f);
            }

            float period_samples = s->sample_counter - 1.0f + frac;

            if (period_samples > 1.0f)
            {
                float f_meas = 1.0f / (period_samples * PVINV_CTRL_TS);

                if ((f_meas >= PVINV_REF_FREQ_MIN) &&
                    (f_meas <= PVINV_REF_FREQ_MAX))
                {
                    s->freq_hz = 0.88f * s->freq_hz + 0.12f * f_meas;

                    float elapsed = 1.0f - frac;
                    float theta_zc_now =
                        2.0f * M_PI * s->freq_hz * PVINV_CTRL_TS * elapsed;

                    float phase_err = theta_zc_now - s->theta;

                    if (phase_err > M_PI)
                    {
                        phase_err -= 2.0f * M_PI;
                    }
                    else if (phase_err < -M_PI)
                    {
                        phase_err += 2.0f * M_PI;
                    }

                    /*
                     * 平滑修正相位，不硬重置，减小抖动。
                     */
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
 * 5. PR / QPR 电流控制器
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

    float T = PVINV_CTRL_TS;
    float wc = p->wc;
    float w0 = p->w0;

    /*
     * 准 PR 谐振项：
     *
     * Gres(s) = Kr * 2wc*s / (s^2 + 2wc*s + w0^2)
     *
     * 双线性离散化。
     */
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

/*
 * 带抗饱和的 PR 更新。
 * ff 是可选前馈调制量。
 */
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

    /*
     * back-calculation 抗饱和。
     */
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
 * 6. 电导增量法 MPPT
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

    /*
     * probe_dir:
     * +1：下一次探索时增加 Iamp
     * -1：下一次探索时减小 Iamp
     */
    m->probe_dir = 1;
    m->initialized = 0u;
    m->g_last = 0.0f;
}

static void MPPT_Reset(MPPT_IncCond_t *m)
{
    MPPT_Init(m);
}

/*
 * 电导增量法：
 *
 * P = U * I
 * dP/dU = I + U * dI/dU
 *
 * MPP 条件：
 * dP/dU = 0
 * dI/dU = -I/U
 *
 * 定义：
 * g = dI/dU + I/U
 *
 * g > 0:
 *     dP/dU > 0
 *     位于 MPP 左侧，Ud 偏低，需要 Ud 升高。
 *     本系统中：Iamp 减小 -> 取功率减少 -> Ud 升高。
 *
 * g < 0:
 *     dP/dU < 0
 *     位于 MPP 右侧，Ud 偏高，需要 Ud 降低。
 *     本系统中：Iamp 增大 -> 取功率增加 -> Ud 降低。
 */
static float MPPT_Update(MPPT_IncCond_t *m, float v, float i, float *g_out)
{
    if (g_out != 0)
    {
        *g_out = m->g_last;
    }

    if (i < 0.0f)
    {
        i = 0.0f;
    }

    if (v < 1.0f)
    {
        return m->iamp_target;
    }

    if (!m->initialized)
    {
        m->v_prev = v;
        m->i_prev = i;

        /*
         * 小初值，避免从 0 开始没有扰动信息。
         */
        m->iamp_target = 0.18f;
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
            /*
             * ΔU、ΔI 都太小，电导增量法没有足够信息。
             * 做极小探索扰动，不使用 Ud=30V 判断。
             */
            m->iamp_target += (float)m->probe_dir * PVINV_MPPT_PROBE_STEP;
        }
        else
        {
            /*
             * ΔU 很小但 ΔI 明显变化。
             * 该分支容易受噪声影响，只做保守处理。
             */
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

        /*
         * g = dI/dU + I/U
         */
        float g = inc_cond + inst_cond;
        m->g_last = g;

        if (g_out != 0)
        {
            *g_out = g;
        }

        float gain = 1.0f + 18.0f * abs_f(g);
        gain = clamp_f(gain, 1.0f, PVINV_MPPT_STEP_GAIN_MAX);

        float step = PVINV_MPPT_BASE_STEP * gain;

        if (g > PVINV_MPPT_G_EPS)
        {
            /*
             * 左侧，Ud 偏低，需要 Ud 升高。
             * 减小 Iamp。
             */
            m->iamp_target -= step;
            m->probe_dir = -1;
        }
        else if (g < -PVINV_MPPT_G_EPS)
        {
            /*
             * 右侧，Ud 偏高，需要 Ud 降低。
             * 增大 Iamp。
             */
            m->iamp_target += step;
            m->probe_dir = 1;
        }
        else
        {
            /*
             * 接近 MPP，保持。
             */
        }
    }

    m->iamp_target = clamp_f(m->iamp_target, PVINV_IAMP_MIN, PVINV_IAMP_MAX);

    m->v_prev = v;
    m->i_prev = i;

    return m->iamp_target;
}

/* ============================================================
 * 7. 全局对象
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

/* MPPT 周期平均 */
static float g_mppt_v_sum = 0.0f;
static float g_mppt_i_sum = 0.0f;
static uint32_t g_mppt_sample_count = 0u;

/* ============================================================
 * 8. ADC 换算
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
 * 9. PWM / 单极性 SPWM
 * ============================================================ */

static void PWM_SetZeroDuty(void)
{
    uint32_t arr = PVINV_PWM_TIM->ARR;
    uint32_t mid = arr / 2u;

    PVINV_PWM_TIM->CCR1 = mid;
    PVINV_PWM_TIM->CCR2 = mid;
}

static void PWM_EnableOutputFast(void)
{
    PWM_SetZeroDuty();

    /*
     * 高级定时器主输出使能。
     */
    PVINV_PWM_TIM->BDTR |= TIM_BDTR_MOE;

    g_inv.pwm_output_enabled = 1u;
}

static void PWM_DisableOutputFast(void)
{
    PWM_SetZeroDuty();

    /*
     * 关闭高级定时器主输出。
     */
    PVINV_PWM_TIM->BDTR &= ~TIM_BDTR_MOE;

    g_inv.pwm_output_enabled = 0u;
}

static void SPWM_Unipolar_Update(float m)
{
    m = clamp_f(m, PVINV_MOD_MIN, PVINV_MOD_MAX);

    /*
     * 单极性 SPWM：
     *
     * A 桥臂：dutyA = 0.5 + 0.5m
     * B 桥臂：dutyB = 0.5 - 0.5m
     */
    float dutyA = 0.5f + 0.5f * m;
    float dutyB = 0.5f - 0.5f * m;

    dutyA = clamp_f(dutyA, 0.02f, 0.98f);
    dutyB = clamp_f(dutyB, 0.02f, 0.98f);

    uint32_t arr = PVINV_PWM_TIM->ARR;

    PVINV_PWM_TIM->CCR1 = (uint32_t)(dutyA * (float)arr);
    PVINV_PWM_TIM->CCR2 = (uint32_t)(dutyB * (float)arr);
}

/* ============================================================
 * 10. 故障处理
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

    /*
     * 软件过流只是第二层保护。
     * 真正上板建议使用 COMP + TIM Break。
     */
    if (abs_f(g_inv.ifb) > PVINV_IFB_OC_TH)
    {
        EnterFault(PVINV_STATE_FAULT_OVERCURRENT);
        return;
    }

    /*
     * 硬件 Break 把 MOE 清掉后，这里能检测到。
     */
    if (g_inv.pwm_output_enabled &&
        ((PVINV_PWM_TIM->BDTR & TIM_BDTR_MOE) == 0u))
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
            /*
             * Break 故障建议人工复位。
             */
            recover_ok = 0u;
            break;

        default:
            return;
    }

    if (recover_ok)
    {
        g_inv.fault_cnt++;
    }
    else
    {
        g_inv.fault_cnt = 0u;
    }

    /*
     * 约 0.5s 后自动恢复到 WAIT_REF。
     */
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
 * 11. 对外接口
 * ============================================================ */

void PVINV_Init(void)
{
    memset(&g_inv, 0, sizeof(g_inv));

    g_inv.state = PVINV_STATE_STOP;

    /*
     * 滤波参数：
     * Ud / Id 给 MPPT，滤波稍强。
     * uREF 给过零同步，不能滤太重。
     * ifb 给电流环，不能滤太重。
     */
    LPF1_Init(&g_lpf_ud,   0.035f, 0.0f);
    LPF1_Init(&g_lpf_id,   0.035f, 0.0f);
    LPF1_Init(&g_lpf_uref, 0.12f,  0.0f);
    LPF1_Init(&g_lpf_ifb,  0.20f,  0.0f);
    LPF1_Init(&g_lpf_uo,   0.08f,  0.0f);

    RefSync_Init(&g_refsync);

    PR_Init(
        &g_pr,
        PVINV_PR_KP,
        PVINV_PR_KR,
        PVINV_PR_WC_HZ,
        PVINV_REF_FREQ_DEFAULT
    );

    MPPT_Init(&g_mppt);

    PWM_SetZeroDuty();

    /*
     * 初始化时启动 PWM 通道。
     * 后续只通过 MOE 位快速使能/关闭输出。
     */
    HAL_TIM_PWM_Start(&PVINV_PWM_HANDLE, TIM_CHANNEL_1);
    HAL_TIMEx_PWMN_Start(&PVINV_PWM_HANDLE, TIM_CHANNEL_1);

    HAL_TIM_PWM_Start(&PVINV_PWM_HANDLE, TIM_CHANNEL_2);
    HAL_TIMEx_PWMN_Start(&PVINV_PWM_HANDLE, TIM_CHANNEL_2);

    PWM_DisableOutputFast();

    /*
     * 启动 ADC DMA。
     * ADC 必须配置为 Scan + Circular DMA。
     */
    HAL_ADC_Start_DMA(
        &PVINV_ADC_HANDLE,
        (uint32_t *)g_pvinv_adc_dma,
        PVINV_ADC_CH_NUM
    );
}

void PVINV_Start(void)
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

        g_mppt_v_sum = 0.0f;
        g_mppt_i_sum = 0.0f;
        g_mppt_sample_count = 0u;

        RefSync_Reset(&g_refsync);
        PR_Reset(&g_pr);
        MPPT_Reset(&g_mppt);
    }
}

void PVINV_Stop(void)
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

const PVINV_Handle_t *PVINV_GetHandle(void)
{
    return &g_inv;
}

/* ============================================================
 * 12. 主控制中断
 * ============================================================ */

void PVINV_ControlISR(void)
{
    /*
     * 1. ADC 换算
     */
    g_inv.ud   = adc_to_ud(g_pvinv_adc_dma[0]);
    g_inv.id   = adc_to_id(g_pvinv_adc_dma[1]);
    g_inv.uref = adc_to_uref(g_pvinv_adc_dma[2]);
    g_inv.ifb  = adc_to_ifb(g_pvinv_adc_dma[3]);
    g_inv.uo   = adc_to_uo(g_pvinv_adc_dma[4]);

    /*
     * 2. 滤波
     */
    g_inv.ud_f   = LPF1_Update(&g_lpf_ud,   g_inv.ud);
    g_inv.id_f   = LPF1_Update(&g_lpf_id,   g_inv.id);
    g_inv.uref_f = LPF1_Update(&g_lpf_uref, g_inv.uref);
    g_inv.ifb_f  = LPF1_Update(&g_lpf_ifb,  g_inv.ifb);
    g_inv.uo_f   = LPF1_Update(&g_lpf_uo,   g_inv.uo);

    if (g_inv.id_f > 0.0f)
    {
        g_inv.pv_power = g_inv.ud_f * g_inv.id_f;
    }
    else
    {
        g_inv.pv_power = 0.0f;
    }

    /*
     * 3. MPPT 周期平均累加
     */
    g_mppt_v_sum += g_inv.ud_f;
    g_mppt_i_sum += g_inv.id_f;
    g_mppt_sample_count++;

    /*
     * 4. uREF 同步
     */
    g_inv.theta = RefSync_Update(&g_refsync, g_inv.uref_f);
    g_inv.ref_freq = g_refsync.freq_hz;
    g_inv.ref_amp = g_refsync.amp_est;
    g_inv.ref_locked = g_refsync.locked;

    /*
     * 5. 快速保护
     */
    Protection_CheckFast();

    /*
     * 6. 状态机
     */
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

            /*
             * 等待 uREF 锁定，同时输入电压高于恢复阈值。
             */
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
            /*
             * 软启动固定小电流，不让 MPPT 一开始就接管。
             */
            g_inv.iamp_target = 0.18f;

            g_inv.iamp_cmd = slew_f(
                g_inv.iamp_target,
                g_inv.iamp_cmd,
                PVINV_IAMP_SLEW_PER_ISR
            );

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

            g_inv.i_ref = g_inv.iamp_cmd * arm_sin_f32(theta);
            g_inv.current_err = g_inv.i_ref - g_inv.ifb_f;

            float mod_ff = 0.0f;

            if (g_inv.ud_f > 5.0f)
            {
                mod_ff = PVINV_UO_FF_GAIN * (g_inv.uo_f / g_inv.ud_f);
            }

            float mod_target = PR_Update(&g_pr, g_inv.current_err, mod_ff);

            g_inv.modulation = slew_f(
                mod_target,
                g_inv.modulation,
                PVINV_MOD_SLEW_PER_ISR
            );

            SPWM_Unipolar_Update(g_inv.modulation);
            break;
        }

        case PVINV_STATE_RUN:
        {
            /*
             * 6.1 MPPT 电导增量法更新
             */
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
                g_inv.iamp_target = MPPT_Update(
                    &g_mppt,
                    v_avg,
                    i_avg,
                    &g_inv.mppt_g
                );
#endif
            }

            /*
             * 6.2 电流幅值斜率限制
             */
            g_inv.iamp_cmd = slew_f(
                g_inv.iamp_target,
                g_inv.iamp_cmd,
                PVINV_IAMP_SLEW_PER_ISR
            );

            /*
             * 6.3 PR 中心频率跟随 uREF
             */
            g_pr_update_cnt++;
            if (g_pr_update_cnt >= 20u)
            {
                g_pr_update_cnt = 0u;
                PR_UpdateCoeff(&g_pr, g_inv.ref_freq);
            }

            /*
             * 6.4 生成与 uREF 同相的交流电流参考
             */
            float theta = wrap_2pi(g_inv.theta + PVINV_PHASE_COMP_RAD);

            g_inv.i_ref = g_inv.iamp_cmd * arm_sin_f32(theta);

            /*
             * 6.5 PR 电流闭环
             */
            g_inv.current_err = g_inv.i_ref - g_inv.ifb_f;

            float mod_ff = 0.0f;

            if (g_inv.ud_f > 5.0f)
            {
                mod_ff = PVINV_UO_FF_GAIN * (g_inv.uo_f / g_inv.ud_f);
            }

            float mod_target = PR_Update(&g_pr, g_inv.current_err, mod_ff);

            /*
             * 6.6 调制量斜率限制
             */
            g_inv.modulation = slew_f(
                mod_target,
                g_inv.modulation,
                PVINV_MOD_SLEW_PER_ISR
            );

            /*
             * 6.7 单极性 SPWM
             */
            SPWM_Unipolar_Update(g_inv.modulation);
            break;
        }

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
