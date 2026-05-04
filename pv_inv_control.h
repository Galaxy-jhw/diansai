#ifndef PV_INV_CONTROL_H
#define PV_INV_CONTROL_H

#include "main.h"
#include <stdint.h>

/* ============================================================
 * 0. 外设选择
 * ============================================================ */

/*
 * 默认使用 TIM1：
 *
 * TIM1_CH1  / CH1N -> A 桥臂
 * TIM1_CH2  / CH2N -> B 桥臂
 *
 * 使用 TIM8 时改为：
 * #define PVINV_PWM_HANDLE htim8
 * #define PVINV_PWM_TIM    TIM8
 */
#define PVINV_PWM_HANDLE              htim1
#define PVINV_PWM_TIM                 TIM1

/*
 * ADC DMA 通道顺序必须为：
 *
 * ADC[0] = Ud    输入直流电压
 * ADC[1] = Id    输入直流电流
 * ADC[2] = uREF  参考正弦信号
 * ADC[3] = iF    输出电流反馈
 * ADC[4] = uo    输出电压，可选
 */
#define PVINV_ADC_HANDLE              hadc1
#define PVINV_ADC_CH_NUM              5u

/* ============================================================
 * 1. 控制频率
 * ============================================================ */

#define PVINV_CTRL_FREQ_HZ            20000.0f
#define PVINV_CTRL_TS                 (1.0f / PVINV_CTRL_FREQ_HZ)

/* ============================================================
 * 2. ADC 标定参数
 *
 * 实际物理量 = ADC_raw * SCALE + OFFSET
 *
 * 这些必须按你的硬件重新标定。
 * ============================================================ */

/* Ud：输入直流电压 */
#define PVINV_UD_SCALE                0.0200f
#define PVINV_UD_OFFSET               0.0f

/* Id：输入直流电流，正方向为从模拟光伏源流入逆变器 */
#define PVINV_ID_SCALE                0.0050f
#define PVINV_ID_OFFSET               0.0f
#define PVINV_ID_SIGN                 1.0f

/*
 * uREF：题目给的参考正弦，峰峰值 2V。
 * 若硬件偏置到 1.65V 后进 ADC，这里应换算成以 0 为中心的正弦量。
 */
#define PVINV_UREF_SCALE              0.0016117f
#define PVINV_UREF_OFFSET            -3.3000f

/*
 * iF：输出电流反馈。
 * 电流环一启动就冲击时，优先把 PVINV_IFB_SIGN 改为 -1.0f。
 */
#define PVINV_IFB_SCALE               0.0030f
#define PVINV_IFB_OFFSET             -6.144f
#define PVINV_IFB_SIGN                1.0f

/* uo：输出电压，可选 */
#define PVINV_UO_SCALE                0.0200f
#define PVINV_UO_OFFSET               0.0f

/* ============================================================
 * 3. 题目参数和保护参数
 * ============================================================ */

#define PVINV_US_NOMINAL              60.0f
#define PVINV_UD_MPP                  30.0f

/* 欠压保护：题目要求 Ud(th) = 25 ± 0.5V */
#define PVINV_UD_UNDER_TH             25.0f
#define PVINV_UD_RECOVER_TH           27.0f

/* 输入过压保护 */
#define PVINV_UD_OVER_TH              65.0f
#define PVINV_UD_OVER_RECOVER_TH      50.0f

/* 输出过流保护，先保守设为约 3.2A */
#define PVINV_IFB_OC_TH               3.2f
#define PVINV_IFB_OC_RECOVER_TH       1.5f

/* uREF 频率范围 */
#define PVINV_REF_FREQ_MIN            45.0f
#define PVINV_REF_FREQ_MAX            55.0f
#define PVINV_REF_FREQ_DEFAULT        50.0f

/* uREF 上升过零检测滞回 */
#define PVINV_UREF_ZC_HYST            0.03f

/* uREF 幅值过低时不允许锁定 */
#define PVINV_UREF_AMP_MIN            0.15f

/* 超过该时间未检测到有效过零，认为参考丢失 */
#define PVINV_REF_LOST_TIME_S         0.060f

/* ============================================================
 * 4. MPPT 电导增量法参数
 * ============================================================ */

/*
 * MPPT 更新频率：
 * 20kHz / 100 = 200Hz
 */
#define PVINV_MPPT_DIV                100u

