/*
 * main.c用户代码片段
 *
 * 只复制到USER CODE区域。
 * 不要修改CubeMX自动生成的MX_TIM8_Init、MX_GPIO_Init、SystemClock_Config。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_openloop_spwm.h"
#include "pvinv_pwm_tim8.h"
/* USER CODE END Includes */


/* USER CODE BEGIN PV */
static PVINV_OL_Status_t g_ol_status;
static PVINV_PWM_Status_t g_pwm_status;
static PVINV_PWM_Diag_t g_pwm_diag;
/* USER CODE END PV */


/* USER CODE BEGIN 2 */

/*
 * 初始化并检查TIM8配置。
 * 这里不会修改PSC/ARR/RCR/DeadTime/Dithering，只检查CubeMX配置是否可用。
 */
g_ol_status = PVINV_OpenLoopSPWM_Init();
if (g_ol_status != PVINV_OL_STATUS_OK)
{
    /*
     * 调试时查看：
     * g_ol_status
     * PVINV_OpenLoopSPWM_StatusToString(g_ol_status)
     * PVINV_PWM_TIM8_GetLastStatus()
     * PVINV_PWM_StatusToString(PVINV_PWM_TIM8_GetLastStatus())
     */
    g_pwm_status = PVINV_PWM_TIM8_GetLastStatus();
    g_pwm_diag = PVINV_PWM_TIM8_GetDiag();
    (void)g_pwm_status;
    (void)g_pwm_diag;
    Error_Handler();
}

/*
 * 50Hz是正弦调制频率，不是PWM载波频率。
 * PWM载波由CubeMX的TIM8 PSC/ARR/中心对齐/Dithering决定。
 */
PVINV_OpenLoopSPWM_SetSineFrequencyHz(50.0f);

/*
 * 初次开环测试建议小调制比，例如0.02~0.05。
 */
PVINV_OpenLoopSPWM_SetTargetModulation(0.03f);

g_ol_status = PVINV_OpenLoopSPWM_Start();
if (g_ol_status != PVINV_OL_STATUS_OK)
{
    g_pwm_status = PVINV_PWM_TIM8_GetLastStatus();
    g_pwm_diag = PVINV_PWM_TIM8_GetDiag();
    (void)g_pwm_status;
    (void)g_pwm_diag;
    Error_Handler();
}

/* USER CODE END 2 */


/* USER CODE BEGIN 4 */

/*
 * 如果工程中已经存在HAL_TIM_PeriodElapsedCallback，不要重复定义。
 * 只需要把PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(htim);
 * 合并到已有回调中。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_OpenLoopSPWM_TIM_PeriodElapsedCallback(htim);
}

/* USER CODE END 4 */
