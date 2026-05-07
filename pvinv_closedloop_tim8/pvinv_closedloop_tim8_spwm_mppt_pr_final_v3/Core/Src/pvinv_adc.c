#include "pvinv_adc.h"
#include "pvinv_utils.h"
#include "adc.h"

static volatile uint32_t g_adc1_raw[PVINV_ADC1_DMA_LEN];
static volatile uint32_t g_adc2_raw[PVINV_ADC2_DMA_LEN];

static PVINV_ADC_Meas_t g_meas;

static int32_t PVINV_ADC_DiffRawToSigned12(uint32_t raw)
{
    uint32_t code = raw & 0x0FFFu;

#if (PVINV_ADC_DIFF_TWOS_COMPLEMENT != 0u)
    if ((code & 0x0800u) != 0u)
    {
        return (int32_t)code - 4096;
    }

    return (int32_t)code;
#else
    return (int32_t)code - 2048;
#endif
}

static uint32_t PVINV_ADC_SingleRaw12(uint32_t raw)
{
    return raw & 0x0FFFu;
}

static float PVINV_ADC_DiffCodeToVolt(int32_t code)
{
    return ((float)code) * PVINV_ADC_VREF / PVINV_ADC_DIFF_FULL_SCALE_CODE;
}

static float PVINV_ADC_SingleCodeToVolt(uint32_t code)
{
    return ((float)code) * PVINV_ADC_VREF / PVINV_ADC_SINGLE_FULL_SCALE_CODE;
}

PVINV_ADC_Status_t PVINV_ADC_Init(void)
{
    uint32_t i;

    for (i = 0u; i < PVINV_ADC1_DMA_LEN; i++)
    {
        g_adc1_raw[i] = 0u;
        g_meas.adc1_raw[i] = 0u;
    }

    for (i = 0u; i < PVINV_ADC2_DMA_LEN; i++)
    {
        g_adc2_raw[i] = 0u;
        g_meas.adc2_raw[i] = 0u;
    }

    g_meas.ud = 0.0f;
    g_meas.id = 0.0f;
    g_meas.uref_v = 0.0f;
    g_meas.uref_norm = 0.0f;
    g_meas.iF = 0.0f;
    g_meas.uo = 0.0f;

    g_meas.ud_f = 0.0f;
    g_meas.id_f = 0.0f;
    g_meas.uref_norm_f = 0.0f;
    g_meas.iF_f = 0.0f;
    g_meas.uo_f = 0.0f;

    g_meas.filter_initialized = 0u;
    g_meas.update_count = 0u;

    return PVINV_ADC_OK;
}

PVINV_ADC_Status_t PVINV_ADC_StartDMA(void)
{
#if (PVINV_CFG_START_ADC_DMA_IN_USER_CODE != 0u)
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc1_raw, PVINV_ADC1_DMA_LEN) != HAL_OK)
    {
        return PVINV_ADC_ERR_ADC1_START;
    }

    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)g_adc2_raw, PVINV_ADC2_DMA_LEN) != HAL_OK)
    {
        return PVINV_ADC_ERR_ADC2_START;
    }
#endif

    return PVINV_ADC_OK;
}

void PVINV_ADC_ResetFilters(void)
{
    g_meas.ud_f = 0.0f;
    g_meas.id_f = 0.0f;
    g_meas.uref_norm_f = 0.0f;
    g_meas.iF_f = 0.0f;
    g_meas.uo_f = 0.0f;
    g_meas.filter_initialized = 0u;
    g_meas.update_count = 0u;
}

