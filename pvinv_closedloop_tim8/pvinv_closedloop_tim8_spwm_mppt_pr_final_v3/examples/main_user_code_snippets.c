/*
 * main.c用户代码片段
 *
 * 只复制到USER CODE区域。
 * 不要修改CubeMX自动生成的MX_TIM8_Init、MX_ADC1_Init、MX_ADC2_Init、
 * MX_GPDMA1_Init、MX_GPIO_Init、SystemClock_Config等函数。
 */

/* USER CODE BEGIN Includes */
#include "pvinv_control.h"
/* USER CODE END Includes */


/* USER CODE BEGIN PV */
static PVINV_Status_t g_pvinv_status;
static PVINV_Diag_t g_pvinv_diag;
/* USER CODE END PV */


/* USER CODE BEGIN 2 */

/*
 * 初始化闭环用户层：
 * - 检查TIM8配置；
 * - 初始化ADC用户层缓冲；
 * - 初始化MPPT；
 * - 初始化PR电流环。
 */
g_pvinv_status = PVINV_Control_Init();
if (g_pvinv_status != PVINV_OK)
{
    g_pvinv_diag = PVINV_Control_GetDiag();
    (void)g_pvinv_diag;
    Error_Handler();
}

/*
 * 启动闭环：
 * - 启动ADC DMA；
 * - 启动TIM8四路互补输出；
 * - 使能TIM8 Update中断；
 * - 进入ADC_WARMUP；
 * - 之后进入SOFTSTART。
 *
 * 强烈建议：
 * 第一次上板不要直接调用Start，应先完成开环发波验证和ADC标定。
 */
g_pvinv_status = PVINV_Control_Start();
if (g_pvinv_status != PVINV_OK)
{
    g_pvinv_diag = PVINV_Control_GetDiag();
    (void)g_pvinv_diag;
    Error_Handler();
}

/* USER CODE END 2 */


/*
 * 在while(1)中加入：
 *
 * USER CODE BEGIN WHILE
 * while (1)
 * {
 *     PVINV_Control_Service();
 *     USER CODE END WHILE
 *     USER CODE BEGIN 3
 * }
 * USER CODE END 3
 */


/* USER CODE BEGIN 4 */

/*
 * 如果工程中已有HAL_TIM_PeriodElapsedCallback，不要重复定义。
 * 只把PVINV_Control_TIM_PeriodElapsedCallback(htim);
 * 合并到已有函数中。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    PVINV_Control_TIM_PeriodElapsedCallback(htim);
}

/* USER CODE END 4 */
