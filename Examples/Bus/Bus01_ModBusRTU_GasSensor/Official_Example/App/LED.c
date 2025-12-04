

#include  "LED.h"
#include  "Delay.h"

/*******************************************************************************  
*
* 函 数 名: void LEDInit(void)
* 功能描述: LED流水灯闪烁程序 调用PB3、PB4、PB5、PB6 
* 函数说明: 
* 调用函数: 
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240314
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED_Init(void)
{  	
		GPIO_InitTypeDef GPIO_InitStructure;                          //声明一个结构体变量
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);          //选择PB口外设时钟
	
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);           //选择复用端口使能
		GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);       //重定义端口 释放JTAG为普通端口 PB3 PB4 PA15为普通IO
	
		GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;                //GPIO模式，赋值为推挽输出模式
		GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1;             //GPIO引脚，赋值为所有引脚
		GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;	              //GPIO速度，赋值为50MHz
		GPIO_Init(GPIOB,&GPIO_InitStructure);                         //将赋值后的构体变量传递给GPIO_Init函数
	                                                                //函数内部会自动根据结构体的参数配置相应寄存器
	  GPIO_SetBits(GPIOB, GPIO_Pin_0|GPIO_Pin_1);		//设置PB0引脚为高电平
}//实现GPIOA的初始化

/*******************************************************************************  
*
* 函 数 名: void LEDdisplay(void)
* 功能描述: LED流水灯闪烁程序 调用PB3、PB4、PB5、PB6  每隔200ms点亮一个LED
* 函数说明: 
* 调用函数: void Delay_ms(uint32_t xms);
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240314
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED_Display(void)
{
	GPIO_Write(GPIOB, 0xFFF7);  //PB3 0  1111 0111
	Delay_ms(200);
	GPIO_Write(GPIOB, 0xFFEF);  //PB4 0  1110 1111
	Delay_ms(200);
	GPIO_Write(GPIOB, 0xFFDF);  //PB5 0  1101 1111
	Delay_ms(200);
	GPIO_Write(GPIOB, 0xFFBF);  //PB6 0  1011 1111
	Delay_ms(200);
}

/*******************************************************************************  
*
* 函 数 名: vvoid LED1_ON(void)
* 功能描述: LED1-PB0引脚为低电平 点亮   
* 函数说明: 
* 调用函数: void GPIO_ResetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/

void LED1_ON(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_0);		//设置PB0引脚为低电平
}

/*******************************************************************************  
*
* 函 数 名: void LED1_OFF(void)
* 功能描述: LED1-PB0引脚为高电平 熄灭  
* 函数说明: 
* 调用函数: void GPIO_SetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED1_OFF(void)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_0);		//设置PB0引脚为高电平
}

/**
  * 函    数：
  * 参    数：无
  * 返 回 值：无
  */

/*******************************************************************************  
*
* 函 数 名: void LED1_Turn(void)
* 功能描述: LED1状态翻转
* 函数说明: 
* 调用函数: uint8_t GPIO_ReadOutputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED1_Turn(void)
{
	if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 0)		//获取输出寄存器的状态，如果当前引脚输出低电平
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_0);					          //则设置PB0引脚为高电平
	}
	else													                        //否则，即当前引脚输出高电平
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_0);					        //则设置PB0引脚为低电平
	}
}

/*******************************************************************************  
*
* 函 数 名: vvoid LED2_ON(void)
* 功能描述: LED2-PB1引脚为低电平 点亮   
* 函数说明: 
* 调用函数: void GPIO_ResetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/

void LED2_ON(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_1);		//设置PB1引脚为低电平
}

/*******************************************************************************  
*
* 函 数 名: void LED2_OFF(void)
* 功能描述: LED2-PB1引脚为高电平 熄灭  
* 函数说明: 
* 调用函数: void GPIO_SetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED2_OFF(void)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_1);		//设置PB1引脚为高电平
}

/**
  * 函    数：
  * 参    数：无
  * 返 回 值：无
  */

/*******************************************************************************  
*
* 函 数 名: void LED2_Turn(void)
* 功能描述: LED2状态翻转
* 函数说明: 
* 调用函数: uint8_t GPIO_ReadOutputDataBit(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240314
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void LED2_Turn(void)
{
	if (GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_1) == 0)		//获取输出寄存器的状态，如果当前引脚输出低电平
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_1);					          //则设置PB1引脚为高电平
	}
	else													                        //否则，即当前引脚输出高电平
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_1);					        //则设置PB1引脚为低电平
	}
}


