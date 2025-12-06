

#include "app_uart1.h"

uint8_t     Modbus_Read_Cmd[8]={0x01,0x03,0x00,0x00,0x00,0x0A,0xC5,0xCD }; //01 03 00 00 00 0A C5 CD
uint8_t     ClientRx1DateBuff[FRAME_Rx1Client]={0};
uint8_t     ClientTx1DateBuff[FRAME_Tx1Client]={0};

uint8_t     DucReceive1Buff[]={0};
uint8_t     DucUart1State=UART1INIT;
uint8_t     DucUart1Count=0;

uint8_t     Uart1_RxClient_Flag=0;


uint16_t   DucNowchroma ;                      //实时浓度�?
uint16_t   DuiGasHighAlarm;		       //高报浓度�?
uint16_t   DuiGasLowAlarm;		       //低报浓度�?
uint16_t   DuiCGasChroma;		       //全量程浓度�?
uint16_t   DuiGasIform;	                       //气体类型�?CH4
uint16_t   DuiGasDecimalDigits;	               //小数点个�?
uint16_t   DuiGasUnit;                         //数据单位 


uint16_t   DuiGasLocalAddrss;                      //设备地址  初始化地址�?x01;  
uint8_t    GasWorkState; 
uint16_t   DuiGas_Adc; 

uint8_t    Uart1_RxClient_Flag;
uint8_t    Uart1_TxClient_Cnt;


/**
  * �?   数：串口初始�?
  * �?   数：�?
  * �?�?值：�?
  */
void USART1_Init(void)
{
	/*开启时�?/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	//开启USART1的时�?
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时�?
	
	/*GPIO初始�?/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//将PA9引脚初始化为复用推挽输出  TX
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);					//将PA10引脚初始化为上拉输入  Rx
	
	/*USART初始�?/
	USART_InitTypeDef USART_InitStructure;					//定义结构体变�?
	USART_InitStructure.USART_BaudRate = 9600;				//波特�?
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	//硬件流控制，不需�?
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;	//模式，发送模式和接收模式均选择
	USART_InitStructure.USART_Parity = USART_Parity_No;		//奇偶校验，不需�?
	USART_InitStructure.USART_StopBits = USART_StopBits_1;	//停止位，选择1�?
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;		//字长，选择8�?
	USART_Init(USART1, &USART_InitStructure);				//将结构体变量交给USART_Init，配置USART1
	
	/*中断输出配置*/
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);			//开启串口接收数据的中断
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);			//配置NVIC为分�?
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;					//定义结构体变�?
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;		//选择配置NVIC的USART1�?
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;		//指定NVIC线路的抢占优先级�?
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//指定NVIC线路的响应优先级�?
	NVIC_Init(&NVIC_InitStructure);							//将结构体变量交给NVIC_Init，配置NVIC外设
	
	/*USART使能*/
	USART_Cmd(USART1, ENABLE);								//使能USART1，串口开始运�?
}

/**
  * �?   数：串口发送一个字�?
  * �?   数：Byte 要发送的一个字�?
  * �?�?值：�?
  */
void USART1_SendByte(uint8_t Byte)
{
	USART_SendData(USART1, Byte);		//将字节数据写入数据寄存器，写入后USART自动生成时序波形
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);	//等待发送完�?
	/*下次写入数据寄存器会自动清除发送完成标志位，故此循环后，无需清除标志�?/
}

/**
  * �?   数：串口发送一个数�?
  * �?   数：Array 要发送数组的首地址
  * �?   数：Length 要发送数组的长度
  * �?�?值：�?
  */
void USART1_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)		//遍历数组
	{
		USART1_SendByte(Array[i]);		//依次调用USART1_SendByte发送每个字节数�?
	}
}

/**
  * �?   数：串口发送一个字符串
  * �?   数：String 要发送字符串的首地址
  * �?�?值：�?
  */
void USART1_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)//遍历字符数组（字符串），遇到字符串结束标志位后停�?
	{
		USART1_SendByte(String[i]);		//依次调用USART1_SendByte发送每个字节数�?
	}
}

/**
  * �?   数：次方函数（内部使用）
  * �?�?值：返回值等于X的Y次方
  */
uint32_t USART1_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//设置结果初值为1
	while (Y --)			//执行Y�?
	{
		Result *= X;		//将X累乘到结�?
	}
	return Result;
}

/**
  * �?   数：串口发送数�?
  * �?   数：Number 要发送的数字，范围：0~4294967295
  * �?   数：Length 要发送数字的长度，范围：0~10
  * �?�?值：�?
  */
void USART1_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)		//根据数字长度遍历数字的每一�?
	{
		USART1_SendByte(Number / USART1_Pow(10, Length - i - 1) % 10 + '0');	//依次调用USART1_SendByte发送每位数�?
	}
}

/**
  * �?   数：使用printf需要重定向的底层函�?
  * �?   数：保持原始格式即可，无需变动
  * �?�?值：保持原始格式即可，无需变动
  */
int fputc(int ch, FILE *f)
{
	USART1_SendByte(ch);			//将printf的底层重定向到自己的发送字节函�?
	return ch;
}

/**
  * �?   数：自己封装的prinf函数
  * �?   数：format 格式化字符串
  * �?   数：... 可变的参数列�?
  * �?�?值：�?
  */
