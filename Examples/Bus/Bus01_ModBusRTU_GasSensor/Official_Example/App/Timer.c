#include "timer.h"                  // Device header
#include "key.h" 

uint32_t Timer_Num=0;
uint32_t Timer_Cnt=0;
uint16_t hour=0,min=0,second=0;
/**
  * �?   数：定时中断初始�?
  * �?   数：�?
  * �?�?值：�?
  *///计数器溢出频率：CK_CNT_OV = CK_CNT / (ARR + 1)
			//		        = CK_PSC / (PSC + 1) / (ARR + 1)
   // f=72MHz/(7200-1+1)/(10000-1+1)=72MHz/7200/10000=1Hz --> t=1/f=1s
	 // t=1/f=(PSC + 1) * (ARR + 1)/72000000=1
	 
void Timer_Init(uint32_t arr ,uint32_t psc )
{
	/*开启时�?/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);			//开启TIM2的时�?
	
	/*配置时钟�?/
	TIM_InternalClockConfig(TIM2);		//选择TIM2为内部时钟，若不调用此函数，TIM默认也为内部时钟
	
	/*时基单元初始�?/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				//定义结构体变�?
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//时钟分频，选择不分频，此参数用于配置滤波器时钟，不影响时基单元功能
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;	//计数器模式，选择向上计数
	TIM_TimeBaseInitStructure.TIM_Period = arr - 1;				//计数周期，即ARR的�?
	TIM_TimeBaseInitStructure.TIM_Prescaler = psc - 1;				//预分频器，即PSC的�?
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;			//重复计数器，高级定时器才会用�?
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);				//将结构体变量交给TIM_TimeBaseInit，配置TIM2的时基单�?
	
	/*中断输出配置*/
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);						//清除定时器更新标志位
																//TIM_TimeBaseInit函数末尾，手动产生了更新事件
																//若不清除此标志位，则开启中断后，会立刻进入一次中�?
																//如果不介意此问题，则不清除此标志位也�?
	
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);					//开启TIM2的更新中�?
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分�?
																//即抢占优先级范围�?~3，响应优先级范围�?~3
																//此分组配置在整个工程中仅需调用一�?
																//若有多个中断，可以把此代码放在main函数内，while循环之前
																//若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配�?
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变�?
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;				//选择配置NVIC的TIM2�?
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	//指定NVIC线路的抢占优先级�?
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//指定NVIC线路的响应优先级�?
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设
	
	/*TIM使能*/
	TIM_Cmd(TIM2, ENABLE);			//使能TIM2，定时器开始运�?
}

 //定时器中断函数，可以复制到使用它的地�?
void TIM2_IRQHandler(void)
{
		if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
		{
				if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)
				{
						KeyNum++;
						if(KeyNum>=4)                     //键值增加到4时，强制键值为1
						{
							KeyNum=1;
						}
				}
				Timer_Cnt++;
				if(Timer_Cnt>=5)
				{
						Timer_Cnt=0;
						Timer_Num++;
						if(Timer_Num>=86400)
						{
							Timer_Num=0;
						}
						second=Timer_Num%60;
						min=Timer_Num/60;
						if(min>=60)
						{
							min=min%60;
						}
						hour=Timer_Num/3600;
				}
				TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		}
}
