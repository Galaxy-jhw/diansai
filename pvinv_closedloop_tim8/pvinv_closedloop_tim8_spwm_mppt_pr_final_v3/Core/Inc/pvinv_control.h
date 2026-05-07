#ifndef __PVINV_CONTROL_H__
#define __PVINV_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_pwm_tim8.h"
#include "pvinv_adc.h"
#include "pvinv_mppt_inccond.h"
#include "pvinv_pr.h"

typedef enum
{
    PVINV_STATE_IDLE = 0,
    PVINV_STATE_READY,
    PVINV_STATE_ADC_WARMUP,
    PVINV_STATE_SOFTSTART,
    PVINV_STATE_CLOSED_LOOP,
    PVINV_STATE_FAULT
} PVINV_State_t;

typedef enum
{
    PVINV_FAULT_NONE = 0,
    PVINV_FAULT_PWM_CONFIG,
    PVINV_FAULT_ADC_START,
    PVINV_FAULT_UNDERVOLTAGE,
    PVINV_FAULT_OVERVOLTAGE,
    PVINV_FAULT_INPUT_OVERCURRENT,
    PVINV_FAULT_OUTPUT_OVERCURRENT,
    PVINV_FAULT_OUTPUT_OVERVOLTAGE,
    PVINV_FAULT_PWM_UPDATE
} PVINV_Fault_t;

typedef enum
{
    PVINV_OK = 0,
    PVINV_ERR_PWM,
    PVINV_ERR_ADC,
    PVINV_ERR_NOT_READY
} PVINV_Status_t;

typedef struct
{
    PVINV_State_t state;
    PVINV_Fault_t fault;

    float control_hz;
    float control_ts;
    float softstart_gain;

    uint32_t adc_warmup_count;

    PVINV_ADC_Meas_t meas;
    PVINV_PWM_Diag_t pwm;

    float ref_unit;
    float mppt_v_ref;
    float i_amp_cmd;
    float i_amp_soft;
    float i_ref;
    float i_err;
    float pr_out;
    float v_ff;
    float duty_d;
} PVINV_Diag_t;

PVINV_Status_t PVINV_Control_Init(void);
PVINV_Status_t PVINV_Control_Start(void);
void PVINV_Control_Stop(void);
void PVINV_Control_ClearFault(void);

/*
 * 可在main while(1)中周期调用。
 * 当前主要用于FAULT后的HAL Stop清理。
 */
void PVINV_Control_Service(void);

void PVINV_Control_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void PVINV_Control_OnTim8UpdateIrq(void);

PVINV_State_t PVINV_Control_GetState(void);
PVINV_Fault_t PVINV_Control_GetFault(void);
PVINV_Diag_t PVINV_Control_GetDiag(void);

const char *PVINV_StateToString(PVINV_State_t state);
const char *PVINV_FaultToString(PVINV_Fault_t fault);

#ifdef __cplusplus
}
#endif

#endif
