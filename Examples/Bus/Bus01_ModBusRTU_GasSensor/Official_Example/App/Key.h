
#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"                  // Device header

extern uint8_t KeyNum;

void Key_Init(void);
void Key_GetNum(void);

#endif
