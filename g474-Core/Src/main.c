/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "hrtim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "my_hrtim.h"
#include "my_adc.h"
#include "arm_math.h"
#include "bsp_oled.h"
#include "bsp_keyboard.h"
#include "park_trans.h"
#include "QPR_FP.h"
#include "vofa.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
int workmode=0,startstate=0,three_state=0;
float my_theta=0.f;
float U12a,U12b;
float M=0.6f;//三相调制比
float a,b,c,d,e,f,g;

PIDStructure pidIsd,pidIsq;//单相电流环
PIDStructure pidUbus1;//仅交流
PIDStructure pidUbus2;//交直流共用的外环
PIDStructure pidUbus3;//仅直流
PIDStructure pidUbuck;
PIDStructure pidIboost;
PIDStructure pidUout;//输出电压环

float Isdref,Isqref,U12d_1,U12q_1,U12d_2,U12q_2;

float Ud_rec,Uq_rec,Ua_rec,Ub_rec;//整流模式送入SVPWM的量

float vofa1,vofa2;//调试用

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_HRTIM1_Init();
  MX_ADC2_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HRTIM_Start();//使能hrtim
	my_adc_init();//使能adc
	HAL_TIM_Base_Start_IT(&htim2);//使能定时器2
	
	MYSELF_TIM3_Init();
  Keyboard_Init();
	PLL_Init();
  HAL_TIM_Base_Start_IT(&htim3);
	
	PID_Init(&pidUbus1,0.08f,1.5f,1.f/20000.f,0.f,4.f,4.f);//仅交流
	PID_Init(&pidUbus2,0.06f,0.3f,1.f/20000.f,-3.f,4.f,4.f);//交直流共用
	PID_Init(&pidUbus3,0.01f,0.02f,1.f/20000.f,-0.2f,0.2f,0.2f);//仅直流
	PID_Init(&pidUbuck,0.005f,0.01f,1.f/20000.f,-0.1f,0.1f,0.1f);
	PID_Init(&pidUout,0.01f,0.03f,1.f/20000.f,-0.2f,0.2f,0.2f);	
	PID_Init(&pidIsd,5.f,20.f,1.f/20000.f,-40.f,40.f,40.f);//单相电流d
	PID_Init(&pidIsq,5.f,20.f,1.f/20000.f,-40.f,40.f,40.f);//单相电流q
	PID_Init(&pidIboost, 0.1f, 1.f, 1.f/20000.f, -0.3f,0.3f,0.3f);
	
	Upper_Computer_Init(&vofa1);
	Upper_Computer_Init(&vofa2);
	
	HAL_UART_MspDeInit(&huart2);
	OLED_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		OLED_Scan();
		//Upper_Computer_Show_Wave();
		
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV6;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)//定时器中断
{
	if(htim==(&htim2))
	{
		my_adc_getvalue();
		
		my_theta+=2*PI/400;
		if(my_theta>2*PI)
			my_theta-=2*PI;	
		
		if(startstate==0)
		{
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TC1|HRTIM_OUTPUT_TC2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
		}
		
		else if(workmode==0 && startstate==1)//开环模式
		{
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2);
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TC1|HRTIM_OUTPUT_TC2);
			
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
			
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
			
			//pidUrms.ref=24.f;
			//float M=0.6f+PID_Calc(&pidUrms,U_rms);
			a=M*arm_cos_f32(my_theta);
			b=M*arm_sin_f32(my_theta);
			SVPWM1(a,b);
			HRTIM_OUTPUTD_duty(0.5f+0.5f*a);
			HRTIM_OUTPUTE_duty(0.5f-0.5f*a);
			HRTIM_OUTPUTF_duty(0.5f);
	  }
