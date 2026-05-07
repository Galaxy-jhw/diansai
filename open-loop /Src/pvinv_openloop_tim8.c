#include "pvinv_openloop_tim8.h"
#include "tim.h"
#include <math.h>

#define PVOL_TWO_PI              (6.28318530717958647692f)
#define PVOL_MIN_ARR             (100u)
#define PVOL_MIN_SINE_HZ         (0.0f)
#define PVOL_MAX_SINE_HZ         (1000.0f)

static volatile PVOL_State_t g_state = PVOL_STATE_IDLE;
static volatile PVOL_Status_t g_last_status = PVOL_STATUS_OK;

static volatile float g_tim8_clk_hz = 0.0f;
static volatile float g_carrier_hz = 0.0f;
static volatile float g_update_irq_hz = 0.0f;

static volatile float g_sine_hz = PVOL_DEFAULT_SINE_HZ;
static volatile float g_phase_step_rad = 0.0f;
static volatile float g_theta_rad = 0.0f;

static volatile float g_mod_target = PVOL_DEFAULT_MODULATION;
static volatile float g_mod_now = 0.0f;
static volatile float g_last_d = 0.0f;

static volatile uint32_t g_last_ccr1 = 0u;
static volatile uint32_t g_last_ccr2 = 0u;

static volatile uint32_t g_psc = 0u;
static volatile uint32_t g_arr = 0u;
static volatile uint32_t g_rcr = 0u;
static volatile uint32_t g_deadtime_code = 0u;
static volatile uint32_t g_dithering_enabled = 0u;

static float PVOL_ClampFloat(float x, float min_v, float max_v)
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

static uint32_t PVOL_EnterCritical(void)
{
    uint32_t primask;

    primask = __get_PRIMASK();
    __disable_irq();

    return primask;
}

static void PVOL_ExitCritical(uint32_t primask)
{
    if (primask == 0u)
    {
        __enable_irq();
    }
}

/*
 * 获取TIM8时钟的工程估算值。
 *
 * 你的当前工程APB2=HCLK/1，因此TIM8_CLK约等于PCLK2。
 * 若以后APB2分频不是1，常见STM32定时器时钟通常为2*PCLK。
 *
 * 注意：最终仍以示波器实测PWM频率为准。
 */
static float PVOL_GetTim8ClockHz(void)
{
    uint32_t hclk;
    uint32_t pclk2;
    uint32_t timclk;

    hclk = HAL_RCC_GetHCLKFreq();
    pclk2 = HAL_RCC_GetPCLK2Freq();

    if (pclk2 == hclk)
    {
        timclk = pclk2;
    }
    else
    {
        timclk = pclk2 * 2u;
    }

    return (float)timclk;
}

static void PVOL_UpdatePhaseStep(void)
{
    float update_hz;
    float sine_hz;

    update_hz = g_update_irq_hz;
    sine_hz = g_sine_hz;

    if (update_hz < 1.0f)
    {
        g_phase_step_rad = 0.0f;
        return;
    }

    sine_hz = PVOL_ClampFloat(sine_hz, PVOL_MIN_SINE_HZ, PVOL_MAX_SINE_HZ);
    g_phase_step_rad = PVOL_TWO_PI * sine_hz / update_hz;
}

PVOL_Status_t PVOL_Init(void)
{
    uint32_t primask;

    primask = PVOL_EnterCritical();

    g_state = PVOL_STATE_IDLE;
    g_last_status = PVOL_STATUS_OK;

    g_tim8_clk_hz = 0.0f;
    g_carrier_hz = 0.0f;
    g_update_irq_hz = 0.0f;

    g_sine_hz = PVOL_DEFAULT_SINE_HZ;
    g_phase_step_rad = 0.0f;
    g_theta_rad = 0.0f;

    g_mod_target = PVOL_DEFAULT_MODULATION;
    g_mod_now = 0.0f;
    g_last_d = 0.0f;

    g_last_ccr1 = 0u;
    g_last_ccr2 = 0u;

    g_psc = PVOL_PWM_TIM->PSC;
    g_arr = PVOL_PWM_TIM->ARR;
    g_rcr = PVOL_PWM_TIM->RCR;
    g_deadtime_code = PVOL_PWM_TIM->BDTR & TIM_BDTR_DTG;

#if defined(TIM_CR1_DITHEN)
    g_dithering_enabled = ((PVOL_PWM_TIM->CR1 & TIM_CR1_DITHEN) != 0u) ? 1u : 0u;
#else
    g_dithering_enabled = 0u;
#endif

    PVOL_ExitCritical(primask);

    PVOL_ForceNeutralDuty();

    return PVOL_STATUS_OK;
}