void USART1_Printf(char *format, ...)
{
	char String[100];				//定义字符数组
	va_list arg;					//定义可变参数列表数据类型的变量arg
	va_start(arg, format);			//从format开始，接收参数列表到arg变量
	vsprintf(String, format, arg);	//使用vsprintf打印格式化字符串和参数列表到字符数组�?
	va_end(arg);					//结束变量arg
	USART1_SendString(String);		//串口发送字符数组（字符串）
}




/**
  * �?   数：USART1中断函数
  * �?   数：�?
  * �?�?值：�?
  * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
  *           函数名为预留的指定名称，可以从启动文件复�?
  *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
  * 数据格式�?xFF 0x01 0x02 0x03 0x04 0xFE
  */
void USART1_IRQHandler(void)
{
	uint8_t   temp_buf,i=0;
	uint16_t  CRC_Data;
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)		//判断是否是USART1的接收事件触发的中断
	{
		  temp_buf=USART_ReceiveData(USART1);
     if(DucUart1State==UART1INIT)
		 {
			 if(temp_buf==0x01)                                 //接收到帧头：0x01
			 {
				 DucUart1State=UART1START;                         //接收到帧�?
         DucReceive1Buff[DucUart1Count]=temp_buf;          //把帧头存放在DucReceiveBuff[0]
 				 DucUart1Count+=1;			                           //接收计数�?1	 
			 }
		 }
		 else if(DucUart1State==UART1START)
		 {
				DucReceive1Buff[DucUart1Count]=temp_buf;
				if((DucReceive1Buff[0]==0x01)&&(DucReceive1Buff[1]!=0x03))              //判断�?位是否正�? 终止接收)
				{			
					DucUart1Count=0;
					DucUart1State=UART1INIT;       
					return;
				}
				else
				{
					DucUart1Count+=1;
				}				 
				if((DucUart1Count==FRAME_Rx1Client)&&(DucReceive1Buff[0]==0x01))
				{
						DucUart1Count=0;
						DucUart1State=UART1INIT;
						CRC_Data =(DucReceive1Buff[24]&0x00ff)<<8;                         //CRC高位在后
						CRC_Data =CRC_Data +DucReceive1Buff[23];                          //CRC低位在前					
						if((DucReceive1Buff[0]==0x01)&&(CRC_Data==FuiCheckCRC16(DucReceive1Buff,FRAME_Rx1Client-2)))
						{
							for(i=0;i<FRAME_Rx1Client;i++)
							{
							ClientRx1DateBuff[i]=DucReceive1Buff[i];
							}	
              Uart1_RxClient_Flag=1;							
						}	
            DucUart1State=UART1INIT;						
				}	
				else if(DucUart1Count>=26)    //接收超时，重新接�?
				{
					DucUart1Count=0;
					DucUart1State=UART1INIT;
					return;
				}				
		 }
	
	USART_ClearITPendingBit(USART1, USART_IT_RXNE);			   //清除USART1的RXNE标志�?
																//读取数据寄存器会自动清除此标志位
	}															//如果已经读取了数据寄存器，也可以不执行此代码
}



/*******************************************************************************  
*
* �?�?�? void Uart1_Client_RxData(void)
* 功能描述: 传感器模组接口数据处�?
* 函数说明: 对模组返回的数据进行处理 modbus协议 
* 调用函数: �?
* 全局变量: �?
* �?   �? �?
* �?   �? 
* �?�?者：Pirate1865               日期�?30815
* �?�?者：                         日期�?
* �?   本：
*
********************************************************************************/
void Uart1_Client_RxData(void)
{
  if(Uart1_RxClient_Flag==1)
  {
    DuiGasLocalAddrss =ClientRx1DateBuff[0];                              //模组本地地址
    DuiGasUnit =(ClientRx1DateBuff[3]&0xf0)>>4; 
    DuiGasUnit =DuiGasUnit/2;                                             //与单位显示数组对�?
    DuiGasDecimalDigits=((ClientRx1DateBuff[3]&0x0f)>>2);                 //与转换小数点对照
    DuiGasIform =ClientRx1DateBuff[19];                                   //气体类型
    ClientRx1DateBuff[4]=0x00;                                            //保留数据�?
    DucNowchroma =ClientRx1DateBuff[5];
    DucNowchroma = ((DucNowchroma<<8)&0xff00)+ClientRx1DateBuff[6];       //实时浓度�?				
    DuiGasLowAlarm =ClientRx1DateBuff[7];
    DuiGasLowAlarm = ((DuiGasLowAlarm<<8)&0xff00)+ClientRx1DateBuff[8];   //低报浓度值低8�?
    DuiGasHighAlarm =ClientRx1DateBuff[9];
    DuiGasHighAlarm = ((DuiGasHighAlarm<<8)&0xff00)+ClientRx1DateBuff[10];//高报浓度值低8�?
    DuiCGasChroma =ClientRx1DateBuff[11];
    DuiCGasChroma = ((DuiCGasChroma<<8)&0xff00)+ClientRx1DateBuff[12];    //全量程浓度�?
    ClientRx1DateBuff[13]=0x00;                                            //数据保留�?
    GasWorkState=ClientRx1DateBuff[14];                                   //工作状�?
    DuiGas_Adc =ClientRx1DateBuff[15];			
    DuiGas_Adc = ((DuiGas_Adc<<8)&0xff00)+ClientRx1DateBuff[16];          //当前AD�?					
    Uart1_RxClient_Flag=0;				
  }
}
