/*
 * Copy the relevant lines into CubeMX-generated main.c USER CODE areas.
 * Do NOT replace the whole CubeMX main.c blindly.
 */

/* USER CODE BEGIN Includes */
#include "pvinv_h563_ll.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
/*
 * CubeMX-generated init functions must already have been called before this point:
 *   MX_GPIO_Init();
 *   MX_GPDMA1_Init();
 *   MX_LPUART1_UART_Init();    // if used
 *   MX_ICACHE_Init();          // if used
 *   MX_ADC1_Init();
 *   MX_ADC2_Init();
 *   MX_TIM8_Init();
 *
 * Start ADC1/ADC2 DMA according to CubeMX-generated LL/HAL code.
 * DMA destination buffers must be:
 *   g_pvinv_adc1_raw, length PVINV_ADC1_RAW_NUM = 3
 *   g_pvinv_adc2_raw, length PVINV_ADC2_RAW_NUM = 2
 */
PVINV_LL_Init();

/* Start ADC1 DMA here. Destination = g_pvinv_adc1_raw, length = PVINV_ADC1_RAW_NUM. */
/* Start ADC2 DMA here. Destination = g_pvinv_adc2_raw, length = PVINV_ADC2_RAW_NUM. */

PVINV_LL_Start();

/* Start TIM8 after ADC/DMA are ready. TIM8 should generate PWM and TRGO ADC trigger. */
PVINV_PWM_TIM->CR1 |= TIM_CR1_CEN;
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
/*
 * Final while(1) must be clean. Do not repeatedly reconfigure GPDMA here.
 * Old test code such as LL_DMA_DisableChannel/SetSrcAddress/EnableChannel must be deleted.
 */
const PVINV_Handle_t *p = PVINV_LL_GetHandle();
(void)p;
/* Add only low-speed tasks here: OLED display, serial print, key scanning, status monitoring. */
/* USER CODE END 3 */
