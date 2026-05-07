#ifndef __PVINV_OPENLOOP_TIM8_H__
#define __PVINV_OPENLOOP_TIM8_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * STM32H563RGT6 TIM8开环单极性SPWM用户层模块
 *
 * 注意：
 * 1.本模块不生成CubeMX可以生成的初始化代码。
 * 2.本模块不修改PSC/ARR/RCR/DeadTime/Dithering。
 * 3.本模块只检查CubeMX配置是否适合开环发波。
 * 4.本模块只做TIM8开环单极性SPWM，不做MPPT、不做PR闭环、不做ADC DMA。
 *
 * 目标PWM引脚：
 * TIM8_CH1  -> PC6 -> A桥臂上管
 * TIM8_CH1N -> PA7 -> A桥臂下管
 * TIM8_CH2  -> PC7 -> B桥臂上管
 * TIM8_CH2N -> PB0 -> B桥臂下管
 *
 * 单极性SPWM核心：
 * D = M * sin(theta)
 * CCR1 = center + center * D
 * CCR2 = center - center * D
 */

#define PVOL_PWM_TIM                         TIM8

/* 推荐PWM载波范围。最终必须用示波器确认。 */
#define PVOL_CARRIER_MIN_HZ                  (18000.0f)
#define PVOL_CARRIER_MAX_HZ                  (25000.0f)

/*
 * TIM8更新中断频率允许范围。
 * 中心对齐模式下，原始Update事件可能出现在上溢和下溢；
 * 再经过RepetitionCounter分频。
 * 所以这里不强行要求等于20kHz，而是允许一定范围。
 */
#define PVOL_UPDATE_MIN_HZ                   (15000.0f)
#define PVOL_UPDATE_MAX_HZ                   (50000.0f)

/* 默认正弦调制频率 */
#define PVOL_DEFAULT_SINE_HZ                 (50.0f)

/* 默认目标调制比。第一次上板建议0.02~0.05。 */
#define PVOL_DEFAULT_MODULATION              (0.03f)

/* 调制量D限幅，避免CCR贴边 */
#define PVOL_D_LIMIT                         (0.98f)

/* 调制比软启动速度：每秒增加多少调制比 */
#define PVOL_MOD_RAMP_PER_SEC                (0.20f)

/*
 * 是否允许DeadTime=0。
 * 0：不允许，安全；
 * 1：仅允许主控板空载测逻辑波形，绝不能接驱动板/功率桥。
 */
#define PVOL_ALLOW_ZERO_DEADTIME_LOGIC_ONLY  (0u)

/*
 * 上板初期必须关闭Dithering。
 * 你当前工程中Dithering开启并改写ARR=48000，会导致频率不直观。
 */
#define PVOL_REJECT_DITHERING                (1u)

typedef enum
{
    PVOL_STATUS_OK = 0,

    PVOL_STATUS_ERR_NOT_CENTER_ALIGNED,
    PVOL_STATUS_ERR_DITHERING_ENABLED,
    PVOL_STATUS_ERR_ZERO_DEADTIME,
    PVOL_STATUS_ERR_ARR_TOO_SMALL,
    PVOL_STATUS_ERR_CARRIER_TOO_LOW,
    PVOL_STATUS_ERR_CARRIER_TOO_HIGH,
    PVOL_STATUS_ERR_UPDATE_RATE_INVALID,

    PVOL_STATUS_ERR_HAL_START_CH1,
    PVOL_STATUS_ERR_HAL_START_CH1N,
    PVOL_STATUS_ERR_HAL_START_CH2,
    PVOL_STATUS_ERR_HAL_START_CH2N,

    PVOL_STATUS_ERR_NOT_READY
} PVOL_Status_t;

typedef enum
{
    PVOL_STATE_IDLE = 0,
    PVOL_STATE_RAMP,
    PVOL_STATE_RUN,
    PVOL_STATE_FAULT
} PVOL_State_t;

typedef struct
{
    float tim8_clk_hz;
    float carrier_hz;
    float update_irq_hz;
    float sine_hz;
    float phase_step_rad;

    uint32_t psc;
    uint32_t arr;
    uint32_t rcr;
    uint32_t deadtime_code;
    uint32_t dithering_enabled;
} PVOL_Diag_t;

PVOL_Status_t PVOL_Init(void);

/*
 * 只检查CubeMX生成的TIM8配置，不修改PSC/ARR/RCR。
 * 必须在MX_TIM8_Init()之后调用。
 */
PVOL_Status_t PVOL_CheckCubeMXConfig(void);

void PVOL_SetSineFrequencyHz(float sine_hz);
void PVOL_SetTargetModulation(float modulation);

PVOL_Status_t PVOL_Start(void);
void PVOL_Stop(void);

/*
 * 在HAL_TIM_PeriodElapsedCallback里调用本函数。
 * 不需要你手写TIM8_UP_IRQHandler。
 */
void PVOL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

/*
 * 如果你不用HAL回调，而是在TIM8更新中断里直接调用，也可以调用这个函数。
 */
void PVOL_OnTim8UpdateIrq(void);

void PVOL_ForceNeutralDuty(void);

PVOL_State_t PVOL_GetState(void);
PVOL_Diag_t PVOL_GetDiag(void);
PVOL_Status_t PVOL_GetLastStatus(void);

float PVOL_GetThetaRad(void);
float PVOL_GetModulationNow(void);
float PVOL_GetModulationTarget(void);
float PVOL_GetLastD(void);
uint32_t PVOL_GetLastCCR1(void);
uint32_t PVOL_GetLastCCR2(void);

const char *PVOL_StatusToString(PVOL_Status_t status);

#ifdef __cplusplus
}
#endif

#endif
