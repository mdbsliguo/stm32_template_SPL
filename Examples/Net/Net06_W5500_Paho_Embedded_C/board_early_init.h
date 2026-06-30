/**
 * @file board_early_init.h
 * @brief 小精灵 F103ZE 板级最早初始化（JTAG 释放）
 */

#ifndef BOARD_EARLY_INIT_H
#define BOARD_EARLY_INIT_H

#include "error_code.h"

error_code_t Board_EarlyInit(void);

#endif /* BOARD_EARLY_INIT_H */