/*
 * 电导增量法基础步长。
 * 单位：交流输出电流幅值 A。
 */
#define PVINV_MPPT_BASE_STEP          0.004f

/* 自适应步长最大倍数 */
#define PVINV_MPPT_STEP_GAIN_MAX      3.0f

/* 电导增量判断死区 */
#define PVINV_MPPT_DV_EPS             0.04f
#define PVINV_MPPT_DI_EPS             0.01f
#define PVINV_MPPT_G_EPS              0.002f

/*
 * 当 ΔU、ΔI 太小时，为了获得斜率信息，做极小探索扰动。
 * 这里不使用 Ud=30V 作为控制目标。
 */
#define PVINV_MPPT_PROBE_STEP         0.0015f

/* 输出交流电流幅值限幅 */
#define PVINV_IAMP_MIN                0.00f
#define PVINV_IAMP_MAX                3.00f

/*
 * 每个控制周期电流幅值最大变化。
 * 20kHz 下，0.00008A/周期 ≈ 1.6A/s。
 */
#define PVINV_IAMP_SLEW_PER_ISR       0.00008f

/* ============================================================
 * 5. PR/QPR 电流环参数
 * ============================================================ */

/*
 * 准 PR 控制器：
 *
 * G(s) = Kp + Kr * 2wc*s / (s^2 + 2wc*s + w0^2)
 *
 * w0 跟随 uREF 实际频率。
 */
#define PVINV_PR_KP                   0.045f
#define PVINV_PR_KR                   45.0f
#define PVINV_PR_WC_HZ                6.0f

/* PR 抗饱和回算系数 */
#define PVINV_PR_AW_GAIN              0.20f

/* PR 谐振状态限幅 */
#define PVINV_PR_RESONANT_LIMIT       0.80f

/* 调制量限幅 */
#define PVINV_MOD_MIN                -0.95f
#define PVINV_MOD_MAX                 0.95f

/* 每个控制周期调制量最大变化 */
#define PVINV_MOD_SLEW_PER_ISR        0.012f

/*
 * 输出电压前馈，默认关闭。
 * uo 采样稳定后可以再打开调试。
 */
#define PVINV_UO_FF_GAIN              0.0f

/*
 * 相位补偿。
 * 输出电流相对 uREF 滞后 5° 时，可设：
 * 5 * pi / 180 = 0.0873
 */
#define PVINV_PHASE_COMP_RAD          0.0f

/* ============================================================
 * 6. 测试模式
 * ============================================================ */

/*
 * 1：关闭 MPPT，用固定电流幅值调试电流环。
 * 0：正常电导增量法 MPPT。
 */
#define PVINV_TEST_FIXED_IAMP_ENABLE  0
#define PVINV_TEST_FIXED_IAMP         0.30f

/* ============================================================
 * 7. 状态机
 * ============================================================ */

typedef enum
{
    PVINV_STATE_STOP = 0,
    PVINV_STATE_WAIT_REF,
    PVINV_STATE_SOFT_START,
    PVINV_STATE_RUN,

    PVINV_STATE_FAULT_UNDERVOLTAGE,
    PVINV_STATE_FAULT_OVERVOLTAGE,
    PVINV_STATE_FAULT_OVERCURRENT,
    PVINV_STATE_FAULT_REF_LOST,
    PVINV_STATE_FAULT_PWM_BREAK
} PVINV_State_t;

typedef struct
{
    float ud;
    float id;
    float uref;
    float ifb;
    float uo;

    float ud_f;
    float id_f;
    float uref_f;
    float ifb_f;
    float uo_f;

    float pv_power;

    float theta;
    float ref_freq;
    float ref_amp;
    uint8_t ref_locked;

    float iamp_target;
    float iamp_cmd;
    float i_ref;

    float current_err;
    float modulation;

    float mppt_v_avg;
    float mppt_i_avg;
    float mppt_g;

    PVINV_State_t state;

    uint32_t mppt_cnt;
    uint32_t fault_cnt;

    uint8_t pwm_output_enabled;
} PVINV_Handle_t;

void PVINV_Init(void);
void PVINV_Start(void);
void PVINV_Stop(void);

/*
 * 推荐由 ADC DMA 完成中断调用。
 * 调用频率必须等于 PVINV_CTRL_FREQ_HZ。
 */
void PVINV_ControlISR(void);

const PVINV_Handle_t *PVINV_GetHandle(void);

#endif
