
#include "EXTI_KEY.h"                  // Device header
#include "key.h"  

uint16_t KEY_Count=0;				//全局变量，用于计�?
uint16_t KEY_Flag=0;				//全局变量，用于计�?

/**
  * �?   数：计数传感器初始化
  * �?   数：�?
  * �?�?值：�?
  */
void EXTI_KEY_Init(void)
{
	/*开启时�?/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA的时�?
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);		//开启AFIO的时钟，外部中断必须开启AFIO的时�?
	
	/*GPIO初始�?/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//将PA0引脚初始化为上拉输入
	
	/*AFIO选择中断引脚*/
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);//将外部中断的0号线映射到GPIOA，即选择PA0为外部中断引�?
	
	/*EXTI初始�?/
	EXTI_InitTypeDef EXTI_InitStructure;						//定义结构体变�?
	EXTI_InitStructure.EXTI_Line = EXTI_Line0;					//选择配置外部中断�?号线
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;					//指定外部中断线使�?
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;			//指定外部中断线为中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;		//指定外部中断线为下降沿触�?
	EXTI_Init(&EXTI_InitStructure);								//将结构体变量交给EXTI_Init，配置EXTI外设
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分�?
																//即抢占优先级范围�?~3，响应优先级范围�?~3
																//此分组配置在整个工程中仅需调用一�?
																//若有多个中断，可以把此代码放在main函数内，while循环之前
																//若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配�?
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						           //定义结构体变�?
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;		       //选择配置NVIC的EXTI0�?
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				     //指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	 //指定NVIC线路的抢占优先级�?
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			   //指定NVIC线路的响应优先级�?
	NVIC_Init(&NVIC_InitStructure);								             //将结构体变量交给NVIC_Init，配置NVIC外设
}


/**
  * �?   数：获取计数传感器的计数�?
  * �?   数：�?
  * �?�?值：计数值，范围�?~65535
  */
uint16_t EXTI_KEY_Get(void)
{
	return KEY_Count;
}

/**
  * �?   数：EXTI15_10外部中断函数
  * �?   数：�?
  * �?�?值：�?
  * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
  *           函数名为预留的指定名称，可以从启动文件复�?
  *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
  */
void EXTI0_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line0) == 1)		//判断是否是外部中�?4号线触发的中�?
	{
		/*如果出现数据乱跳的现象，可再次判断引脚电平，以避免抖�?/
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)
		{
				KEY_Count ++;					//计数值自增一�?
				KeyNum++;
				if(KeyNum>=4)                     //键值增加到4时，强制键值为1
				{
						KeyNum=1;
				}
		}
		EXTI_ClearITPendingBit(EXTI_Line0);		//清除外部中断0号线的中断标志位
													//中断标志位必须清�?
													//否则中断将连续不断地触发，导致主程序卡死
	}
}
