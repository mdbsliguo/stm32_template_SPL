#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"                  // Device header

extern uint32_t Timer_Num;
extern uint32_t Timer_Cnt;
extern uint16_t hour;
extern uint16_t	min;
extern uint16_t second;
void Timer_Init(uint32_t arr ,uint32_t psc );

#endif
