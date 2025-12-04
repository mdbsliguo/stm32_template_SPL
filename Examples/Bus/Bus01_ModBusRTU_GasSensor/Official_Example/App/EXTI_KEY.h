
#ifndef __EXTI_KEY_H
#define __EXTI_KEY_H

#include "stm32f10x.h"                  // Device header
extern uint16_t KEY_Count;
extern uint16_t KEY_Flag;
void EXTI_KEY_Init(void);
uint16_t EXTI_KEY_Get(void);

#endif
