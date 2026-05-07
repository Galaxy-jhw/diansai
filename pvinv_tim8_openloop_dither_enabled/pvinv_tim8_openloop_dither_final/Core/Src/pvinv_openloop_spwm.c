#include "pvinv_openloop_spwm.h"
#include "tim.h"
#include <math.h>

#define PVINV_OL_TWO_PI            (6.28318530717958647692f)
#define PVINV_OL_MIN_SINE_HZ       (0.0f)
#define PVINV_OL_MAX_SINE_HZ       (1000.0f)

static volatile PVINV_OL_State_t g_ol_state = PVINV_OL_STATE_IDLE;
static volatile PVINV_OL_Status_t g_ol_last_status = PVINV_OL_STATUS_OK;

static volatile float g_sine_hz = PVINV_CFG_DEFAULT_SINE_HZ;
static volatile float g_phase_step_rad = 0.0f;
static volatile float g_theta_rad = 0.0f;

static volatile float g_mod_target = PVINV_CFG_DEFAULT_MODULATION;
static volatile float g_mod_now = 0.0f;

static uint32_t PVINV_OL_EnterCritical(void)
{
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();

    return primask;
}

static void PVINV_OL_ExitCritical(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

static float PVINV_OL_ClampFloat(float x, float min_v, float max_v)
{
    if (x > max_v)
    {
        return max_v;
    }

    if (x < min_v)
    {
        return min_v;
    }

    return x;
}

static void PVINV_OL_UpdatePhaseStep(void)
{
    PVINV_PWM_Diag_t diag;
    float update_hz;
    float sine_hz;

    diag = PVINV_PWM_TIM8_GetDiag();
    update_hz = diag.update_irq_hz;
    sine_hz = g_sine_hz;

    if (update_hz < 1.0f)
    {
        g_phase_step_rad = 0.0f;
        return;
    }

    g_phase_step_rad = PVINV_OL_TWO_PI * sine_hz / update_hz;
}

PVINV_OL_Status_t PVINV_OpenLoopSPWM_Init(void)
{
    PVINV_PWM_Status_t pwm_status;

    g_ol_state = PVINV_OL_STATE_IDLE;
    g_ol_last_status = PVINV_OL_STATUS_OK;

    g_sine_hz = PVINV_CFG_DEFAULT_SINE_HZ;
    g_phase_step_rad = 0.0f;
    g_theta_rad = 0.0f;

    g_mod_target = PVINV_CFG_DEFAULT_MODULATION;
    g_mod_now = 0.0f;

    pwm_status = PVINV_PWM_TIM8_Init();
    if (pwm_status != PVINV_PWM_STATUS_OK)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_CONFIG;
        return g_ol_last_status;
    }

    pwm_status = PVINV_PWM_TIM8_CheckCubeMXConfig();
    if (pwm_status != PVINV_PWM_STATUS_OK)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_CONFIG;
        return g_ol_last_status;
    }

    PVINV_OL_UpdatePhaseStep();

    return PVINV_OL_STATUS_OK;
}

PVINV_OL_Status_t PVINV_OpenLoopSPWM_Start(void)
{
    PVINV_PWM_Status_t pwm_status;

    pwm_status = PVINV_PWM_TIM8_CheckCubeMXConfig();
    if (pwm_status != PVINV_PWM_STATUS_OK)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_CONFIG;
        return g_ol_last_status;
    }

    PVINV_OL_UpdatePhaseStep();

    if (g_phase_step_rad <= 0.0f)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_UPDATE_RATE;
        return g_ol_last_status;
    }

    pwm_status = PVINV_PWM_TIM8_StartOutputs();
    if (pwm_status != PVINV_PWM_STATUS_OK)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_START;
        return g_ol_last_status;
    }

    PVINV_PWM_TIM8_EnableUpdateInterrupt();

    g_theta_rad = 0.0f;
    g_mod_now = 0.0f;
    g_ol_state = PVINV_OL_STATE_RAMP;
    g_ol_last_status = PVINV_OL_STATUS_OK;

    return PVINV_OL_STATUS_OK;
}

void PVINV_OpenLoopSPWM_Stop(void)
{
    PVINV_PWM_TIM8_DisableUpdateInterrupt();
    PVINV_PWM_TIM8_ForceNeutral();
    PVINV_PWM_TIM8_StopOutputs();

    g_mod_now = 0.0f;
    g_theta_rad = 0.0f;
    g_ol_state = PVINV_OL_STATE_IDLE;
}

