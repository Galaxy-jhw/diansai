#include "pvinv_pwm_tim8.h"
#include "pvinv_utils.h"
#include "tim.h"

static volatile PVINV_PWM_Status_t g_last_status = PVINV_PWM_OK;

static volatile float g_tim8_clk_hz = 0.0f;
static volatile float g_effective_period_ticks = 0.0f;
static volatile float g_ccr_period_reg = 0.0f;
static volatile float g_carrier_hz = 0.0f;
static volatile float g_raw_update_hz = 0.0f;
static volatile float g_control_hz = 0.0f;

static volatile uint32_t g_psc = 0u;
static volatile uint32_t g_arr_reg = 0u;
static volatile uint32_t g_rcr = 0u;
static volatile uint32_t g_deadtime_code = 0u;
static volatile uint32_t g_center_aligned = 0u;
static volatile uint32_t g_dithering_enabled = 0u;

static volatile uint32_t g_last_ccr1 = 0u;
static volatile uint32_t g_last_ccr2 = 0u;
static volatile float g_last_d = 0.0f;

static float PVINV_PWM_GetTim8ClockHz(void)
{
#if (PVINV_CFG_USE_MANUAL_TIM8_CLK_HZ != 0u)
    return PVINV_CFG_MANUAL_TIM8_CLK_HZ;
#else
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

    if (pclk2 == hclk)
    {
        return (float)pclk2;
    }

    return (float)(pclk2 * 2u);
#endif
}

