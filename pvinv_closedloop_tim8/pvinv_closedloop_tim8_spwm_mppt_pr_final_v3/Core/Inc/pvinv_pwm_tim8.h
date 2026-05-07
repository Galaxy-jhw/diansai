#ifndef __PVINV_PWM_TIM8_H__
#define __PVINV_PWM_TIM8_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_project_config.h"
#include <stdint.h>

/*
 * TIM8底层发波封装。
 *
 * 不生成MX_TIM8_Init、GPIO初始化、时钟初始化、中断入口。
 */

#define PVINV_PWM_TIM                                   TIM8

typedef enum
{
    PVINV_PWM_OK = 0,
    PVINV_PWM_ERR_NOT_CENTER_ALIGNED,
    PVINV_PWM_ERR_DITHERING_NOT_ALLOWED,
    PVINV_PWM_ERR_ZERO_DEADTIME,
    PVINV_PWM_ERR_ARR_TOO_SMALL,
    PVINV_PWM_ERR_CARRIER_TOO_LOW,
    PVINV_PWM_ERR_CARRIER_TOO_HIGH,
    PVINV_PWM_ERR_UPDATE_RATE_INVALID,
    PVINV_PWM_ERR_HAL_START_CH1,
    PVINV_PWM_ERR_HAL_START_CH1N,
    PVINV_PWM_ERR_HAL_START_CH2,
    PVINV_PWM_ERR_HAL_START_CH2N
} PVINV_PWM_Status_t;

typedef struct
{
    float tim8_clk_hz;
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
} PVINV_PWM_Diag_t;

PVINV_PWM_Status_t PVINV_PWM_TIM8_Init(void);
PVINV_PWM_Status_t PVINV_PWM_TIM8_CheckCubeMXConfig(void);

PVINV_PWM_Status_t PVINV_PWM_TIM8_StartOutputs(void);
void PVINV_PWM_TIM8_StopOutputs(void);

/*
 * ISR故障快速关断：不调用HAL Stop，只关中断、清MOE、回中点。
 * 适合在TIM8更新中断里使用。
 */
void PVINV_PWM_TIM8_EmergencyShutdownFast(void);

void PVINV_PWM_TIM8_ForceNeutral(void);

PVINV_PWM_Status_t PVINV_PWM_TIM8_SetUnipolarD(float d);

void PVINV_PWM_TIM8_EnableUpdateInterrupt(void);
void PVINV_PWM_TIM8_DisableUpdateInterrupt(void);

PVINV_PWM_Diag_t PVINV_PWM_TIM8_GetDiag(void);
PVINV_PWM_Status_t PVINV_PWM_TIM8_GetLastStatus(void);
float PVINV_PWM_TIM8_GetControlHz(void);

uint32_t PVINV_PWM_TIM8_GetLastCCR1(void);
uint32_t PVINV_PWM_TIM8_GetLastCCR2(void);
float PVINV_PWM_TIM8_GetLastD(void);

const char *PVINV_PWM_StatusToString(PVINV_PWM_Status_t status);

#ifdef __cplusplus
}
#endif

#endif