void PVINV_OpenLoopSPWM_SetSineFrequencyHz(float sine_hz)
{
    uint32_t primask;

    sine_hz = PVINV_OL_ClampFloat(sine_hz,
                                  PVINV_OL_MIN_SINE_HZ,
                                  PVINV_OL_MAX_SINE_HZ);

    primask = PVINV_OL_EnterCritical();
    g_sine_hz = sine_hz;
    PVINV_OL_UpdatePhaseStep();
    PVINV_OL_ExitCritical(primask);
}

void PVINV_OpenLoopSPWM_SetTargetModulation(float modulation)
{
    uint32_t primask;

    modulation = PVINV_OL_ClampFloat(modulation,
                                      0.0f,
                                      PVINV_CFG_D_LIMIT);

    primask = PVINV_OL_EnterCritical();
    g_mod_target = modulation;
    PVINV_OL_ExitCritical(primask);
}

void PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return;
    }

    if (htim->Instance == TIM8)
    {
        PVINV_OpenLoopSPWM_OnTim8UpdateIrq();
    }
}

void PVINV_OpenLoopSPWM_OnTim8UpdateIrq(void)
{
    float theta;
    float mod_now;
    float mod_target;
    float ramp_step;
    float d;
    PVINV_PWM_Diag_t diag;
    PVINV_PWM_Status_t pwm_status;

    if ((g_ol_state != PVINV_OL_STATE_RAMP) &&
        (g_ol_state != PVINV_OL_STATE_RUN))
    {
        PVINV_PWM_TIM8_ForceNeutral();
        return;
    }

    diag = PVINV_PWM_TIM8_GetDiag();

    mod_now = g_mod_now;
    mod_target = g_mod_target;

    if (diag.update_irq_hz > 1.0f)
    {
        ramp_step = PVINV_CFG_MOD_RAMP_PER_SEC / diag.update_irq_hz;
    }
    else
    {
        ramp_step = 0.00001f;
    }

    if (mod_now < mod_target)
    {
        mod_now += ramp_step;
        if (mod_now > mod_target)
        {
            mod_now = mod_target;
        }
    }
    else if (mod_now > mod_target)
    {
        mod_now -= ramp_step;
        if (mod_now < mod_target)
        {
            mod_now = mod_target;
        }
    }
    else
    {
        /* no action */
    }

    g_mod_now = mod_now;

    if ((g_ol_state == PVINV_OL_STATE_RAMP) && (mod_now >= mod_target))
    {
        g_ol_state = PVINV_OL_STATE_RUN;
    }

    theta = g_theta_rad + g_phase_step_rad;

    while (theta >= PVINV_OL_TWO_PI)
    {
        theta -= PVINV_OL_TWO_PI;
    }

    while (theta < 0.0f)
    {
        theta += PVINV_OL_TWO_PI;
    }

    g_theta_rad = theta;

    /*
     * 开环调制：
     * D = M * sin(theta)
     */
    d = mod_now * sinf(theta);

    pwm_status = PVINV_PWM_TIM8_SetUnipolarD(d);
    if (pwm_status != PVINV_PWM_STATUS_OK)
    {
        g_ol_state = PVINV_OL_STATE_FAULT;
        g_ol_last_status = PVINV_OL_STATUS_ERR_PWM_SET_D;
        PVINV_PWM_TIM8_StopOutputs();
        return;
    }
}

PVINV_OL_State_t PVINV_OpenLoopSPWM_GetState(void)
{
    return g_ol_state;
}

PVINV_OL_Status_t PVINV_OpenLoopSPWM_GetLastStatus(void)
{
    return g_ol_last_status;
}

float PVINV_OpenLoopSPWM_GetThetaRad(void)
{
    return g_theta_rad;
}

float PVINV_OpenLoopSPWM_GetSineFrequencyHz(void)
{
    return g_sine_hz;
}

float PVINV_OpenLoopSPWM_GetModulationNow(void)
{
    return g_mod_now;
}

float PVINV_OpenLoopSPWM_GetModulationTarget(void)
{
    return g_mod_target;
}

float PVINV_OpenLoopSPWM_GetPhaseStepRad(void)
{
    return g_phase_step_rad;
}

const char *PVINV_OpenLoopSPWM_StatusToString(PVINV_OL_Status_t status)
{
    switch (status)
    {
        case PVINV_OL_STATUS_OK:
            return "OK";

        case PVINV_OL_STATUS_ERR_PWM_CONFIG:
            return "PWM TIM8 config error";

        case PVINV_OL_STATUS_ERR_PWM_START:
            return "PWM TIM8 start error";

        case PVINV_OL_STATUS_ERR_PWM_UPDATE_RATE:
            return "PWM update rate invalid";

        case PVINV_OL_STATUS_ERR_PWM_SET_D:
            return "PWM SetUnipolarD error";

        default:
            return "Unknown open-loop SPWM status";
    }
}