static uint32_t PVINV_PWM_IsDitheringEnabled(void)
{
#if defined(TIM_CR1_DITHEN)
    return ((PVINV_PWM_TIM->CR1 & TIM_CR1_DITHEN) != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

static float PVINV_PWM_GetEffectivePeriodTicks(uint32_t arr_reg, uint32_t dithering_enabled)
{
    if (dithering_enabled != 0u)
    {
        return ((float)arr_reg) / PVINV_CFG_TIM8_DITHERING_SCALE;
    }

    return ((float)arr_reg) + 1.0f;
}

static float PVINV_PWM_GetCcrPeriodReg(uint32_t arr_reg)
{
    return (float)arr_reg;
}

PVINV_PWM_Status_t PVINV_PWM_TIM8_Init(void)
{
    g_last_status = PVINV_PWM_OK;

    PVINV_PWM_TIM8_ForceNeutral();

    g_psc = PVINV_PWM_TIM->PSC;
    g_arr_reg = PVINV_PWM_TIM->ARR;
    g_rcr = PVINV_PWM_TIM->RCR;
    g_deadtime_code = PVINV_PWM_TIM->BDTR & TIM_BDTR_DTG;
    g_center_aligned = ((PVINV_PWM_TIM->CR1 & TIM_CR1_CMS) != 0u) ? 1u : 0u;
    g_dithering_enabled = PVINV_PWM_IsDitheringEnabled();

    return PVINV_PWM_TIM8_CheckCubeMXConfig();
}

PVINV_PWM_Status_t PVINV_PWM_TIM8_CheckCubeMXConfig(void)
{
    float timclk;
    float effective_period_ticks;
    float ccr_period_reg;
    float carrier_hz;
    float raw_update_hz;
    float control_hz;

    uint32_t psc;
    uint32_t arr_reg;
    uint32_t rcr;
    uint32_t deadtime_code;
    uint32_t center_aligned;
    uint32_t dithering_enabled;

    timclk = PVINV_PWM_GetTim8ClockHz();

    psc = PVINV_PWM_TIM->PSC;
    arr_reg = PVINV_PWM_TIM->ARR;
    rcr = PVINV_PWM_TIM->RCR;
    deadtime_code = PVINV_PWM_TIM->BDTR & TIM_BDTR_DTG;
    center_aligned = ((PVINV_PWM_TIM->CR1 & TIM_CR1_CMS) != 0u) ? 1u : 0u;
    dithering_enabled = PVINV_PWM_IsDitheringEnabled();

    if (center_aligned == 0u)
    {
        g_last_status = PVINV_PWM_ERR_NOT_CENTER_ALIGNED;
        return g_last_status;
    }

    if ((dithering_enabled != 0u) && (PVINV_CFG_ALLOW_TIM8_DITHERING == 0u))
    {
        g_last_status = PVINV_PWM_ERR_DITHERING_NOT_ALLOWED;
        return g_last_status;
    }

    if ((deadtime_code == 0u) && (PVINV_CFG_ALLOW_ZERO_DEADTIME_LOGIC_ONLY == 0u))
    {
        g_last_status = PVINV_PWM_ERR_ZERO_DEADTIME;
        return g_last_status;
    }

    if (arr_reg < PVINV_CFG_MIN_ARR_REG)
    {
        g_last_status = PVINV_PWM_ERR_ARR_TOO_SMALL;
        return g_last_status;
    }

    effective_period_ticks = PVINV_PWM_GetEffectivePeriodTicks(arr_reg, dithering_enabled);
    ccr_period_reg = PVINV_PWM_GetCcrPeriodReg(arr_reg);

    if (effective_period_ticks < 1.0f)
    {
        g_last_status = PVINV_PWM_ERR_ARR_TOO_SMALL;
        return g_last_status;
    }

    carrier_hz = timclk / (2.0f * ((float)psc + 1.0f) * effective_period_ticks);

    if (carrier_hz < PVINV_CFG_CARRIER_MIN_HZ)
    {
        g_last_status = PVINV_PWM_ERR_CARRIER_TOO_LOW;
        return g_last_status;
    }

    if (carrier_hz > PVINV_CFG_CARRIER_MAX_HZ)
    {
        g_last_status = PVINV_PWM_ERR_CARRIER_TOO_HIGH;
        return g_last_status;
    }

#if (PVINV_CFG_USE_MANUAL_CONTROL_HZ != 0u)
    raw_update_hz = PVINV_CFG_MANUAL_CONTROL_HZ;
    control_hz = PVINV_CFG_MANUAL_CONTROL_HZ;
#else
    raw_update_hz = timclk / (((float)psc + 1.0f) * effective_period_ticks);
    control_hz = raw_update_hz / ((float)rcr + 1.0f);
#endif

    if ((control_hz < PVINV_CFG_UPDATE_MIN_HZ) ||
        (control_hz > PVINV_CFG_UPDATE_MAX_HZ))
    {
        g_last_status = PVINV_PWM_ERR_UPDATE_RATE_INVALID;
        return g_last_status;
    }

    g_tim8_clk_hz = timclk;
    g_effective_period_ticks = effective_period_ticks;
    g_ccr_period_reg = ccr_period_reg;
    g_carrier_hz = carrier_hz;
    g_raw_update_hz = raw_update_hz;
    g_control_hz = control_hz;

    g_psc = psc;
    g_arr_reg = arr_reg;
    g_rcr = rcr;
    g_deadtime_code = deadtime_code;
    g_center_aligned = center_aligned;
    g_dithering_enabled = dithering_enabled;

    g_last_status = PVINV_PWM_OK;
    return g_last_status;
}

PVINV_PWM_Status_t PVINV_PWM_TIM8_StartOutputs(void)
{
    PVINV_PWM_Status_t status = PVINV_PWM_TIM8_CheckCubeMXConfig();
    if (status != PVINV_PWM_OK)
    {
        PVINV_PWM_TIM8_ForceNeutral();
        return status;
    }

    PVINV_PWM_TIM8_ForceNeutral();

    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
    {
        g_last_status = PVINV_PWM_ERR_HAL_START_CH1;
        return g_last_status;
    }

    if (HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1) != HAL_OK)
    {
        g_last_status = PVINV_PWM_ERR_HAL_START_CH1N;
        return g_last_status;
    }

    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
    {
        g_last_status = PVINV_PWM_ERR_HAL_START_CH2;
        return g_last_status;
    }

    if (HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_2) != HAL_OK)
    {
        g_last_status = PVINV_PWM_ERR_HAL_START_CH2N;
        return g_last_status;
    }

    __HAL_TIM_MOE_ENABLE(&htim8);

    g_last_status = PVINV_PWM_OK;
    return g_last_status;
}

void PVINV_PWM_TIM8_StopOutputs(void)
{
    PVINV_PWM_TIM8_DisableUpdateInterrupt();

    PVINV_PWM_TIM8_ForceNeutral();

    (void)HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_1);

    (void)HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_2);

    __HAL_TIM_MOE_DISABLE(&htim8);
}

void PVINV_PWM_TIM8_EmergencyShutdownFast(void)
{
    PVINV_PWM_TIM8_DisableUpdateInterrupt();
    PVINV_PWM_TIM8_ForceNeutral();
    __HAL_TIM_MOE_DISABLE(&htim8);
}

void PVINV_PWM_TIM8_ForceNeutral(void)
{
    uint32_t arr_reg = PVINV_PWM_TIM->ARR;
    uint32_t center = (arr_reg < 2u) ? 0u : (arr_reg / 2u);

    PVINV_PWM_TIM->CCR1 = center;
    PVINV_PWM_TIM->CCR2 = center;

    g_last_ccr1 = center;
    g_last_ccr2 = center;
    g_last_d = 0.0f;
}

