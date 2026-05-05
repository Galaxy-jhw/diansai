#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>
typedef struct { volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR, RCR, CCR1, CCR2, CCR3, CCR4, BDTR; } TIM_TypeDef;
extern TIM_TypeDef fake_tim8;
#define TIM8 (&fake_tim8)
#define TIM_CR1_CEN (1u<<0)
#define TIM_BDTR_MOE (1u<<15)
#define TIM_CCER_CC1E (1u<<0)
#define TIM_CCER_CC1NE (1u<<2)
#define TIM_CCER_CC2E (1u<<4)
#define TIM_CCER_CC2NE (1u<<6)
#define TIM_CCER_CC3E (1u<<8)
#define TIM_CCER_CC3NE (1u<<10)
#define TIM_CCER_CC4E (1u<<12)
#define TIM_CCER_CC4NE (1u<<14)
#define TIM_SR_UIF (1u<<0)
#define TIM_SR_BIF (1u<<7)
#define TIM_SR_B2IF (1u<<8)
#endif
