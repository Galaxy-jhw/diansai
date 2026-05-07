#include "KB.h"

static void delay_us(uint32_t n)
{
	uint32_t temp;	    	 
	SysTick->LOAD=n*(250); 					//时间加载	  		 
	SysTick->VAL=0x00;        					//清空计数器
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//开始倒数	  
	do
	{
		temp=SysTick->CTRL;
	}while((temp&0x01)&&!(temp&(1<<16)));		//等待时间到达   
	SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//关闭计数器
	SysTick->VAL =0X00;      					 //清空计数器	 
}
uint16_t Keyboard_ReadData(void)  //输出被按下的按键号
{
	uint8_t i,data;
//	HAL_GPIO_WritePin(GPIO_LD, GPIO_LD_PIN,GPIO_PIN_SET);
	LL_GPIO_SetOutputPin(GPIO_LD,GPIO_LD_PIN);
	delay_us(1);
//	HAL_GPIO_WritePin(GPIO_LD, GPIO_LD_PIN,GPIO_PIN_RESET);
	LL_GPIO_ResetOutputPin(GPIO_LD,GPIO_LD_PIN);
	for(i=0;i<16;i++)
	{
		delay_us(1);
		if(/*HAL_GPIO_ReadPin(GPIO_DA, GPIO_DA_PIN)*/LL_GPIO_IsInputPinSet(GPIO_DA,GPIO_DA_PIN))
			data = 16 - i;
		LL_GPIO_SetOutputPin(GPIO_CK, GPIO_CK_PIN);
		delay_us(1);
		LL_GPIO_ResetOutputPin(GPIO_CK, GPIO_CK_PIN);
	}
	return data;
}
void KeyAction(uint8_t key)
{
	switch(key)
	{
		case 1:
		{
		LL_GPIO_SetOutputPin(LED0_GPIO_Port,LED0_Pin);
		}
	  break;
		
		case 2:
		{
		LL_GPIO_SetOutputPin(LED1_GPIO_Port,LED1_Pin);
		}	
	  break;
		
		case 3:
		{
		LL_GPIO_ResetOutputPin(LED0_GPIO_Port,LED0_Pin);
		}
	break;
		
		case 4:
		{
		LL_GPIO_ResetOutputPin(LED1_GPIO_Port,LED1_Pin);
		}
	break;
		
		case 5:
		{

		}
	  break;
				
		case 6:
		{

		}
	break;
				
		case 7:
		{

		}
	  break;
		case 8:
		{

		}
	  break;
		case 9:
		{

	  break;
		}
		case 10:
		{

		}
	   break;
				
		case 11:
		{

		}
	  break;
				
		case 12:
		{

		}
	  break;
				
		case 13:
			{

		}
	  break;
				
		case 14:
			{

		}
	  break;
				
		case 15:
			{

		}
	  break;
				
		case 16:
			{

		}
	  break;
	}	
}

