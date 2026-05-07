#ifndef __PVINV_PWM_TIM8_H__
#define __PVINV_PWM_TIM8_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pvinv_project_config.h"
#include <stdint.h>

/*
 * STM32H563RGT6 TIM8底层发波封装
 *
 * 本模块只做用户层TIM8 PWM封装：
 * 1.检查CubeMX生成的TIM8配置；
 * 2.兼容TIM8 Dithering；
 * 3.启动CH1/CH1N、CH2/CH2N；
 * 4.提供单极性SPWM底层接口SetUnipolarD(D)。
 *
 * 本模块不生成：
 * 1.MX_TIM8_Init()
 * 2.MX_GPIO_Init()
 * 3.SystemClock_Config()
 * 4.HAL_TIM_MspPostInit()
 * 5.TIM8_UP_IRQHandler()
 *
 * 这些仍由CubeMX生成。
 *
 * 目标引脚：
 * TIM8_CH1  -> PC6 -> A桥臂上管
 * TIM8_CH1N -> PA7 -> A桥臂下管
 * TIM8_CH2  -> PC7 -> B桥臂上管
 * TIM8_CH2N -> PB0 -> B桥臂下管
 */

#define PVINV_PWM_TIM                                  TIM8

typedef enum
{
    PVINV_PWM_STATUS_OK = 0,

    PVINV_PWM_STATUS_ERR_NOT_CENTER_ALIGNED,
    PVINV_PWM_STATUS_ERR_DITHERING_NOT_ALLOWED,
    PVINV_PWM_STATUS_ERR_ZERO_DEADTIME,
    PVINV_PWM_STATUS_ERR_ARR_TOO_SMALL,
    PVINV_PWM_STATUS_ERR_CARRIER_TOO_LOW,
    PVINV_PWM_STATUS_ERR_CARRIER_TOO_HIGH,
    PVINV_PWM_STATUS_ERR_UPDATE_RATE_INVALID,

    PVINV_PWM_STATUS_ERR_HAL_START_CH1,
    PVINV_PWM_STATUS_ERR_HAL_START_CH1N,
    PVINV_PWM_STATUS_ERR_HAL_START_CH2,
    PVINV_PWM_STATUS_ERR_HAL_START_CH2N,

    PVINV_PWM_STATUS_ERR_HAL_STOP,
    PVINV_PWM_STATUS_ERR_NOT_READY
} PVINV_PWM_Status_t;

typedef struct
{
    float tim8_clk_hz;

    /*
     * 用于频率计算的等效周期tick：
     * 普通模式：ARR+1
     * Dithering模式：ARR/PVINV_CFG_TIM8_DITHERING_SCALE
     */
    float effective_period_ticks;

    /*
     * 用于CCR写入的寄存器周期：
     * 普通模式：ARR
     * Dithering模式：ARR
     *
     * 重要：
     * Dithering开启时，CCR必须和ARR寄存器单位保持一致。
     * 所以CCR写入不除以16。
     */
    float ccr_period_reg;

    float carrier_hz;
    float raw_update_hz;
    float update_irq_hz;

    uint32_t psc;
    uint32_t arr_reg;
    uint32_t rcr;
    uint32_t deadtime_code;
    uint32_t center_aligned;
    uint32_t dithering_enabled;
} PVINV_PWM_Diag_t;

PVINV_PWM_Status_t PVINV_PWM_TIM8_Init(void);

/*
 * 只检查CubeMX配置，不修改PSC/ARR/RCR/DeadTime/Dithering。
 * 必须在MX_TIM8_Init()之后调用。
 */
PVINV_PWM_Status_t PVINV_PWM_TIM8_CheckCubeMXConfig(void);

/*
 * 启动TIM8四路互补PWM输出：
 * CH1、CH1N、CH2、CH2N。
 */
PVINV_PWM_Status_t PVINV_PWM_TIM8_StartOutputs(void);

/*
 * 停止TIM8输出。
 */
void PVINV_PWM_TIM8_StopOutputs(void);

/*
 * 强制A/B桥臂回到中点，即两桥臂约50%。
 */
void PVINV_PWM_TIM8_ForceNeutral(void);

/*
 * 单极性SPWM底层接口。
 *
 * 输入：
 *   d范围建议[-0.98, +0.98]
 *
 * 内部执行：
 *   CCR1 = center + center * d
 *   CCR2 = center - center * d
 *
 * 其中center使用ARR寄存器单位。
 */
PVINV_PWM_Status_t PVINV_PWM_TIM8_SetUnipolarD(float d);

/*
 * 使能/关闭TIM8更新中断源。
 * NVIC入口仍由CubeMX生成。
 */
void PVINV_PWM_TIM8_EnableUpdateInterrupt(void);
void PVINV_PWM_TIM8_DisableUpdateInterrupt(void);

PVINV_PWM_Diag_t PVINV_PWM_TIM8_GetDiag(void);
PVINV_PWM_Status_t PVINV_PWM_TIM8_GetLastStatus(void);

uint32_t PVINV_PWM_TIM8_GetLastCCR1(void);
uint32_t PVINV_PWM_TIM8_GetLastCCR2(void);
float PVINV_PWM_TIM8_GetLastD(void);

const char *PVINV_PWM_StatusToString(PVINV_PWM_Status_t status);

#ifdef __cplusplus
}
#endif

#endif
