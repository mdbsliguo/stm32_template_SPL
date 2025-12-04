#ifndef __APP_UART1_H
#define __APP_UART1_H

#include "app_public.h"                  // 

#define   UART1INIT	                 0X00	       //初始状态
#define	  UART1START	               0X01	       //开始接收数据包
#define   UART1OVER	                 0X02	       //接收完成数据包

#define   FRAME_Rx1Client			       25
#define   FRAME_Tx1Client			       8

extern uint8_t     DucReceive1Buff[100];
extern uint8_t     DucUart1State;
extern uint8_t     DucUart1Count;	

extern uint8_t     ClientRx1DateBuff[FRAME_Rx1Client];
extern uint8_t     ClientTx1DateBuff[FRAME_Tx1Client];
extern uint8_t     Modbus_Read_Cmd[8];


extern uint8_t     Uart1_RxClient_Flag;



extern uint16_t   DucNowchroma ;
extern uint16_t   DuiGasHighAlarm;									  //高报浓度值
extern uint16_t   DuiGasLowAlarm;									    //低报浓度值
extern uint16_t   DuiCGasChroma;										  //全量程浓度值

extern uint16_t   DuiGasIform;	                      //气体类型值 CH4
extern uint16_t   DuiGasDecimalDigits;	              //小数点个数
extern uint16_t   DuiGasUnit;                         //数据单位 


extern uint16_t   DuiGasLocalAddrss;                 //设备地址  初始化地址为0x01;   
extern uint8_t    GasWorkState; 

extern uint16_t   DuiGas_Adc;                       //模组实时AD值;   

extern uint8_t    Uart1_TxClient_Flag;
extern uint8_t    Uart1_TxClient_Cnt;


void USART1_Init(void);
void USART1_SendByte(uint8_t Byte);
void USART1_SendArray(uint8_t *Array, uint16_t Length);
void USART1_SendString(char *String);
void USART1_SendNumber(uint32_t Number, uint8_t Length);
void USART1_Printf(char *format, ...);
void Uart1_Client_RxData(void);





#endif
