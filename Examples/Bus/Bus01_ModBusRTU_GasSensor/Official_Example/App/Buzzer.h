

#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"                  // Device header

#define  BUZZERPORT  GPIOB
#define  BUZZER  GPIO_Pin_9

/*低音部分*/
 
#define l1 262
 
#define l2 294
 
#define l3 330
 
#define l4 349
 
#define l5 392
 
#define l6 440
 
#define l7 494
 
/*中音部分*/
 
#define m1 523
 
#define m2 587
 
#define m3 659
 
#define m4 694
 
#define m5 784
 
#define m6 880
 
#define m7 988
 
/*高音部分*/
 
#define h1 1046
 
#define h2 1175
 
#define h3 1318
 
#define h4 1397
 
#define h5 1568
 
#define h6 1760
 
#define h7 1976

#define Buzzer_TOGGLE		 {GPIOB->ODR ^=GPIO_Pin_9;}

extern uc16 love_the_world[870];
extern uc16 music1[22];


extern uc16 SONG_TONE[];
extern uc16 SONG_LONG[];


void Buzzer_Init(void);
void Buzzer_ON(void);
void Buzzer_OFF(void);
void Buzzer_Turn01(void);
void Buzzer_Turn02(void);
void love_world(uc16 music[]);
void HappyBirthday(void);
void Disirer(void);

//enum  Low_frequency{};
//enum  Normal_frequency{};
//enum  High_frequency{};

#endif  