//		
		else if(workmode==1 && startstate==1)//1并网模式
		{
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2);				
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TC1|HRTIM_OUTPUT_TC2);
		
				pidUout.ref=24.f;
				M=0.75f+PID_Calc(&pidUout,Uo_rms);
				a=M*arm_cos_f32(my_theta);
				b=M*arm_sin_f32(my_theta);
				SVPWM1(a,b);
			
			if(Us_rms>5.f){
				if(PLL_Locked==0){
					HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
					HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);				
					HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
				}
				else if((PLL_Locked==1)&&(Us_rms>5.f)){
					HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
					HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
					HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
					
					pidUbus1.ref=45.f;
					pidIsd.ref=PID_Calc(&pidUbus1,Ubus);
					pidIsq.ref=0.f;

					U12d_1=Us_d+Is_q*100.f*PI*0.00094f-PID_Calc(&pidIsd,Is_d);
					U12q_1=Us_q-Is_d*100.f*PI*0.00094f-PID_Calc(&pidIsq,Is_q);
					dq_to_AlphaBeta(&U12a, &U12b, theta, U12d_1, U12q_1);
			
					c=U12a/45.f;
			
					HRTIM_OUTPUTD_duty(0.5f+0.5f*c);
					HRTIM_OUTPUTE_duty(0.5f-0.5f*c);	
				}
			}
			if(Us_rms<5.f){
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);				
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
				pidUbus3.ref=45.f;
				float f=0.467f+PID_Calc(&pidUbus3,Ubus);
				HRTIM_OUTPUTF_duty(f);
			}
	  }

		else if(workmode==2 && startstate==1)//Boost
		{
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
			HRTIM_OUTPUTF_duty(0.4f);
		}
		
		else if(workmode==3 && startstate==1)//功率均分
		{
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2);				
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TC1|HRTIM_OUTPUT_TC2);
		
			pidUout.ref=24.f;
			M=0.75f+PID_Calc(&pidUout,Uo_rms);
			a=M*arm_cos_f32(my_theta);
			b=M*arm_sin_f32(my_theta);
			SVPWM1(a,b);
			
			if(PLL_Locked==0){
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
				
			}
			else if(PLL_Locked==1){
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);

				pidUbus2.ref=45.f;
				pidIsd.ref=1.f+PID_Calc(&pidUbus2,Ubus);
				pidIboost.ref=0.70710675f*pidIsd.ref;
				pidIsq.ref=0.f;

				U12d_1=Us_d+Is_q*100.f*PI*0.00094f-PID_Calc(&pidIsd,Is_d);
				U12q_1=Us_q-Is_d*100.f*PI*0.00094f-PID_Calc(&pidIsq,Is_q);
				dq_to_AlphaBeta(&U12a, &U12b, theta, U12d_1, U12q_1);
				float d=0.467f+PID_Calc(&pidIboost,Iboost);
			
				float e=U12a/45.f;
			
				HRTIM_OUTPUTD_duty(0.5f+0.5f*e);
				HRTIM_OUTPUTE_duty(0.5f-0.5f*e);	
				HRTIM_OUTPUTF_duty(d);	
			}
		}			
	
		else if(workmode==4 && startstate==1)//Buck
		{
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TB1|HRTIM_OUTPUT_TB2);				
			HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TC1|HRTIM_OUTPUT_TC2);
			HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TF1|HRTIM_OUTPUT_TF2);
			pidUbuck.ref=24.f;
			f=0.53f-PID_Calc(&pidUbuck,Uboost);
			HRTIM_OUTPUTF_duty(f);
			
			
			if(PLL_Locked==0){
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
				HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
				
			}
			else if(PLL_Locked==1){
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);
				HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TE1|HRTIM_OUTPUT_TE2);
				

				pidUbus2.ref=45.f;
				pidIsd.ref=1.f+PID_Calc(&pidUbus2,Ubus);
				pidIsq.ref=0.f;

				U12d_1=Us_d+Is_q*100.f*PI*0.00094f-PID_Calc(&pidIsd,Is_d);
				U12q_1=Us_q-Is_d*100.f*PI*0.00094f-PID_Calc(&pidIsq,Is_q);
				dq_to_AlphaBeta(&U12a, &U12b, theta, U12d_1, U12q_1);
				
			
				g=U12a/45.f;
			
				HRTIM_OUTPUTD_duty(0.5f+0.5f*g);
				HRTIM_OUTPUTE_duty(0.5f-0.5f*g);					
			}				
		}
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
