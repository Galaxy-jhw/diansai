#include "pvinv_control.h"
#include "pvinv_utils.h"
#include <math.h>

#define PVINV_CTRL_TWO_PI (6.28318530717958647692f)

static PVINV_State_t g_state = PVINV_STATE_IDLE;
static PVINV_Fault_t g_fault = PVINV_FAULT_NONE;

static PVINV_MPPT_t g_mppt;
static PVINV_PR_t g_pr;

static PVINV_Diag_t g_diag;

static float g_internal_theta = 0.0f;
static float g_internal_phase_step = 0.0f;

static volatile uint32_t g_fault_stop_pending = 0u;

static void PVINV_Control_SetFaultFromIsr(PVINV_Fault_t fault)
{
    g_fault = fault;
    g_state = PVINV_STATE_FAULT;
    g_fault_stop_pending = 1u;

    /*
     * ISR中只做快速安全关断，不调用完整HAL Stop。
     */
    PVINV_PWM_TIM8_EmergencyShutdownFast();
}

static uint32_t PVINV_Control_CheckProtection(const PVINV_ADC_Meas_t *m)
{
    if (m->ud_f < PVINV_PROT_UD_MIN)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_UNDERVOLTAGE);
        return 0u;
    }

    if (m->ud_f > PVINV_PROT_UD_MAX)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_OVERVOLTAGE);
        return 0u;
    }

    if (fabsf(m->id_f) > PVINV_PROT_ID_ABS_MAX)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_INPUT_OVERCURRENT);
        return 0u;
    }

    if (fabsf(m->iF_f) > PVINV_PROT_IF_ABS_MAX)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_OUTPUT_OVERCURRENT);
        return 0u;
    }

    if (fabsf(m->uo_f) > PVINV_PROT_UO_ABS_MAX)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_OUTPUT_OVERVOLTAGE);
        return 0u;
    }

    return 1u;
}

static float PVINV_Control_GetReferenceUnit(PVINV_ADC_Meas_t *m)
{
#if (PVINV_CFG_USE_EXTERNAL_UREF != 0u)
    return PVINV_ClampFloat(m->uref_norm_f, -1.0f, 1.0f);
#else
    float s;

    g_internal_theta += g_internal_phase_step;
    while (g_internal_theta >= PVINV_CTRL_TWO_PI)
    {
        g_internal_theta -= PVINV_CTRL_TWO_PI;
    }

    while (g_internal_theta < 0.0f)
    {
        g_internal_theta += PVINV_CTRL_TWO_PI;
    }

    s = sinf(g_internal_theta);
    return s;
#endif
}

PVINV_Status_t PVINV_Control_Init(void)
{
    PVINV_PWM_Status_t pwm_status;
    PVINV_ADC_Status_t adc_status;
    PVINV_PWM_Diag_t pwm_diag;
    float control_hz;
    float control_ts;

    g_state = PVINV_STATE_IDLE;
    g_fault = PVINV_FAULT_NONE;
    g_fault_stop_pending = 0u;

    adc_status = PVINV_ADC_Init();
    if (adc_status != PVINV_ADC_OK)
    {
        g_state = PVINV_STATE_FAULT;
        g_fault = PVINV_FAULT_ADC_START;
        return PVINV_ERR_ADC;
    }

    pwm_status = PVINV_PWM_TIM8_Init();
    if (pwm_status != PVINV_PWM_OK)
    {
        g_state = PVINV_STATE_FAULT;
        g_fault = PVINV_FAULT_PWM_CONFIG;
        return PVINV_ERR_PWM;
    }

    pwm_diag = PVINV_PWM_TIM8_GetDiag();
    control_hz = pwm_diag.control_hz;

    if (control_hz <= 1.0f)
    {
        g_state = PVINV_STATE_FAULT;
        g_fault = PVINV_FAULT_PWM_CONFIG;
        return PVINV_ERR_PWM;
    }

    control_ts = 1.0f / control_hz;

    PVINV_MPPT_Init(&g_mppt, control_ts);

    PVINV_PR_Init(&g_pr,
                  PVINV_PR_KP,
                  PVINV_PR_KR,
                  PVINV_PR_F0_HZ,
                  PVINV_PR_WC_HZ,
                  control_ts,
                  PVINV_PR_OUT_MIN,
                  PVINV_PR_OUT_MAX);

    g_internal_theta = 0.0f;
    g_internal_phase_step = PVINV_CTRL_TWO_PI * PVINV_CFG_INTERNAL_REF_HZ / control_hz;

    g_diag.control_hz = control_hz;
    g_diag.control_ts = control_ts;
    g_diag.softstart_gain = 0.0f;
    g_diag.adc_warmup_count = 0u;
    g_diag.pwm = pwm_diag;

    g_state = PVINV_STATE_READY;
    return PVINV_OK;
}

