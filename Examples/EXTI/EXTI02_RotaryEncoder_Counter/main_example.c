/**
 * @file main_example.c
 * @brief EXTI02 - 旋转编码器计数示例
 * @example Examples/EXTI/EXTI02_RotaryEncoder_Counter/main_example.c
 * @status 待测试
 * @details 使用外部中断（EXTI）实现旋转编码器的计数功能，通过检测两个通道的触发先后顺序判断旋转方向
 * 
 * 硬件要求：
 * - 旋转编码器通道A连接到PB0（EXTI Line 0）
 * - 旋转编码器通道B连接到PB1（EXTI Line 1）
 * - LED1连接到PA1（用于状态指示）
 * - OLED显示屏（可选，用于显示计数）
 * - UART1：PA9(TX), PA10(RX)，115200波特率
 * 
 * 硬件配置：
 * 在 Examples/EXTI/EXTI02_RotaryEncoder_Counter/board.h 中配置：
 * - EXTI_CONFIGS: EXTI配置（PB0和PB1，双边沿触发）
 * - LED_CONFIGS: LED配置（PA1）
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8/9，可选）
 * - UART_CONFIGS: UART配置（PA9/10，115200）
 * 
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/EXTI/EXTI02_RotaryEncoder_Counter/Examples.uvprojx
 * 2. 根据实际硬件修改 board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "system_init.h"
#include "exti.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "board.h"
#include "gpio.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"  /* 用于GPIO_EXTILineConfig */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* 计数器（volatile，中断和主循环都会访问） */
static volatile int32_t g_counter = 0;

/* 编码器上一次状态（用于状态机判断方向） */
/* 状态编码：bit0=通道A(PB0)，bit1=通道B(PB1) */
static volatile uint8_t g_encoder_last_state = 0xFF;  /* 初始化为无效状态 */

/**
 * @brief 编码器状态机处理函数（根据状态变化判断方向）
 * @param current_state 当前状态（bit0=通道A，bit1=通道B）
 * @note 编码器旋转一个步进会产生4个状态变化，但只在特定状态变化时计数一次
 *       正转：00 -> 01 -> 11 -> 10 -> 00（只在00->01时计数+1）
 *       反转：00 -> 10 -> 11 -> 01 -> 00（只在00->10时计数-1）
 */
static void Encoder_ProcessState(uint8_t current_state)
{
    uint8_t last_state = g_encoder_last_state;
    
    /* 如果上一次状态无效，直接更新状态，不判断方向 */
    if (last_state == 0xFF)
    {
        g_encoder_last_state = current_state;
        return;
    }
    
    /* 如果状态没有变化，忽略 */
    if (current_state == last_state)
    {
        return;
    }
    
    /* 根据状态变化模式判断方向 */
    /* 编码器状态变化模式（取决于接线，这里假设标准接线） */
    /* 正转：00 -> 01 -> 11 -> 10 -> 00 */
    /* 反转：00 -> 10 -> 11 -> 01 -> 00 */
    /* 注意：每个步进有4个状态变化，但只在特定状态变化时计数一次 */
    /* 选择在00->01（正转）或00->10（反转）时计数，因为00是稳定状态 */
    
    /* 只在从00状态变化时计数（避免重复计数） */
    if (last_state == 0x00)
    {
        if (current_state == 0x01)  /* 00 -> 01（正转开始） */
        {
            g_counter++;  /* 正转：+1 */
        }
        else if (current_state == 0x02)  /* 00 -> 10（反转开始） */
        {
            g_counter--;  /* 反转：-1 */
        }
        /* 其他状态变化不计数（避免重复计数） */
    }
    /* 其他状态变化不计数（这些是中间状态变化） */
    
    /* 更新上一次状态 */
    g_encoder_last_state = current_state;
}

/**
 * @brief EXTI0中断回调函数（编码器通道A：PB0）
 * @param line EXTI线号
 * @param user_data 用户数据
 */
static void EncoderA_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    
    /* 读取两个通道的当前状态 */
    uint8_t state_a = GPIO_ReadPin(GPIOB, GPIO_Pin_0) ? 1 : 0;  /* 通道A状态 */
    uint8_t state_b = GPIO_ReadPin(GPIOB, GPIO_Pin_1) ? 1 : 0;  /* 通道B状态 */
    uint8_t current_state = (state_a) | (state_b << 1);  /* 组合状态：bit0=A, bit1=B */
    
    /* 使用状态机判断方向 */
    Encoder_ProcessState(current_state);
}

