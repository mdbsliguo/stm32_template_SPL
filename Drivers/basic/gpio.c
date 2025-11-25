#include "gpio.h"
#include "stm32f10x.h"
#include <stddef.h>

/*******************************************************************************
 * 函数名称：GPIO_GetPortClock
 *******************************************************************************/
static uint32_t GPIO_GetPortClock(GPIO_TypeDef* port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    if (port == GPIOE) return RCC_APB2Periph_GPIOE;
    if (port == GPIOF) return RCC_APB2Periph_GPIOF;
    if (port == GPIOG) return RCC_APB2Periph_GPIOG;
    return 0xFFFFFFFF;
}

/*******************************************************************************
 * 函数名称：GPIO_EnableClock
 *******************************************************************************/
GPIO_Status_t GPIO_EnableClock(GPIO_TypeDef* port)
{
    /* ========== 参数校验 ========== */
    if (port == NULL) {
        return GPIO_ERROR_NULL_PTR;
    }
    
    uint32_t clock = GPIO_GetPortClock(port);
    if (clock == 0xFFFFFFFF) {
        return GPIO_ERROR_INVALID_PORT;
    }
    RCC_APB2PeriphClockCmd(clock, ENABLE);
    return GPIO_OK;
}

/*******************************************************************************
 * 函数名称：GPIO_Config
 * 修改点：函数名从GPIO_Init改为GPIO_Config
 * 关键修正：第58行直接调用标准库的 GPIO_Init，不是STM32_GPIO_Init
 *******************************************************************************/
GPIO_Status_t GPIO_Config(GPIO_TypeDef* port, uint16_t pin, GPIO_Mode_t mode, GPIO_Speed_t speed)
{
    /* ========== 参数校验 ========== */
    if (port == NULL) {
        return GPIO_ERROR_NULL_PTR;
    }
    if (pin == 0) {
        return GPIO_ERROR_INVALID_PIN;
    }
    /* 注意：mode和speed的枚举值范围由编译器保证 */
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_Status_t status = GPIO_EnableClock(port);
    if (status != GPIO_OK) {
        return status;
    }
    
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Mode = (GPIOMode_TypeDef)mode;
    GPIO_InitStructure.GPIO_Speed = (GPIOSpeed_TypeDef)speed;
    
    /* 修正：直接调用标准库函数 GPIO_Init */
    GPIO_Init(port, &GPIO_InitStructure);
    
    return GPIO_OK;
}

/*******************************************************************************
 * 函数名称：GPIO_WritePin
 *******************************************************************************/
GPIO_Status_t GPIO_WritePin(GPIO_TypeDef* port, uint16_t pin, BitAction bit_val)
{
    /* ========== 参数校验 ========== */
    if (port == NULL) {
        return GPIO_ERROR_NULL_PTR;
    }
    if (pin == 0) {
        return GPIO_ERROR_INVALID_PIN;
    }
    /* 注意：bit_val只能是Bit_SET或Bit_RESET，由编译器保证 */
    
    if (bit_val == Bit_SET) {
        GPIO_SetBits(port, pin);
    } else {
        GPIO_ResetBits(port, pin);
    }
    return GPIO_OK;
}

/*******************************************************************************
 * 函数名称：GPIO_ReadPin
 *******************************************************************************/
uint8_t GPIO_ReadPin(GPIO_TypeDef* port, uint16_t pin)
{
    /* ========== 参数校验 ========== */
    if (port == NULL || pin == 0) {
        return 0;  /* 无效参数返回0（低电平） */
    }
    
    return GPIO_ReadInputDataBit(port, pin);
}

/*******************************************************************************
 * 函数名称：GPIO_Toggle
 *******************************************************************************/
GPIO_Status_t GPIO_Toggle(GPIO_TypeDef* port, uint16_t pin)
{
    /* ========== 参数校验 ========== */
    if (port == NULL) {
        return GPIO_ERROR_NULL_PTR;
    }
    if (pin == 0) {
        return GPIO_ERROR_INVALID_PIN;
    }
    
    port->ODR ^= pin;
    return GPIO_OK;
}