PVINV_Status_t PVINV_Control_Start(void)
{
    PVINV_ADC_Status_t adc_status;
    PVINV_PWM_Status_t pwm_status;

    /*
     * V3修正：
     * 只允许READY启动，避免未Init时从IDLE直接进入闭环。
     */
    if (g_state != PVINV_STATE_READY)
    {
        return PVINV_ERR_NOT_READY;
    }

    adc_status = PVINV_ADC_StartDMA();
    if (adc_status != PVINV_ADC_OK)
    {
        g_state = PVINV_STATE_FAULT;
        g_fault = PVINV_FAULT_ADC_START;
        return PVINV_ERR_ADC;
    }

    pwm_status = PVINV_PWM_TIM8_StartOutputs();
    if (pwm_status != PVINV_PWM_OK)
    {
        g_state = PVINV_STATE_FAULT;
        g_fault = PVINV_FAULT_PWM_CONFIG;
        return PVINV_ERR_PWM;
    }

    PVINV_MPPT_Reset(&g_mppt);
    PVINV_PR_Reset(&g_pr);
    PVINV_ADC_ResetFilters();

    g_diag.softstart_gain = 0.0f;
    g_diag.adc_warmup_count = 0u;
    g_internal_theta = 0.0f;

    PVINV_PWM_TIM8_EnableUpdateInterrupt();

    /*
     * V3修正：
     * 先进入ADC_WARMUP，等待DMA和滤波稳定，再进入SOFTSTART。
     */
    g_state = PVINV_STATE_ADC_WARMUP;
    g_fault = PVINV_FAULT_NONE;
    g_fault_stop_pending = 0u;

    return PVINV_OK;
}

void PVINV_Control_Stop(void)
{
    PVINV_PWM_TIM8_DisableUpdateInterrupt();
    PVINV_PWM_TIM8_SetUnipolarD(0.0f);
    PVINV_PWM_TIM8_StopOutputs();

    g_diag.softstart_gain = 0.0f;
    g_diag.adc_warmup_count = 0u;
    g_state = PVINV_STATE_READY;
}

void PVINV_Control_ClearFault(void)
{
    PVINV_PWM_TIM8_DisableUpdateInterrupt();
    PVINV_PWM_TIM8_SetUnipolarD(0.0f);
    PVINV_PWM_TIM8_StopOutputs();

    PVINV_PR_Reset(&g_pr);
    PVINV_MPPT_Reset(&g_mppt);
    PVINV_ADC_ResetFilters();

    g_fault = PVINV_FAULT_NONE;
    g_fault_stop_pending = 0u;
    g_diag.softstart_gain = 0.0f;
    g_diag.adc_warmup_count = 0u;
    g_state = PVINV_STATE_READY;
}

void PVINV_Control_Service(void)
{
    if (g_fault_stop_pending != 0u)
    {
        g_fault_stop_pending = 0u;
        PVINV_PWM_TIM8_StopOutputs();
    }
}

void PVINV_Control_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == 0)
    {
        return;
    }

    if (htim->Instance == TIM8)
    {
        PVINV_Control_OnTim8UpdateIrq();
    }
}

