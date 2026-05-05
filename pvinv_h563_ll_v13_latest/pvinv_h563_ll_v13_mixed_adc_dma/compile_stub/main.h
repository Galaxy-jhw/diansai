#ifndef MAIN_H
#define MAIN_H
#include <stdint.h>

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
} TIM_TypeDef;
extern TIM_TypeDef fake_tim8;
#define TIM8 (&fake_tim8)
#define TIM_CCER_CC1E  (1u<<0)
#define TIM_CCER_CC1NE (1u<<2)
#define TIM_CCER_CC2E  (1u<<4)
#define TIM_CCER_CC2NE (1u<<6)
#define TIM_CCER_CC3E  (1u<<8)
#define TIM_CCER_CC3NE (1u<<10)
#define TIM_CCER_CC4E  (1u<<12)
#define TIM_CCER_CC4NE (1u<<14)
#define TIM_BDTR_MOE   (1u<<15)
#define TIM_SR_BIF     (1u<<7)
#define TIM_SR_B2IF    (1u<<8)

#endif
