
#ifndef __APP_PUBLIC_H
#define __APP_PUBLIC_H

#include "stm32f10x.h"                  // Device header
#include <stdio.h>
#include "string.h"
#include <stdarg.h>




void Delay_us(uint32_t xus);
void Delay_ms(uint32_t xms);
void Delay_s(uint32_t xs);

uint16_t FuiCheckCRC16( unsigned char pucFrame[], unsigned char usLen );




#endif