PVOL_Status_t PVOL_CheckCubeMXConfig(void)
{
    float timclk;
    float carrier_hz;
    float raw_update_hz;
    float update_irq_hz;
    uint32_t psc;
    uint32_t arr;
    uint32_t rcr;
    uint32_t cms;
    uint32_t dtg;
    uint32_t dithering;

    timclk = PVOL_GetTim8ClockHz();

    psc = PVOL_PWM_TIM->PSC;
    arr = PVOL_PWM_TIM->ARR;
    rcr = PVOL_PWM_TIM->RCR;

    cms = PVOL_PWM_TIM->CR1 & TIM_CR1_CMS;

    if (cms == 0u)
    {
        g_last_status = PVOL_STATUS_ERR_NOT_CENTER_ALIGNED;
        return g_last_status;
    }

#if defined(TIM_CR1_DITHEN)
    dithering = ((PVOL_PWM_TIM->CR1 & TIM_CR1_DITHEN) != 0u) ? 1u : 0u;
#else
    dithering = 0u;
#endif

    if ((PVOL_REJECT_DITHERING != 0u) && (dithering != 0u))
    {
        g_last_status = PVOL_STATUS_ERR_DITHERING_ENABLED;
        return g_last_status;
    }

    dtg = PVOL_PWM_TIM->BDTR & TIM_BDTR_DTG;

    if ((dtg == 0u) && (PVOL_ALLOW_ZERO_DEADTIME_LOGIC_ONLY == 0u))
    {
        g_last_status = PVOL_STATUS_ERR_ZERO_DEADTIME;
        return g_last_status;
    }

    if (arr < PVOL_MIN_ARR)
    {
        g_last_status = PVOL_STATUS_ERR_ARR_TOO_SMALL;
        return g_last_status;
    }

    /*
     * 中心对齐PWM载波频率：
     * fPWM = TIM8_CLK / [2 * (PSC+1) * (ARR+1)]
     */
    carrier_hz = timclk / (2.0f * ((float)psc + 1.0f) * ((float)arr + 1.0f));

    if (carrier_hz < PVOL_CARRIER_MIN_HZ)
    {
        g_last_status = PVOL_STATUS_ERR_CARRIER_TOO_LOW;
        return g_last_status;
    }

    if (carrier_hz > PVOL_CARRIER_MAX_HZ)
    {
        g_last_status = PVOL_STATUS_ERR_CARRIER_TOO_HIGH;
        return g_last_status;
    }

    /*
     * 中心对齐模式下，原始Update事件通常可能出现在上溢和下溢；
     * RepetitionCounter会进一步分频。
     *
     * 估算：
     * raw_update_hz = TIM8_CLK / [(PSC+1) * (ARR+1)]
     * update_irq_hz = raw_update_hz / (RCR+1)
     *
     * 例如：
     * TIM8_CLK≈250MHz, PSC=1, ARR≈3124, RCR=1
     * carrier≈20kHz, update_irq≈20kHz
     */
    raw_update_hz = timclk / (((float)psc + 1.0f) * ((float)arr + 1.0f));
    update_irq_hz = raw_update_hz / ((float)rcr + 1.0f);

    if ((update_irq_hz < PVOL_UPDATE_MIN_HZ) || (update_irq_hz > PVOL_UPDATE_MAX_HZ))
    {
        g_last_status = PVOL_STATUS_ERR_UPDATE_RATE_INVALID;
        return g_last_status;
    }

    g_tim8_clk_hz = timclk;
    g_carrier_hz = carrier_hz;
    g_update_irq_hz = update_irq_hz;

    g_psc = psc;
    g_arr = arr;
    g_rcr = rcr;
    g_deadtime_code = dtg;
    g_dithering_enabled = dithering;

    PVOL_UpdatePhaseStep();

    g_last_status = PVOL_STATUS_OK;
    return PVOL_STATUS_OK;
}

void PVOL_SetSineFrequencyHz(float sine_hz)
{
    uint32_t primask;

    sine_hz = PVOL_ClampFloat(sine_hz, PVOL_MIN_SINE_HZ, PVOL_MAX_SINE_HZ);

    primask = PVOL_EnterCritical();
    g_sine_hz = sine_hz;
    PVOL_UpdatePhaseStep();
    PVOL_ExitCritical(primask);
}

void PVOL_SetTargetModulation(float modulation)
{
    uint32_t primask;

    modulation = PVOL_ClampFloat(modulation, 0.0f, PVOL_D_LIMIT);

    primask = PVOL_EnterCritical();
    g_mod_target = modulation;
    PVOL_ExitCritical(primask);
}