void PVINV_Control_OnTim8UpdateIrq(void)
{
    PVINV_ADC_Meas_t m;
    PVINV_PWM_Status_t pwm_status;

    float ref_unit;
    float i_amp_cmd;
    float i_amp_soft;
    float i_ref;
    float i_err;
    float pr_out;
    float v_ff;
    float ud_safe;
    float d;

    if (g_state == PVINV_STATE_ADC_WARMUP)
    {
        PVINV_ADC_Update();
        m = PVINV_ADC_GetMeas();

        PVINV_PWM_TIM8_SetUnipolarD(0.0f);

        g_diag.adc_warmup_count++;
        g_diag.meas = m;
        g_diag.pwm = PVINV_PWM_TIM8_GetDiag();

        if (g_diag.adc_warmup_count >= PVINV_ADC_WARMUP_ISR_TICKS)
        {
            /*
             * 预热结束后再开始软启动和保护判断。
             */
            PVINV_PR_Reset(&g_pr);
            PVINV_MPPT_Reset(&g_mppt);
            g_diag.softstart_gain = 0.0f;
            g_state = PVINV_STATE_SOFTSTART;
        }

        return;
    }

    if ((g_state != PVINV_STATE_SOFTSTART) &&
        (g_state != PVINV_STATE_CLOSED_LOOP))
    {
        PVINV_PWM_TIM8_SetUnipolarD(0.0f);
        return;
    }

    PVINV_ADC_Update();
    m = PVINV_ADC_GetMeas();

    if (PVINV_Control_CheckProtection(&m) == 0u)
    {
        return;
    }

    if (g_state == PVINV_STATE_SOFTSTART)
    {
        g_diag.softstart_gain += PVINV_SOFTSTART_RAMP_PER_SEC * g_diag.control_ts;
        if (g_diag.softstart_gain >= 1.0f)
        {
            g_diag.softstart_gain = 1.0f;
            g_state = PVINV_STATE_CLOSED_LOOP;
        }
    }

    ref_unit = PVINV_Control_GetReferenceUnit(&m);

    /*
     * MPPT电导增量法 + PV电压外环，生成输出电流幅值命令。
     */
    i_amp_cmd = PVINV_MPPT_Update(&g_mppt, m.ud_f, m.id_f);
    i_amp_soft = i_amp_cmd * g_diag.softstart_gain;

    /*
     * 电流参考：
     * i_ref = i_amp_soft * ref_unit。
     */
    i_ref = i_amp_soft * ref_unit;
    i_err = i_ref - m.iF_f;

    /*
     * PR电流环。
     */
    pr_out = PVINV_PR_Update(&g_pr, i_err);

    /*
     * 电压前馈，默认关闭。
     */
    v_ff = PVINV_VFF_AMP * ref_unit;

    ud_safe = fabsf(m.ud_f);
    if (ud_safe < PVINV_PROT_UD_MIN)
    {
        ud_safe = PVINV_PROT_UD_MIN;
    }

    /*
     * 闭环调制量：
     * D = (v_ff + pr_out) / Ud。
     *
     * 底层PWM模块执行：
     * CCR1 = center + center*D
     * CCR2 = center - center*D
     * 即单极性SPWM。
     */
    d = (v_ff + pr_out) / ud_safe;
    d = PVINV_ClampFloat(d, -PVINV_CFG_D_LIMIT, PVINV_CFG_D_LIMIT);

    pwm_status = PVINV_PWM_TIM8_SetUnipolarD(d);
    if (pwm_status != PVINV_PWM_OK)
    {
        PVINV_Control_SetFaultFromIsr(PVINV_FAULT_PWM_UPDATE);
        return;
    }

    g_diag.state = g_state;
    g_diag.fault = g_fault;
    g_diag.meas = m;
    g_diag.pwm = PVINV_PWM_TIM8_GetDiag();

    g_diag.ref_unit = ref_unit;
    g_diag.mppt_v_ref = PVINV_MPPT_GetVref(&g_mppt);
    g_diag.i_amp_cmd = i_amp_cmd;
    g_diag.i_amp_soft = i_amp_soft;
    g_diag.i_ref = i_ref;
    g_diag.i_err = i_err;
    g_diag.pr_out = pr_out;
    g_diag.v_ff = v_ff;
    g_diag.duty_d = d;
}

PVINV_State_t PVINV_Control_GetState(void)
{
    return g_state;
}

PVINV_Fault_t PVINV_Control_GetFault(void)
{
    return g_fault;
}

PVINV_Diag_t PVINV_Control_GetDiag(void)
{
    return g_diag;
}

const char *PVINV_StateToString(PVINV_State_t state)
{
    switch (state)
    {
        case PVINV_STATE_IDLE: return "IDLE";
        case PVINV_STATE_READY: return "READY";
        case PVINV_STATE_ADC_WARMUP: return "ADC_WARMUP";
        case PVINV_STATE_SOFTSTART: return "SOFTSTART";
        case PVINV_STATE_CLOSED_LOOP: return "CLOSED_LOOP";
        case PVINV_STATE_FAULT: return "FAULT";
        default: return "UNKNOWN_STATE";
    }
}

const char *PVINV_FaultToString(PVINV_Fault_t fault)
{
    switch (fault)
    {
        case PVINV_FAULT_NONE: return "NONE";
        case PVINV_FAULT_PWM_CONFIG: return "PWM_CONFIG";
        case PVINV_FAULT_ADC_START: return "ADC_START";
        case PVINV_FAULT_UNDERVOLTAGE: return "UNDERVOLTAGE";
        case PVINV_FAULT_OVERVOLTAGE: return "OVERVOLTAGE";
        case PVINV_FAULT_INPUT_OVERCURRENT: return "INPUT_OVERCURRENT";
        case PVINV_FAULT_OUTPUT_OVERCURRENT: return "OUTPUT_OVERCURRENT";
        case PVINV_FAULT_OUTPUT_OVERVOLTAGE: return "OUTPUT_OVERVOLTAGE";
        case PVINV_FAULT_PWM_UPDATE: return "PWM_UPDATE";
        default: return "UNKNOWN_FAULT";
    }
}
