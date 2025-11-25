/**
 * @file main_example.c
 * @brief EXTI01 - 对射式红外传感器计次示例
 * @example Examples/EXTI/EXTI01_Infrared_Counter/main_example.c
 * @status 已完成
 * @details 使用外部中断（EXTI）实现对射式红外传感器的计次功能
 * 
 * 硬件要求：
 * - LED1连接到PA1（用于状态指示）
 * - 对射式红外传感器输出连接到PA0（EXTI Line 0）
 * - OLED显示屏（可选，用于显示计数）
 * 
 * 硬件配置：
 * 在 Examples/EXTI/EXTI01_Infrared_Counter/board.h 中配置：
 * - EXTI_CONFIGS: EXTI配置（PA0，下降沿触发）
 * - LED_CONFIGS: LED配置（PA1）
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8/9，可选）
 * 
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/EXTI/EXTI01_Infrared_Counter/Examples.uvprojx
 * 2. 根据实际硬件修改 board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "system_init.h"
#include "exti.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "delay.h"
#include "board.h"
#include "gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"  /* 用于GPIO_EXTILineConfig */
#include <stdint.h>
#include <stddef.h>

/* 计数器（volatile，中断和主循环都会访问） */
static volatile uint32_t g_counter = 0;

/**
 * @brief EXTI中断回调函数（对射式红外传感器中断）
 * @param line EXTI线号
 * @param user_data 用户数据
 */
static void InfraredSensor_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    
    /* 在中断回调中只做简单操作：计数 */
    g_counter++;
    
    /* 注意：不要在这里执行复杂操作（如OLED显示），复杂操作在主循环处理 */
}

int main(void)
{
    /* 系统初始化 */
    System_Init();
    
    /* OLED初始化（可选） */
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "EXTI Counter");
    OLED_ShowString(2, 1, "Count: 0");
    
    /* 初始化EXTI0（PA0，双边沿触发，中断模式） */
    /* 注意：对射式红外传感器通常输出低电平表示遮挡，高电平表示未遮挡 */
    /* 使用双边沿触发，可以在电平变化时（上升沿或下降沿）都触发中断 */
    /* 如果传感器一直输出低电平，使用下降沿触发不会产生中断 */
    /* EXTI_HW_Init会调用EXTI_ConfigGPIO，它会配置GPIO为浮空输入并配置GPIO_EXTILineConfig */
    if (EXTI_HW_Init(EXTI_LINE_0, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT) != EXTI_OK)
    {
        /* EXTI初始化失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }
    
    /* 重新配置PA0为上拉输入（EXTI_HW_Init会将GPIO配置为浮空输入，需要重新配置为上拉） */
    /* 注意：必须在EXTI初始化之后配置，确保GPIO配置正确 */
    /* GPIO_EXTILineConfig的配置不会因为GPIO_Config而改变，所以不需要重新配置 */
    GPIO_Config(GPIOA, GPIO_Pin_0, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz);
    
    /* 设置EXTI中断回调函数 */
    if (EXTI_SetCallback(EXTI_LINE_0, InfraredSensor_Callback, NULL) != EXTI_OK)
    {
        /* 设置回调失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(50);
            LED1_Off();
            Delay_ms(50);
        }
    }
    
    /* 使能EXTI中断 */
    if (EXTI_Enable(EXTI_LINE_0) != EXTI_OK)
    {
        /* 使能中断失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(50);
            LED1_Off();
            Delay_ms(50);
        }
    }
    
    
    /* 主循环：显示计数等复杂逻辑 */
    uint32_t last_counter = 0;
    while(1)
    {
        /* 检查计数器是否变化 */
        if (g_counter != last_counter)
        {
            last_counter = g_counter;
            
            /* 更新OLED显示 */
            OLED_ShowNum(2, 7, g_counter, 5);
            
            /* LED闪烁反馈 */
            LED1_Toggle();
        }
        
        /* 延时降低CPU占用率 */
        Delay_ms(10);
    }
}