void PVOL_ForceNeutralDuty(void)
{
    uint32_t arr;
    uint32_t center;

    arr = PVOL_PWM_TIM->ARR;

    if (arr < 2u)
    {
        center = 0u;
    }
    else
    {
        center = (arr + 1u) / 2u;
    }

    PVOL_PWM_TIM->CCR1 = center;
    PVOL_PWM_TIM->CCR2 = center;

    g_last_d = 0.0f;
    g_last_ccr1 = center;
    g_last_ccr2 = center;
}

PVOL_Status_t PVOL_Start(void)
{
    PVOL_Status_t status;

    status = PVOL_CheckCubeMXConfig();
    if (status != PVOL_STATUS_OK)
    {
        g_state = PVOL_STATE_FAULT;
        PVOL_ForceNeutralDuty();
        return status;
    }

    PVOL_ForceNeutralDuty();

    /*
     * 启动主通道CH1和互补通道CH1N。
     * 只启动HAL_TIM_PWM_Start是不够的。
     */
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
    {
        g_state = PVOL_STATE_FAULT;
        g_last_status = PVOL_STATUS_ERR_HAL_START_CH1;
        return g_last_status;
    }

    if (HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
    {
        g_state = PVOL_STATE_FAULT;
        g_last_status = PVOL_STATUS_ERR_HAL_START_CH1N;
        return g_last_status;
    }

    /*
     * 启动主通道CH2和互补通道CH2N。
     */
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
    {
        g_state = PVOL_STATE_FAULT;
        g_last_status = PVOL_STATUS_ERR_HAL_START_CH2;
        return g_last_status;
    }

    if (HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
    {
        g_state = PVOL_STATE_FAULT;
        g_last_status = PVOL_STATUS_ERR_HAL_START_CH2N;
        return g_last_status;
    }

    /*
     * 不生成TIM8 IRQ函数。
     * CubeMX必须已经在NVIC里开启TIM8 Update interrupt。
     * 这里仅开启TIM8 Update中断源。
     */
    __HAL_TIM_CLEAR_FLAG(&htim8, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim8, TIM_IT_UPDATE);

    /*
     * 高级定时器主输出使能。
     * HAL_TIM_PWM_Start通常会处理，这里明确打开一次。
     */
    __HAL_TIM_MOE_ENABLE(&htim8);

    g_theta_rad = 0.0f;
    g_mod_now = 0.0f;
    g_last_d = 0.0f;

    g_state = PVOL_STATE_RAMP;
    g_last_status = PVOL_STATUS_OK;

    return PVOL_STATUS_OK;
}

void PVOL_Stop(void)
{
    g_state = PVOL_STATE_IDLE;

    __HAL_TIM_DISABLE_IT(&htim8, TIM_IT_UPDATE);

    g_mod_now = 0.0f;
    g_mod_target = 0.0f;
    g_last_d = 0.0f;

    PVOL_ForceNeutralDuty();

    (void)HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);

    (void)HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);

    __HAL_TIM_MOE_DISABLE(&htim8);
}

void PVOL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return;
    }

    if (htim->Instance == TIM8)
    {
        PVOL_OnTim8UpdateIrq();
    }
}

