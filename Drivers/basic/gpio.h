#ifndef GPIO_H
#define GPIO_H

#include "stm32f10x.h"

/* 新增：GPIO操作状态码 */
typedef enum {
    GPIO_OK = 0,
    GPIO_ERROR_NULL_PTR,
    GPIO_ERROR_INVALID_PORT,
    GPIO_ERROR_INVALID_PIN,
    GPIO_ERROR_INVALID_MODE
} GPIO_Status_t;

/* 新增：GPIO模式枚举 */
typedef enum {
    GPIO_MODE_INPUT_ANALOG      = GPIO_Mode_AIN,
    GPIO_MODE_INPUT_FLOATING    = GPIO_Mode_IN_FLOATING,
    GPIO_MODE_INPUT_PULLUP      = GPIO_Mode_IPU,
    GPIO_MODE_INPUT_PULLDOWN    = GPIO_Mode_IPD,
    GPIO_MODE_OUTPUT_PP         = GPIO_Mode_Out_PP,
    GPIO_MODE_OUTPUT_OD         = GPIO_Mode_Out_OD,
    GPIO_MODE_AF_PP             = GPIO_Mode_AF_PP,
    GPIO_MODE_AF_OD             = GPIO_Mode_AF_OD
} GPIO_Mode_t;

/* 新增：GPIO速度枚举 */
typedef enum {
    GPIO_SPEED_2MHz   = GPIO_Speed_2MHz,
    GPIO_SPEED_10MHz  = GPIO_Speed_10MHz,
    GPIO_SPEED_50MHz  = GPIO_Speed_50MHz
} GPIO_Speed_t;

/* 
 * 修改1：函数名从GPIO_Init改为GPIO_Config，避免与库函数冲突
 * 修改2：返回值改为GPIO_Status_t，提供错误反馈
 */
GPIO_Status_t GPIO_Config(GPIO_TypeDef* port, uint16_t pin, GPIO_Mode_t mode, GPIO_Speed_t speed);

/* 新增：时钟使能函数 */
GPIO_Status_t GPIO_EnableClock(GPIO_TypeDef* port);

/* 
 * 修改3：原有函数改为宏，保持向后兼容
 * 原GPIO_Init_Output()自动调用GPIO_Config()
 * 原GPIO_SetPin()/ResetPin()合并为GPIO_WritePin()
 */
#define GPIO_Init_Output(port, pin)  GPIO_Config(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz)
#define GPIO_SetPin(port, pin)       GPIO_WritePin(port, pin, Bit_SET)
#define GPIO_ResetPin(port, pin)     GPIO_WritePin(port, pin, Bit_RESET)
#define GPIO_TogglePin(port, pin)    GPIO_Toggle(port, pin)

/* 新增：统一电平写入 */
GPIO_Status_t GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, BitAction bit_val);

/* 新增：读取引脚电平 */
uint8_t GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin);

/* 新增：翻转引脚 */
GPIO_Status_t GPIO_Toggle(GPIO_TypeDef* port, uint16_t pin);

#endif /* GPIO_H */
