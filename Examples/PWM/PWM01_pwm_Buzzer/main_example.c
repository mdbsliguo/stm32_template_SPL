/**
 * @file main_example.c
 * @brief 案例 - PWM模式Buzzer控制示例
 * @example Examples/PWM/PWM01_pwm_Buzzer/main_example.c
 * @status 已调试完成 ?
 * @details Buzzer PWM模式控制示例，演示频率控制、音调播放等功能，使用OLED显示提示信息
 *
 * 硬件要求：
 * - Buzzer（无源蜂鸣器）连接到PA6（TIM3 CH1）
 * - OLED显示屏（SSD1306，I2C接口，用于显示提示信息）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 *
 * 硬件配置：
 * 在 Examples/PWM/PWM01_pwm_Buzzer/board.h 中配置：
 * ```c
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1},  // PWM模式，TIM3(实例1)通道1，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/PWM/PWM01_pwm_Buzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/PWM/PWM01_pwm_Buzzer/board.h 中的Buzzer配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include <stdint.h>
#include <string.h>

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    Buzzer_Status_t buzzer_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        /* UART初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        /* Debug初始化失败，无法继续，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
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
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* Buzzer初始化（根据配置表自动初始化所有enabled=1的Buzzer） */
    buzzer_status = Buzzer_Init();
    if (buzzer_status != BUZZER_OK)
    {
        LOG_ERROR("MAIN", "Buzzer初始化失败: %d", buzzer_status);
        ErrorHandler_Handle(buzzer_status, "BUZZER");
        /* Buzzer初始化失败，但可以继续运行（OLED仍可显示） */
    }
    else
    {
        LOG_INFO("MAIN", "Buzzer已初始化: PWM模式，TIM3 CH1");
    }

    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
        /* I2C初始化失败，OLED无法使用，但Buzzer仍可工作 */
        while (1)
        {
            BUZZER1_Beep(100);
            Delay_ms(200);
        }
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C已初始化: PB8(SCL), PB9(SDA)");
    }

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
        /* OLED初始化失败，但可以继续运行（Buzzer仍可工作） */
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "System Init OK");
        OLED_ShowString(2, 1, "UART Ready");
        OLED_ShowString(3, 1, "Log Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }

    /* 清屏并显示标题 */
    OLED_Clear();
    OLED_ShowString(1, 1, "Buzzer Demo");
    OLED_ShowString(2, 1, "PWM Mode");
    OLED_ShowString(3, 1, "TIM3 CH1");
    LOG_INFO("MAIN", "=== Buzzer PWM模式演示开始 ===");
    Delay_ms(1000);

    /* ========== 步骤8：主循环 ========== */
    /* 主循环：演示Buzzer的各种PWM功能 */
    while (1)
    {
        /* 示例1：频率控制 - 不同频率的鸣响 */
        LOG_INFO("MAIN", "示例1：频率控制演示");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 1:");
        OLED_ShowString(2, 1, "Frequency");
        OLED_ShowString(3, 1, "500Hz...");
        LOG_DEBUG("BUZZER", "设置频率: 500Hz");
        buzzer_status = Buzzer_SetFrequency(BUZZER_1, 500);  // 设置500Hz
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "设置频率失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(200);
        
        OLED_ShowString(3, 1, "1000Hz...");
        LOG_DEBUG("BUZZER", "设置频率: 1000Hz");
        buzzer_status = Buzzer_SetFrequency(BUZZER_1, 1000);  // 设置1kHz
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "设置频率失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(200);
        
        OLED_ShowString(3, 1, "2000Hz...");
        LOG_DEBUG("BUZZER", "设置频率: 2000Hz");
        buzzer_status = Buzzer_SetFrequency(BUZZER_1, 2000);  // 设置2kHz
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "设置频率失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        BUZZER1_On();
        Delay_ms(500);
        BUZZER1_Off();
        Delay_ms(500);
        
        /* 示例2：音调播放 - 播放C4-C5音阶 */
        LOG_INFO("MAIN", "示例2：音调播放 - C4-C5音阶");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 2:");
        OLED_ShowString(2, 1, "Play Scale");
        OLED_ShowString(3, 1, "C4...");
        LOG_DEBUG("BUZZER", "播放音调: C4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "D4...");
        LOG_DEBUG("BUZZER", "播放音调: D4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_D4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "E4...");
        LOG_DEBUG("BUZZER", "播放音调: E4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_E4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "F4...");
        LOG_DEBUG("BUZZER", "播放音调: F4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_F4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "G4...");
        LOG_DEBUG("BUZZER", "播放音调: G4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "A4...");
        LOG_DEBUG("BUZZER", "播放音调: A4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "B4...");
        LOG_DEBUG("BUZZER", "播放音调: B4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_B4, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(100);
        
        OLED_ShowString(3, 1, "C5...");
        LOG_DEBUG("BUZZER", "播放音调: C5");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C5, 300);
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(500);
        
        /* 示例3：播放简单旋律（小星星前奏） */
        LOG_INFO("MAIN", "示例3：播放简单旋律（小星星前奏）");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 3:");
        OLED_ShowString(2, 1, "Melody");
        OLED_ShowString(3, 1, "Playing...");
        Delay_ms(500);
        
        /* 小星星：C4-C4-G4-G4-A4-A4-G4 */
        LOG_DEBUG("BUZZER", "播放旋律: 小星星前奏");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_C4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 300);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(50);
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_G4, 600);
        if (buzzer_status != BUZZER_OK) ErrorHandler_Handle(buzzer_status, "BUZZER");
        Delay_ms(1000);
        
        /* 示例4：频率扫描效果 */
        LOG_INFO("MAIN", "示例4：频率扫描效果（200Hz-2000Hz）");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 4:");
        OLED_ShowString(2, 1, "Freq Sweep");
        OLED_ShowString(3, 1, "Sweeping...");
        Delay_ms(500);
        
        /* 从200Hz扫描到2000Hz */
        LOG_DEBUG("BUZZER", "开始频率扫描: 200Hz -> 2000Hz");
        for (uint32_t freq = 200; freq <= 2000; freq += 50)
        {
            buzzer_status = Buzzer_SetFrequency(BUZZER_1, freq);
            if (buzzer_status != BUZZER_OK)
            {
                LOG_ERROR("BUZZER", "设置频率失败: %d Hz, error: %d", freq, buzzer_status);
                ErrorHandler_Handle(buzzer_status, "BUZZER");
            }
            BUZZER1_On();
            Delay_ms(20);
            BUZZER1_Off();
            Delay_ms(5);
        }
        LOG_DEBUG("BUZZER", "频率扫描完成");
        Delay_ms(500);
        
        /* 示例5：持续播放音调（需要手动停止） */
        LOG_INFO("MAIN", "示例5：持续播放音调（A4，1秒后停止）");
        OLED_Clear();
        OLED_ShowString(1, 1, "Example 5:");
        OLED_ShowString(2, 1, "Continuous");
        OLED_ShowString(3, 1, "A4 Tone...");
        LOG_DEBUG("BUZZER", "持续播放音调: A4");
        buzzer_status = Buzzer_PlayTone(BUZZER_1, BUZZER_TONE_A4, 0);  // 0表示持续播放
        if (buzzer_status != BUZZER_OK)
        {
            LOG_ERROR("BUZZER", "播放音调失败: %d", buzzer_status);
            ErrorHandler_Handle(buzzer_status, "BUZZER");
        }
        Delay_ms(1000);
        BUZZER1_Stop();  // 手动停止
        LOG_DEBUG("BUZZER", "停止播放");
        Delay_ms(500);
        
        LOG_INFO("MAIN", "=== 一轮演示完成，开始下一轮 ===");
    }
}