void PVOL_OnTim8UpdateIrq(void)
{
    float theta;
    float phase_step;
    float mod_now;
    float mod_target;
    float ramp_step;
    float d;

    uint32_t arr;
    float period;
    float center;
    float ccr1_f;
    float ccr2_f;
    uint32_t ccr1;
    uint32_t ccr2;

    if ((g_state != PVOL_STATE_RAMP) && (g_state != PVOL_STATE_RUN))
    {
        PVOL_ForceNeutralDuty();
        return;
    }

    /*
     * 调制比软启动。
     * 目标：M从0缓慢增加到g_mod_target，避免一启动就阶跃。
     */
    mod_now = g_mod_now;
    mod_target = g_mod_target;

    if (g_update_irq_hz > 1.0f)
    {
        ramp_step = PVOL_MOD_RAMP_PER_SEC / g_update_irq_hz;
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

    if ((g_state == PVOL_STATE_RAMP) && (mod_now >= mod_target))
    {
        g_state = PVOL_STATE_RUN;
    }

    /*
     * 相位累加：
     * theta += 2*pi*f_sine/f_update
     */
    theta = g_theta_rad;
    phase_step = g_phase_step_rad;

    theta += phase_step;

    if (theta >= PVOL_TWO_PI)
    {
        theta -= PVOL_TWO_PI;
    }

    g_theta_rad = theta;

    /*
     * 开环调制：
     * D = M * sin(theta)
     */
    d = mod_now * sinf(theta);
    d = PVOL_ClampFloat(d, -PVOL_D_LIMIT, PVOL_D_LIMIT);

    /*
     * 单极性SPWM核心：
     * A桥臂：CCR1 = center + center * D
     * B桥臂：CCR2 = center - center * D
     *
     * D>0：CCR1增大，CCR2减小
     * D<0：CCR1减小，CCR2增大
     */
    arr = PVOL_PWM_TIM->ARR;

    if (arr < 2u)
    {
        PVOL_ForceNeutralDuty();
        return;
    }

    period = (float)(arr + 1u);
    center = 0.5f * period;

    ccr1_f = center + center * d;
    ccr2_f = center - center * d;

    ccr1_f = PVOL_ClampFloat(ccr1_f, 1.0f, (float)(arr - 1u));
    ccr2_f = PVOL_ClampFloat(ccr2_f, 1.0f, (float)(arr - 1u));

    ccr1 = (uint32_t)(ccr1_f + 0.5f);
    ccr2 = (uint32_t)(ccr2_f + 0.5f);

    PVOL_PWM_TIM->CCR1 = ccr1;
    PVOL_PWM_TIM->CCR2 = ccr2;

    g_last_d = d;
    g_last_ccr1 = ccr1;
    g_last_ccr2 = ccr2;
}

PVOL_State_t PVOL_GetState(void)
{
    return g_state;
}

PVOL_Diag_t PVOL_GetDiag(void)
{
    PVOL_Diag_t diag;

    diag.tim8_clk_hz = g_tim8_clk_hz;
    diag.carrier_hz = g_carrier_hz;
    diag.update_irq_hz = g_update_irq_hz;
    diag.sine_hz = g_sine_hz;
    diag.phase_step_rad = g_phase_step_rad;

    diag.psc = g_psc;
    diag.arr = g_arr;
    diag.rcr = g_rcr;
    diag.deadtime_code = g_deadtime_code;
    diag.dithering_enabled = g_dithering_enabled;

    return diag;
}

PVOL_Status_t PVOL_GetLastStatus(void)
{
    return g_last_status;
}

float PVOL_GetThetaRad(void)
{
    return g_theta_rad;
}

float PVOL_GetModulationNow(void)
{
    return g_mod_now;
}

float PVOL_GetModulationTarget(void)
{
    return g_mod_target;
}

float PVOL_GetLastD(void)
{
    return g_last_d;
}

uint32_t PVOL_GetLastCCR1(void)
{
    return g_last_ccr1;
}

uint32_t PVOL_GetLastCCR2(void)
{
    return g_last_ccr2;
}

const char *PVOL_StatusToString(PVOL_Status_t status)
{
    switch (status)
    {
        case PVOL_STATUS_OK:
            return "OK";

        case PVOL_STATUS_ERR_NOT_CENTER_ALIGNED:
            return "TIM8 is not center-aligned";

        case PVOL_STATUS_ERR_DITHERING_ENABLED:
            return "TIM8 dithering is enabled; disable it in CubeMX";

        case PVOL_STATUS_ERR_ZERO_DEADTIME:
            return "TIM8 dead time is zero; set non-zero dead time in CubeMX";

        case PVOL_STATUS_ERR_ARR_TOO_SMALL:
            return "TIM8 ARR is too small";

        case PVOL_STATUS_ERR_CARRIER_TOO_LOW:
            return "TIM8 PWM carrier frequency is too low";

        case PVOL_STATUS_ERR_CARRIER_TOO_HIGH:
            return "TIM8 PWM carrier frequency is too high";

        case PVOL_STATUS_ERR_UPDATE_RATE_INVALID:
            return "TIM8 update interrupt frequency is invalid";

        case PVOL_STATUS_ERR_HAL_START_CH1:
            return "HAL_TIM_PWM_Start CH1 failed";

        case PVOL_STATUS_ERR_HAL_START_CH1N:
            return "HAL_TIMEx_PWMN_Start CH1N failed";

        case PVOL_STATUS_ERR_HAL_START_CH2:
            return "HAL_TIM_PWM_Start CH2 failed";

        case PVOL_STATUS_ERR_HAL_START_CH2N:
            return "HAL_TIMEx_PWMN_Start CH2N failed";

        case PVOL_STATUS_ERR_NOT_READY:
            return "PVOL is not ready";

        default:
            return "Unknown PVOL status";
    }
}
