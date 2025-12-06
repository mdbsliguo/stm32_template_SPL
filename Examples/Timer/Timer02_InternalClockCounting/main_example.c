/**
 * @file main_example.c
 * @brief Timer02 - 内部时钟计数示例
 * @details 演示使用硬件定时器TIM3进行内部时钟计数，通过读取CNT寄存器获取计数值
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用TIM3硬件定时器，配置为内部时钟源自动计数
 * - 实时读取TIM3->CNT寄存器值，获取当前计数值
 * - OLED显示计数值（实时更新）
 * - UART输出详细日志（中文，GB2312编码）
 *
 * 硬件要求：
 * - TIM3：硬件定时器（内部时钟源，无需外部输入）
 * - USART1：PA9(TX), PA10(RX)，115200波特率
 * - OLED显示屏（SSD1306，I2C接口）：PB8(SCL), PB9(SDA)
 *
 * 技术要点：
 * - TIM3配置为内部时钟源（TIM_InternalClockConfig）
 * - 设置预分频器（PSC）和自动重装载值（ARR）
 * - 配置为向上计数模式（TIM_CounterMode_Up）
 * - 启动TIM3后，CNT寄存器自动递增
 * - 计数值会从0到ARR自动循环（16位定时器，最大65535）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "TIM2_TimeBase.h"
#include "board.h"
#include <stdint.h>
#include <string.h>

/* TIM3状态标志 */
static bool g_tim3_initialized = false;

/* TIM3统计信息 */
static uint32_t g_tim3_overflow_count = 0;  /* 溢出次数（CNT从ARR回到0的次数） */
static uint16_t g_tim3_last_count = 0;       /* 上次计数值，用于检测溢出 */

/**
 * @brief 初始化TIM3硬件定时器（内部时钟计数）
 * @return error_code_t 错误码（ERROR_OK表示成功）
 * @note 配置TIM3为内部时钟源，自动计数
 * @note PSC=71, ARR=999，实现约1ms计数周期（72MHz系统时钟）
 */
static error_code_t TIM3_InternalClock_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint16_t psc = 71;      /* 预分频器：72MHz / 72 = 1MHz */
    uint16_t arr = 999;     /* 自动重装载值：1MHz / 1000 = 1kHz（1ms周期） */
    
    /* 检查是否已初始化 */
    if (g_tim3_initialized)
    {
        return ERROR_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 使能TIM3时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    /* 等待时钟稳定 */
    volatile uint32_t wait = 100;
    while (wait--);
    
    /* 配置TIM3为内部时钟源 */
    TIM_InternalClockConfig(TIM3);
    
    /* 配置TIM3时间基准 */
    TIM_TimeBaseStructure.TIM_Period = arr;                    /* 自动重装载值 */
    TIM_TimeBaseStructure.TIM_Prescaler = psc;                 /* 预分频器 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;  /* 时钟分频 */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  /* 向上计数模式 */
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    
    /* 清除更新标志 */
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    
    /* 启动TIM3 */
    TIM_Cmd(TIM3, ENABLE);
    
    g_tim3_initialized = true;
    
    return ERROR_OK;
}

/**
 * @brief 读取TIM3当前计数值
 * @param[out] overflow_count 溢出次数（可选，传NULL则忽略）
 * @return uint16_t TIM3的CNT寄存器值（0-999，循环计数）
 * @note TIM3是16位定时器，计数值从0到ARR自动循环
 * @note 通过比较当前值和上次值，检测溢出事件
 */
static uint16_t TIM3_GetCount(uint32_t *overflow_count)
{
    uint16_t current_count;
    
    if (!g_tim3_initialized)
    {
        if (overflow_count != NULL)
        {
            *overflow_count = 0;
        }
        return 0;
    }
    
    current_count = TIM3->CNT;  /* 读取TIM3计数值 */
    
    /* 检测溢出：如果当前值小于上次值，说明发生了溢出（从ARR回到0） */
    if (current_count < g_tim3_last_count)
    {
        g_tim3_overflow_count++;
    }
    g_tim3_last_count = current_count;
    
    if (overflow_count != NULL)
    {
        *overflow_count = g_tim3_overflow_count;
    }
    
    return current_count;
}

/**
 * @brief 更新OLED显示
 * @param[in] count_value TIM3当前计数值
 * @param[in] overflow_count 溢出次数
 * @param[in] run_time_ms 运行时间（毫秒）
 */