PVINV_PWM_Status_t PVINV_PWM_TIM8_SetUnipolarD(float d)
{
    uint32_t arr_reg;
    uint32_t ccr1;
    uint32_t ccr2;
    float center;
    float ccr1_f;
    float ccr2_f;

    d = PVINV_ClampFloat(d, -PVINV_CFG_D_LIMIT, PVINV_CFG_D_LIMIT);

    arr_reg = PVINV_PWM_TIM->ARR;
    if (arr_reg < 2u)
    {
        g_last_status = PVINV_PWM_ERR_ARR_TOO_SMALL;
        return g_last_status;
    }

    /*
     * Dithering开启时，ARR和CCR都使用扩展寄存器单位。
     * 因此CCR写入使用ARR寄存器单位，不除以Dithering scale。
     */
    center = 0.5f * (float)arr_reg;

    ccr1_f = center + center * d;
    ccr2_f = center - center * d;

    ccr1_f = PVINV_ClampFloat(ccr1_f, 1.0f, (float)(arr_reg - 1u));
    ccr2_f = PVINV_ClampFloat(ccr2_f, 1.0f, (float)(arr_reg - 1u));

    ccr1 = (uint32_t)(ccr1_f + 0.5f);
    ccr2 = (uint32_t)(ccr2_f + 0.5f);

    PVINV_PWM_TIM->CCR1 = ccr1;
    PVINV_PWM_TIM->CCR2 = ccr2;

    g_last_ccr1 = ccr1;
    g_last_ccr2 = ccr2;
    g_last_d = d;

    g_last_status = PVINV_PWM_OK;
    return g_last_status;
}

void PVINV_PWM_TIM8_EnableUpdateInterrupt(void)
{
    __HAL_TIM_CLEAR_FLAG(&htim8, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim8, TIM_IT_UPDATE);
}

void PVINV_PWM_TIM8_DisableUpdateInterrupt(void)
{
    __HAL_TIM_DISABLE_IT(&htim8, TIM_IT_UPDATE);
}

PVINV_PWM_Diag_t PVINV_PWM_TIM8_GetDiag(void)
{
    PVINV_PWM_Diag_t diag;

    diag.tim8_clk_hz = g_tim8_clk_hz;
    diag.effective_period_ticks = g_effective_period_ticks;
    diag.ccr_period_reg = g_ccr_period_reg;
    diag.carrier_hz = g_carrier_hz;
    diag.raw_update_hz = g_raw_update_hz;
    diag.control_hz = g_control_hz;

    diag.psc = g_psc;
    diag.arr_reg = g_arr_reg;
    diag.rcr = g_rcr;
    diag.deadtime_code = g_deadtime_code;
    diag.center_aligned = g_center_aligned;
    diag.dithering_enabled = g_dithering_enabled;

    return diag;
}

PVINV_PWM_Status_t PVINV_PWM_TIM8_GetLastStatus(void)
{
    return g_last_status;
}

float PVINV_PWM_TIM8_GetControlHz(void)
{
    return g_control_hz;
}

uint32_t PVINV_PWM_TIM8_GetLastCCR1(void)
{
    return g_last_ccr1;
}

uint32_t PVINV_PWM_TIM8_GetLastCCR2(void)
{
    return g_last_ccr2;
}

float PVINV_PWM_TIM8_GetLastD(void)
{
    return g_last_d;
}

const char *PVINV_PWM_StatusToString(PVINV_PWM_Status_t status)
{
    switch (status)
    {
        case PVINV_PWM_OK: return "OK";
        case PVINV_PWM_ERR_NOT_CENTER_ALIGNED: return "TIM8 is not center-aligned";
        case PVINV_PWM_ERR_DITHERING_NOT_ALLOWED: return "TIM8 dithering is not allowed";
        case PVINV_PWM_ERR_ZERO_DEADTIME: return "TIM8 dead time is zero";
        case PVINV_PWM_ERR_ARR_TOO_SMALL: return "TIM8 ARR is too small";
        case PVINV_PWM_ERR_CARRIER_TOO_LOW: return "TIM8 carrier is too low";
        case PVINV_PWM_ERR_CARRIER_TOO_HIGH: return "TIM8 carrier is too high";
        case PVINV_PWM_ERR_UPDATE_RATE_INVALID: return "TIM8 control/update rate is invalid";
        case PVINV_PWM_ERR_HAL_START_CH1: return "HAL_TIM_PWM_Start CH1 failed";
        case PVINV_PWM_ERR_HAL_START_CH1N: return "HAL_TIMEx_PWMN_Start CH1N failed";
        case PVINV_PWM_ERR_HAL_START_CH2: return "HAL_TIM_PWM_Start CH2 failed";
        case PVINV_PWM_ERR_HAL_START_CH2N: return "HAL_TIMEx_PWMN_Start CH2N failed";
        default: return "Unknown PWM status";
    }
}
