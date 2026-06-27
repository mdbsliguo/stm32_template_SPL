/**
 * @file board_early_init.c
 * @brief 小精灵 F103ZE 板级最早初始化
 * @note 8MHz 板：系统时钟由 startup + System_Init 配置（与 NPN05 一致），此处仅释放 JTAG
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