/**
 * @brief EXTI1中断回调函数（编码器通道B：PB1）
 * @param line EXTI线号
 * @param user_data 用户数据
 */
static void EncoderB_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    
    /* 读取两个通道的当前状态 */
    uint8_t state_a = GPIO_ReadPin(GPIOB, GPIO_Pin_0) ? 1 : 0;  /* 通道A状态 */
    uint8_t state_b = GPIO_ReadPin(GPIOB, GPIO_Pin_1) ? 1 : 0;  /* 通道B状态 */
    uint8_t current_state = (state_a) | (state_b << 1);  /* 组合状态：bit0=A, bit1=B */
    
    /* 使用状态机判断方向 */
    Encoder_ProcessState(current_state);
}

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    EXTI_Status_t exti_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待UART稳定 */
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* 等待Debug模块稳定 */
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;        /* 日志级别：DEBUG（显示所有日志） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳（简化输出） */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
        /* Log初始化失败，但可以继续运行（使用UART直接输出） */
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== EXTI02 旋转编码器计数示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "EXTI02 Demo");
        OLED_ShowString(2, 1, "Rotary Enc");
        OLED_ShowString(3, 1, "Counter: 0");
        OLED_ShowString(4, 1, "Dir: --");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    else
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    /* ========== 步骤8：初始化EXTI0（PB0，编码器通道A） ========== */
    exti_status = EXTI_HW_Init(EXTI_LINE_0, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI0初始化失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        /* EXTI初始化失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }
    
    /* 重新配置PB0为上拉输入（EXTI_HW_Init会将GPIO配置为浮空输入，需要重新配置为上拉） */
    GPIO_Config(GPIOB, GPIO_Pin_0, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz);
    
    /* 设置EXTI0中断回调函数 */
    exti_status = EXTI_SetCallback(EXTI_LINE_0, EncoderA_Callback, NULL);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI0回调设置失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 使能EXTI0中断 */
    exti_status = EXTI_Enable(EXTI_LINE_0);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI0使能失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    LOG_INFO("MAIN", "EXTI0已初始化: PB0（编码器通道A），双边沿触发");
    
    /* ========== 步骤9：初始化EXTI1（PB1，编码器通道B） ========== */
    exti_status = EXTI_HW_Init(EXTI_LINE_1, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI1初始化失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        /* EXTI初始化失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }
    
    /* 重新配置PB1为上拉输入（EXTI_HW_Init会将GPIO配置为浮空输入，需要重新配置为上拉） */
    GPIO_Config(GPIOB, GPIO_Pin_1, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz);
    
    /* 设置EXTI1中断回调函数 */
    exti_status = EXTI_SetCallback(EXTI_LINE_1, EncoderB_Callback, NULL);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI1回调设置失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 使能EXTI1中断 */
    exti_status = EXTI_Enable(EXTI_LINE_1);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI1使能失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    LOG_INFO("MAIN", "EXTI1已初始化: PB1（编码器通道B），双边沿触发");
    
    /* 初始化编码器状态（读取初始状态） */
    {
        uint8_t state_a = GPIO_ReadPin(GPIOB, GPIO_Pin_0) ? 1 : 0;
        uint8_t state_b = GPIO_ReadPin(GPIOB, GPIO_Pin_1) ? 1 : 0;
        g_encoder_last_state = (state_a) | (state_b << 1);
    }
    
    LOG_INFO("MAIN", "编码器初始化完成，开始检测旋转方向");
    
    /* ========== 步骤10：主循环 ========== */
    int32_t last_counter = 0;
    char direction_str[4] = "--";
    
    while(1)
    {
        /* 检查计数器是否变化 */
        if (g_counter != last_counter)
        {
            /* 判断方向（根据计数器变化） */
            if (g_counter > last_counter)
            {
                strcpy(direction_str, "CW ");  /* 顺时针（正转） */
            }
            else
            {
                strcpy(direction_str, "CCW"); /* 逆时针（反转） */
            }
            
            /* 更新OLED显示 */
            OLED_ShowSignedNum(3, 10, g_counter, 5);  /* 使用有符号数显示，支持负数 */
            OLED_ShowString(4, 5, direction_str);
            
            /* UART日志输出 */
            LOG_INFO("ENCODER", "计数器: %d, 方向: %s", g_counter, direction_str);
            
            /* LED闪烁反馈 */
            LED1_Toggle();
            
            /* 更新last_counter */
            last_counter = g_counter;
        }
        
        /* 延时降低CPU占用率 */
        Delay_ms(10);
    }
}