void PVINV_ADC_Update(void)
{
    uint32_t i;

    int32_t ud_code;
    int32_t id_code;
    int32_t iF_code;
    int32_t uo_code;
    uint32_t uref_code;

    float ud_adc_v;
    float id_adc_v;
    float iF_adc_v;
    float uo_adc_v;
    float uref_adc_v;

    for (i = 0u; i < PVINV_ADC1_DMA_LEN; i++)
    {
        g_meas.adc1_raw[i] = g_adc1_raw[i];
    }

    for (i = 0u; i < PVINV_ADC2_DMA_LEN; i++)
    {
        g_meas.adc2_raw[i] = g_adc2_raw[i];
    }

    ud_code = PVINV_ADC_DiffRawToSigned12(g_adc1_raw[PVINV_ADC1_IDX_UD]);
    id_code = PVINV_ADC_DiffRawToSigned12(g_adc1_raw[PVINV_ADC1_IDX_ID]);
    uref_code = PVINV_ADC_SingleRaw12(g_adc1_raw[PVINV_ADC1_IDX_UREF]);

    iF_code = PVINV_ADC_DiffRawToSigned12(g_adc2_raw[PVINV_ADC2_IDX_IF]);
    uo_code = PVINV_ADC_DiffRawToSigned12(g_adc2_raw[PVINV_ADC2_IDX_UO]);

    ud_adc_v = PVINV_ADC_DiffCodeToVolt(ud_code);
    id_adc_v = PVINV_ADC_DiffCodeToVolt(id_code);
    iF_adc_v = PVINV_ADC_DiffCodeToVolt(iF_code);
    uo_adc_v = PVINV_ADC_DiffCodeToVolt(uo_code);
    uref_adc_v = PVINV_ADC_SingleCodeToVolt(uref_code);

    g_meas.ud = PVINV_UD_SIGN * ud_adc_v * PVINV_UD_SCALE + PVINV_UD_OFFSET;
    g_meas.id = PVINV_ID_SIGN * id_adc_v * PVINV_ID_SCALE + PVINV_ID_OFFSET;
    g_meas.iF = PVINV_IF_SIGN * iF_adc_v * PVINV_IF_SCALE + PVINV_IF_OFFSET;
    g_meas.uo = PVINV_UO_SIGN * uo_adc_v * PVINV_UO_SCALE + PVINV_UO_OFFSET;

    g_meas.uref_v = uref_adc_v;
    g_meas.uref_norm = PVINV_UREF_SIGN * ((uref_adc_v - PVINV_UREF_BIAS_V) / PVINV_UREF_AMPLITUDE_V);
    g_meas.uref_norm = PVINV_ClampFloat(g_meas.uref_norm, -1.0f, 1.0f);

    if (g_meas.filter_initialized == 0u)
    {
        g_meas.ud_f = g_meas.ud;
        g_meas.id_f = g_meas.id;
        g_meas.uref_norm_f = g_meas.uref_norm;
        g_meas.iF_f = g_meas.iF;
        g_meas.uo_f = g_meas.uo;
        g_meas.filter_initialized = 1u;
    }
    else
    {
        g_meas.ud_f = PVINV_Lpf1(g_meas.ud_f, g_meas.ud, PVINV_LPF_ALPHA_UD);
        g_meas.id_f = PVINV_Lpf1(g_meas.id_f, g_meas.id, PVINV_LPF_ALPHA_ID);
        g_meas.uref_norm_f = PVINV_Lpf1(g_meas.uref_norm_f, g_meas.uref_norm, PVINV_LPF_ALPHA_UREF);
        g_meas.iF_f = PVINV_Lpf1(g_meas.iF_f, g_meas.iF, PVINV_LPF_ALPHA_IF);
        g_meas.uo_f = PVINV_Lpf1(g_meas.uo_f, g_meas.uo, PVINV_LPF_ALPHA_UO);
    }

    g_meas.ud_code = ud_code;
    g_meas.id_code = id_code;
    g_meas.iF_code = iF_code;
    g_meas.uo_code = uo_code;
    g_meas.uref_code = uref_code;

    g_meas.update_count++;
}

PVINV_ADC_Meas_t PVINV_ADC_GetMeas(void)
{
    return g_meas;
}

volatile uint32_t *PVINV_ADC_GetAdc1Buffer(void)
{
    return g_adc1_raw;
}

volatile uint32_t *PVINV_ADC_GetAdc2Buffer(void)
{
    return g_adc2_raw;
}
