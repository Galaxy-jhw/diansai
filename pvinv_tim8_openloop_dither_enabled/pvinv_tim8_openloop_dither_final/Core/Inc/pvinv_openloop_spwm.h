#ifndef __PVINV_OPENLOOP_SPWM_H__
#define __PVINV_OPENLOOP_SPWM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_pwm_tim8.h"

/*
 * 开环SPWM控制层
 *
 * 本模块负责：
 * 1.产生theta；
 * 2.产生D=M*sin(theta)；
 * 3.实现调制比软启动；
 * 4.调用PVINV_PWM_TIM8_SetUnipolarD(D)更新TIM8 CCR1/CCR2。
 *
 * 本模块不直接生成CubeMX初始化代码。
 */

typedef enum
{
    PVINV_OL_STATE_IDLE = 0,
    PVINV_OL_STATE_RAMP,
    PVINV_OL_STATE_RUN,
    PVINV_OL_STATE_FAULT
} PVINV_OL_State_t;

typedef enum
{
    PVINV_OL_STATUS_OK = 0,
    PVINV_OL_STATUS_ERR_PWM_CONFIG,
    PVINV_OL_STATUS_ERR_PWM_START,
    PVINV_OL_STATUS_ERR_PWM_UPDATE_RATE,
    PVINV_OL_STATUS_ERR_PWM_SET_D
} PVINV_OL_Status_t;

PVINV_OL_Status_t PVINV_OpenLoopSPWM_Init(void);
PVINV_OL_Status_t PVINV_OpenLoopSPWM_Start(void);
void PVINV_OpenLoopSPWM_Stop(void);

void PVINV_OpenLoopSPWM_SetSineFrequencyHz(float sine_hz);
void PVINV_OpenLoopSPWM_SetTargetModulation(float modulation);

/*
 * HAL回调方式：
 * 在HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)中调用。
 */
void PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/*
 * 直接中断方式：
 * 如果你不用HAL回调，也可以在TIM8更新中断中直接调用。
 */
void PVINV_OpenLoopSPWM_OnTim8UpdateIrq(void);

PVINV_OL_State_t PVINV_OpenLoopSPWM_GetState(void);
PVINV_OL_Status_t PVINV_OpenLoopSPWM_GetLastStatus(void);

float PVINV_OpenLoopSPWM_GetThetaRad(void);
float PVINV_OpenLoopSPWM_GetSineFrequencyHz(void);
float PVINV_OpenLoopSPWM_GetModulationNow(void);
float PVINV_OpenLoopSPWM_GetModulationTarget(void);
float PVINV_OpenLoopSPWM_GetPhaseStepRad(void);

const char *PVINV_OpenLoopSPWM_StatusToString(PVINV_OL_Status_t status);

#ifdef __cplusplus
}
#endif

#endif
