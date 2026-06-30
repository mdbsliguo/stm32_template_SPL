/**
 * @file board_early_init.c
 * @brief 小精灵 F103ZE 板级最早初始化
 * @note 释放 PB3/PB4/PB5（SPI1 重映射与 JTAG 复用冲突）
 */

#include "board_early_init.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

error_code_t Board_EarlyInit(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    return ERROR_OK;
}
