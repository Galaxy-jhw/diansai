/*
 * V13 main.c USER CODE示例。
 * 重点：必须删除旧main.c while(1)中的GPDMA测试代码。
 * 这里只写用户应放入USER CODE区域的调用，不包含CubeMX可生成初始化函数。
 */

#include "pvinv_h563_ll.h"

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
/* 如果CubeMX生成了adc.h/tim.h，也可按实际工程包含：
 * #include "adc.h"
 * #include "tim.h"
 */
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */

/* CubeMX生成的初始化函数必须先执行。最终main中应至少有类似顺序：
 * MX_GPIO_Init();
 * MX_GPDMA1_Init();
 * MX_ADC1_Init();
 * MX_ADC2_Init();
 * MX_TIM8_Init();
 * MX_LPUART1_UART_Init();     // 如果你仍使用串口
 * MX_ICACHE_Init();
 */

PVINV_LL_Init();

/*
 * 这里需要启动ADC1/ADC2的DMA采样。
 * DMA目的地址必须分别指向：
 *   g_pvinv_adc1_raw，长度PVINV_ADC1_CH_NUM
 *   g_pvinv_adc2_raw，长度PVINV_ADC2_CH_NUM
 *
 * 由于不同CubeMX/GPDMA通道名称不同，本包不硬写ADC/GPDMA启动代码，
 * 避免和你的.ioc实际分配通道不一致。
 *
 * 如果你用CubeMX生成LL ADC/GPDMA初始化，请在CubeMX生成的USER CODE区启动ADC/DMA。
 */

PVINV_LL_ResetAdcPairSync();
PVINV_LL_Start();

/* 启动TIM8计数。TIM8由CubeMX配置PWM、TRGO、死区、Break。
 * TIM8启动后通过TRGO触发ADC1/ADC2，DMA完成中断再触发PVINV控制。
 */
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;

/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    const PVINV_Handle_t *p = PVINV_LL_GetHandle();
    (void)p;

    /*
     * 这里只允许放低速任务：OLED显示、串口打印、按键扫描、状态查看。
     * 严禁在while(1)中反复调用LL_DMA_DisableChannel/SetSrcAddress/EnableChannel等DMA测试代码。
     */
    /* USER CODE END 3 */
}
/* USER CODE END WHILE */
