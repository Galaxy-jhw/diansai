#ifndef __PVINV_OPENLOOP_LL_H__
#define __PVINV_OPENLOOP_LL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * PVINV Open-loop SPWM module for STM32H563 + TIM8.
 * 作用：只做开环单极性SPWM发波，用于上板初期验证TIM8互补PWM、死区、驱动链路。
 * 不包含MPPT、不包含ADC、不包含电流闭环、不包含并网闭环。
 */

#ifndef PVOL_PWM_TIM
#define PVOL_PWM_TIM                         TIM8
#endif

/* 控制ISR频率：必须与你实际调用PVOL_ControlISR()的频率一致。 */
#ifndef PVOL_CTRL_FREQ_HZ
#define PVOL_CTRL_FREQ_HZ                    20000.0f
#endif

/* 开环输出正弦频率，题目给45Hz~55Hz，这里默认50Hz，可运行时修改。 */
#ifndef PVOL_DEFAULT_REF_FREQ_HZ
#define PVOL_DEFAULT_REF_FREQ_HZ             50.0f
#endif

/* 初始调制度。上板初期务必很小，建议0.02~0.05。 */
#ifndef PVOL_DEFAULT_TARGET_MODULATION
#define PVOL_DEFAULT_TARGET_MODULATION       0.03f
#endif

/* 允许的最大调制度。上板初期不要太大。 */
#ifndef PVOL_MAX_MODULATION
#define PVOL_MAX_MODULATION                  0.20f
#endif

/* 软启动时间，单位秒。 */
#ifndef PVOL_SOFTSTART_TIME_S
#define PVOL_SOFTSTART_TIME_S                3.0f
#endif

/* 是否优先使用testcore的固定中心值6784。
 * 若TIM8->ARR不足以容纳2*6784，代码会自动回退到ARR/2。
 */
#ifndef PVOL_PWM_USE_FIXED_CENTER
#define PVOL_PWM_USE_FIXED_CENTER            1u
#endif

#ifndef PVOL_PWM_FIXED_CENTER_CCR
#define PVOL_PWM_FIXED_CENTER_CCR            6784u
#endif

/* TIM8 Update中断分频。
 * 如果TIM8 Update实际频率=20kHz，保持1。
 * 如果TIM8 Update实际频率=40kHz，设为2。
 */
#ifndef PVOL_TIM8_UPDATE_ISR_DIV
#define PVOL_TIM8_UPDATE_ISR_DIV             1u
#endif

/* PI常数，避免依赖非标准M_PI。 */
#ifndef PVOL_PI_F
#define PVOL_PI_F                            3.14159265358979323846f
#endif

typedef enum
{
    PVOL_STATE_STOP = 0,
    PVOL_STATE_READY,
    PVOL_STATE_RUN,
    PVOL_STATE_FAULT_PARAM
} PVOL_State_t;

typedef struct
{
    PVOL_State_t state;

    float ref_freq_hz;
    float target_modulation;
    float modulation_cmd;
    float modulation_inst;
    float phase_rad;

    uint32_t isr_count;
    uint32_t tim8_update_count;

    uint32_t pwm_center_ccr;
    uint8_t pwm_center_fallback;

    uint32_t ccr1_last;
    uint32_t ccr2_last;

    uint8_t output_enabled;
} PVOL_Handle_t;

void PVOL_Init(void);
void PVOL_Start(void);
void PVOL_Stop(void);
void PVOL_ControlISR(void);
void PVOL_OnTim8UpdateIrq(void);

void PVOL_SetFrequencyHz(float freq_hz);
void PVOL_SetTargetModulation(float modulation);
const PVOL_Handle_t *PVOL_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* __PVINV_OPENLOOP_LL_H__ */
