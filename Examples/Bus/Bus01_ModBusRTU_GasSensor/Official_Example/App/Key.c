#include "key.h"                  // Device header
#include "Delay.h"

uint8_t KeyNum=0;

/*******************************************************************************  
*
* 函 数 名: void Key_Init(void)
* 功能描述: PA0--KEY1  PA1--KEY2 按键初始化
* 函数说明: 
* 调用函数: 
* 全局变量: 
* 输    入: 无
* 返    回: 
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void Key_Init(void)
{

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOA的时钟
	
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);						//将PA0和PA1引脚初始化为上拉输入
}



/*******************************************************************************  
*
* 函 数 名: uint8_t Key_GetNum(void)
* 功能描述: 按键获取键码
* 函数说明: 此函数是阻塞式操作，当按键按住不放时，函数会卡住，直到按键松手
* 调用函数: 
* 全局变量: 
* 输    入: 无
* 返    回: 按下按键的键码值，范围：0~2，返回0代表没有按键按下
* 设 计 者：Pirate1865               日期：20240319
* 修 改 者：                         日期：
* 版    本：
*
********************************************************************************/
void Key_GetNum(void)
{
	
	if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)			//读PA0输入寄存器的状态，如果为0，则代表按键1按下
	{
		Delay_ms(20);											//延时消抖
		while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0);	//等待按键松手
		Delay_ms(20);											//延时消抖
		KeyNum ++;												//键值加1
		if(KeyNum>=4)                     //键值增加到4时，强制键值为1
		{
			KeyNum=1;
		}
	}
}
