#ifndef __PVINV_ADC_H__
#define __PVINV_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_project_config.h"
#include <stdint.h>

/*
 * ADC用户层采样模块。
 *
 * 不生成MX_ADC1_Init、MX_ADC2_Init、MX_GPDMA1_Init。
 */

typedef enum
{
    PVINV_ADC_OK = 0,
    PVINV_ADC_ERR_ADC1_START,
    PVINV_ADC_ERR_ADC2_START
} PVINV_ADC_Status_t;

typedef struct
{
    float ud;
    float id;
    float uref_v;
    float uref_norm;
    float iF;
    float uo;

    float ud_f;
    float id_f;
    float uref_norm_f;
    float iF_f;
    float uo_f;

    uint32_t adc1_raw[PVINV_ADC1_DMA_LEN];
    uint32_t adc2_raw[PVINV_ADC2_DMA_LEN];

    int32_t ud_code;
    int32_t id_code;
    int32_t iF_code;
    int32_t uo_code;
    uint32_t uref_code;

    uint32_t filter_initialized;
    uint32_t update_count;
} PVINV_ADC_Meas_t;

PVINV_ADC_Status_t PVINV_ADC_Init(void);
PVINV_ADC_Status_t PVINV_ADC_StartDMA(void);

void PVINV_ADC_ResetFilters(void);
void PVINV_ADC_Update(void);
PVINV_ADC_Meas_t PVINV_ADC_GetMeas(void);

volatile uint32_t *PVINV_ADC_GetAdc1Buffer(void);
volatile uint32_t *PVINV_ADC_GetAdc2Buffer(void);

#ifdef __cplusplus
}
#endif

#endif