static void UpdateOLEDDisplay(uint16_t count_value, uint32_t overflow_count, uint32_t run_time_ms)
{
    static bool first_update = true;
    uint8_t percent;  /* 计数进度百分比 */
    
    if (first_update)
    {
        /* 首次更新：显示标题和配置信息 */
        OLED_Clear();
        OLED_ShowString(1, 1, "TIM3 Timer Demo");
        OLED_ShowString(2, 1, "PSC=71 ARR=999");
        first_update = false;
    }
    
    /* 第3行：显示当前计数值和进度百分比 */
    /* 格式：CNT:123 (12%) */
    percent = (count_value * 100) / 1000;  /* ARR+1 = 1000 */
    OLED_ShowString(3, 1, "CNT:");
    OLED_ShowNum(3, 5, count_value, 3);
    OLED_ShowString(3, 9, "(");
    OLED_ShowNum(3, 10, percent, 2);
    OLED_ShowString(3, 12, "%)");
    
    /* 第4行：显示溢出次数和运行时间 */
    /* 格式：OVF:123 T:12s */
    OLED_ShowString(4, 1, "OVF:");
    if (overflow_count < 1000)
    {
        OLED_ShowNum(4, 5, overflow_count, 3);
    }
    else
    {
        OLED_ShowNum(4, 5, overflow_count, 4);
    }
    OLED_ShowString(4, 10, " T:");
    OLED_ShowNum(4, 13, run_time_ms / 1000, 2);
    OLED_ShowString(4, 15, "s");
}

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    error_code_t tim3_status;
    uint16_t count_value;
    uint32_t overflow_count = 0;
    uint32_t last_log_time = 0;
    uint32_t last_oled_update_time = 0;
    uint32_t start_time = 0;
    
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
    log_config.enable_module = 1;             /* 启用模块名显示 */
    log_config.enable_color = 0;              /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
        /* Log初始化失败，但可以继续运行（使用UART直接输出） */
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Timer02 内部时钟计数示例 ===");
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
        OLED_ShowString(1, 1, "Timer02 Init");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED已初始化");
    }
    else
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    /* ========== 步骤8：初始化TIM3硬件定时器 ========== */
    tim3_status = TIM3_InternalClock_Init();
    if (tim3_status == ERROR_OK)
    {
        LOG_INFO("MAIN", "TIM3硬件定时器已初始化");
        LOG_INFO("MAIN", "TIM3配置: 内部时钟源，PSC=71, ARR=999");
        LOG_INFO("MAIN", "TIM3计数周期: 约1ms（72MHz系统时钟）");
        LOG_INFO("MAIN", "TIM3计数范围: 0-999（自动循环）");
        
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "TIM3 Ready");
        }
    }
    else
    {
        LOG_ERROR("MAIN", "TIM3初始化失败: %d", tim3_status);
        ErrorHandler_Handle(tim3_status, "TIM3");
        /* TIM3初始化失败，进入死循环 */
        while(1) { Delay_ms(1000); }
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤9：主循环 ========== */
    LOG_INFO("MAIN", "=== 开始读取TIM3计数值 ===");
    LOG_INFO("MAIN", "TIM3工作原理说明：");
    LOG_INFO("MAIN", "  - TIM3使用内部时钟源（72MHz系统时钟）");
    LOG_INFO("MAIN", "  - 预分频器PSC=71，分频后时钟=72MHz/72=1MHz");
    LOG_INFO("MAIN", "  - 自动重装载值ARR=999，计数范围0-999");
    LOG_INFO("MAIN", "  - 计数周期=(PSC+1)*(ARR+1)/72MHz = 72*1000/72MHz = 1ms");
    LOG_INFO("MAIN", "  - CNT寄存器每1ms从0计数到999，然后自动回到0（溢出）");
    LOG_INFO("MAIN", "  - 每次溢出时，溢出计数器加1");
    
    start_time = TIM2_TimeBase_GetTick();
    
    while (1)
    {
        uint32_t current_time;
        uint32_t run_time_ms;
        
        /* 读取TIM3当前计数值和溢出次数 */
        count_value = TIM3_GetCount(&overflow_count);
        
        /* 计算运行时间 */
        current_time = TIM2_TimeBase_GetTick();
        run_time_ms = current_time - start_time;
        
        /* 更新OLED显示（每100ms更新一次，避免刷新过快） */
        if ((current_time - last_oled_update_time) >= 100)
        {
            UpdateOLEDDisplay(count_value, overflow_count, run_time_ms);
            last_oled_update_time = current_time;
        }
        
        /* UART输出详细日志（每1秒输出一次，避免输出过快） */
        if ((current_time - last_log_time) >= 1000)
        {
            uint8_t percent = (count_value * 100) / 1000;
            LOG_INFO("TIM3", "=== TIM3状态 ===");
            LOG_INFO("TIM3", "当前计数值: %d / 999 (%d%%)", count_value, percent);
            LOG_INFO("TIM3", "溢出次数: %lu (每1ms溢出一次)", overflow_count);
            LOG_INFO("TIM3", "运行时间: %lu.%03lu 秒", run_time_ms / 1000, run_time_ms % 1000);
            LOG_INFO("TIM3", "计数频率: 1kHz (每1ms完成一次0-999计数)");
            LOG_INFO("TIM3", "---");
            last_log_time = current_time;
        }
        
        /* 延时降低CPU占用率 */
        Delay_ms(10);
    }
}
