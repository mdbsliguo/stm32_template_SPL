/**
 * @file main.c
 * @brief 主程序入口
 * @version 1.0.0
 * @date 2024-01-01
 */

#include "stm32f10x.h"
#include "system_init.h"

int main(void)
{
    /* 系统初始化 */
    System_Init();
    
    /* 主循环 */
    while (1)
    {
        /* 空循环 */
    }
}
