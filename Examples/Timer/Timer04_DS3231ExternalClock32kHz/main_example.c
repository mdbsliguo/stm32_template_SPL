/**
 * @file main_example.c
 * @brief Timer04 - DS3231外部时钟32kHz示例
 * @details 演示使用DS3231的32K引脚输出32kHz时钟作为TIM1定时器的外部时钟源
 * @note 重要：在STM32F103C8T6中，TIM3没有ETR引脚，只有TIM1和TIM2有ETR引脚
 * @note TIM2的ETR引脚是PA0，TIM1的ETR引脚是PA12
 * @note ?? 警告：TIM2已被TIM2_TimeBase模块占用（系统时间基准），不能用于外部时钟！
 * @note 因此本案例使用TIM1，TIM1的ETR引脚是PA12（不是PA0！）
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用DS3231的32K引脚输出32kHz时钟
 * - 配置TIM1使用ETR引脚接收外部时钟（DS3231的32K输出）
 * - 实时读取TIM1->CNT寄存器值，观察外部时钟驱动的计数效果
 * - OLED显示计数值、溢出次数、外部时钟频率
 * - UART输出详细日志（中文，GB2312编码）
 *
 * 硬件要求：
 * - DS3231实时时钟模块（I2C接口）
 *   - SCL连接到PB10（软件I2C2）
 *   - SDA连接到PB11（软件I2C2）
 *   - 32K引脚连接到PA12（TIM1的ETR引脚）
 *   - VCC连接到3.3V
 *   - GND连接到GND
 * - USART1：PA9(TX), PA10(RX)，115200波特率
 * - OLED显示屏（SSD1306，I2C接口）：PB8(SCL), PB9(SDA)
 *
 * 技术要点：
 * - DS3231使能32kHz输出（DS3231_Enable32kHz）
 * - 配置PA12为输入模式（TIM1_ETR）
 * - TIM1配置为外部时钟模式2（TIM_ETRClockMode2Config）
 * - ETR引脚（PA12）接收DS3231的32K输出作为时钟源
 * - 外部时钟频率：32kHz（每秒计数32000次）
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
#include "i2c_sw.h"
#include "ds3231.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "TIM2_TimeBase.h"
#include "board.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/* TIM1状态标志 */
static bool g_tim1_initialized = false;

/* TIM1统计信息 */
static uint32_t g_tim1_overflow_count = 0;  /* 溢出次数（CNT从ARR回到0的次数） */
static uint16_t g_tim1_last_count = 0;       /* 上次计数值，用于检测溢出 */

/**
 * @brief 初始化TIM1硬件定时器（外部时钟源：DS3231 32kHz输出）
 * @return error_code_t 错误码（ERROR_OK表示成功）
 * @note 配置PA12为浮空输入（TIM1_ETR功能）
 * @note 配置TIM1使用ETR引脚（PA12）作为外部时钟源
 * @note ETR引脚接收DS3231的32K输出（32kHz时钟）
 * @note PSC=0, ARR=999，外部时钟频率32kHz，每31.25ms计数一次
 * @note 注意：ETR是输入信号，GPIO应配置为输入模式（浮空输入）
 * @note 重要：在STM32F103C8T6中，TIM3没有ETR引脚，只有TIM1和TIM2有ETR引脚
 * @note TIM2的ETR引脚是PA0，TIM1的ETR引脚是PA12
 * @note ?? 警告：TIM2已被TIM2_TimeBase模块占用（系统时间基准），不能用于外部时钟！
 * @note 因此本案例使用TIM1，TIM1的ETR引脚是PA12（不是PA0！）
 */
static error_code_t TIM1_ExternalClock_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    uint16_t psc = 0;       /* 预分频器：外部时钟不分频 */
    uint16_t arr = 999;     /* 自动重装载值：0-999计数范围 */
    
    /* 检查是否已初始化 */
    if (g_tim1_initialized)
    {
        return ERROR_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* ========== 步骤1：使能GPIOA和AFIO时钟 ========== */
    /* ETR引脚需要配置为复用功能，需要使能AFIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    
    /* ========== 步骤2：配置PA12为浮空输入（TIM1_ETR功能） ========== */
    /* 注意：TIM1的ETR引脚是PA12，ETR是输入信号，需要配置为输入模式 */
    /* ETR信号通过AFIO连接到定时器，但GPIO本身应配置为输入模式 */
    /* 使用浮空输入模式，因为DS3231的32K输出是推挽输出，不需要上拉 */
    GPIO_InitStructure.GPIO_Pin = TIM1_ETR_PIN;  /* PA12 = TIM1_ETR */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  /* 浮空输入 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TIM1_ETR_PORT, &GPIO_InitStructure);
    
    /* ========== 步骤3：使能TIM1时钟 ========== */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    
    /* 等待时钟稳定 */
    volatile uint32_t wait = 100;
    while (wait--);
    
    /* ========== 步骤4：先禁用TIM1（如果已启动） ========== */
    /* 注意：配置外部时钟前必须先禁用定时器 */
    TIM_Cmd(TIM1, DISABLE);
    
    /* ========== 步骤5：先配置TIM1时间基准（必须在ETR配置之前） ========== */
    /* 注意：必须先配置TimeBase，再配置ETR外部时钟，否则TimeBaseInit可能覆盖ETR配置 */
    TIM_TimeBaseStructure.TIM_Period = arr;                    /* 自动重装载值 */
    TIM_TimeBaseStructure.TIM_Prescaler = psc;                 /* 预分频器 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;  /* 时钟分频 */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  /* 向上计数模式 */
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    
    /* 清除更新标志 */
    TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
    
    /* ========== 步骤6：配置TIM1使用ETR作为外部时钟源（模式2） ========== */
    /* ETR引脚接收DS3231的32K输出（32kHz时钟） */
    /* 注意：必须在TimeBaseInit之后配置ETR，否则ETR配置可能被覆盖 */
    /* 注意：TIM_ETRClockMode2Config只设置ECE位，模式2不需要设置SMS和TS */
    TIM_ETRClockMode2Config(TIM1,
                            TIM_ExtTRGPSC_OFF,              /* 不分频 */
                            TIM_ExtTRGPolarity_NonInverted,  /* 上升沿有效 */
                            0);                             /* 无滤波 */
    
    /* ========== 步骤7：清零计数器 ========== */
    TIM_SetCounter(TIM1, 0);
    
    /* ========== 步骤8：启动TIM1 ========== */
    TIM_Cmd(TIM1, ENABLE);
    
    g_tim1_initialized = true;
    
    return ERROR_OK;
}

/**
 * @brief 读取TIM1当前计数值
 * @param[out] overflow_count 溢出次数（可选，传NULL则忽略）
 * @return uint16_t TIM1的CNT寄存器值（0-999，循环计数）
 * @note TIM1是16位定时器，计数值从0到ARR自动循环
 * @note 通过比较当前值和上次值，检测溢出事件
 */
static uint16_t TIM1_GetCount(uint32_t *overflow_count)
{
    uint16_t current_count;
    
    if (!g_tim1_initialized)
    {
        if (overflow_count != NULL)
        {
            *overflow_count = 0;
        }
        return 0;
    }
    
    current_count = TIM1->CNT;  /* 读取TIM1计数值 */
    
    /* 检测溢出：如果当前值小于上次值，说明发生了溢出（从ARR回到0） */
    if (current_count < g_tim1_last_count)
    {
        g_tim1_overflow_count++;
    }
    g_tim1_last_count = current_count;
    
    if (overflow_count != NULL)
    {
        *overflow_count = g_tim1_overflow_count;
    }
    
    return current_count;
}

/**
 * @brief 更新OLED显示
 * @param[in] count_value TIM1当前计数值
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
        OLED_ShowString(1, 1, "Timer04 Demo");
        OLED_ShowString(2, 1, "DS3231 32kHz");
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
    SoftI2C_Status_t soft_i2c_status;
    DS3231_Status_t ds3231_status;
    DS3231_Config_t ds3231_config;
    error_code_t tim1_status;
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
    LOG_INFO("MAIN", "=== Timer04 DS3231外部时钟32kHz示例 ===");
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
        OLED_ShowString(1, 1, "Timer04 Init");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED已初始化");
    }
    else
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    /* ========== 步骤8：初始化软件I2C（用于DS3231） ========== */
    LOG_INFO("MAIN", "初始化软件I2C2（PB10/11，用于DS3231）");
    soft_i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_2);
    if (soft_i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C2初始化失败: %d", soft_i2c_status);
        ErrorHandler_Handle(soft_i2c_status, "SoftI2C");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "I2C Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "软件I2C2初始化成功");
    
    /* ========== 步骤9：初始化DS3231 ========== */
    LOG_INFO("MAIN", "初始化DS3231模块");
    ds3231_config.interface_type = DS3231_INTERFACE_SOFTWARE;
    ds3231_config.config.software.soft_i2c_instance = SOFT_I2C_INSTANCE_2;
    
    ds3231_status = DS3231_Init(&ds3231_config);
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231初始化失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "DS3231 Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231初始化成功");
    
    /* ========== 步骤10：使能DS3231的32kHz输出 ========== */
    LOG_INFO("MAIN", "使能DS3231的32kHz输出");
    ds3231_status = DS3231_Enable32kHz();
    if (ds3231_status != DS3231_OK)
    {
        LOG_ERROR("MAIN", "DS3231 32kHz输出使能失败: %d", ds3231_status);
        ErrorHandler_Handle(ds3231_status, "DS3231");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "32kHz Enable Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    LOG_INFO("MAIN", "DS3231 32kHz输出已使能（32K引脚）");
    LOG_INFO("MAIN", "硬件连接: DS3231 32K引脚 -> TIM1 ETR引脚（PA12）");
    LOG_WARN("MAIN", "?? 注意：TIM2已被TIM2_TimeBase模块占用，不能用于外部时钟！");
    LOG_WARN("MAIN", "?? 因此使用TIM1，TIM1的ETR引脚是PA12（不是PA0！）");
    
    Delay_ms(500);
    
    /* ========== 步骤11：初始化TIM1外部时钟 ========== */
    LOG_INFO("MAIN", "初始化TIM1外部时钟（ETR模式）");
    tim1_status = TIM1_ExternalClock_Init();
    if (tim1_status == ERROR_OK)
    {
        LOG_INFO("MAIN", "TIM1外部时钟已初始化");
        LOG_INFO("MAIN", "TIM1配置: ETR外部时钟源（PA12），PSC=0, ARR=999");
        LOG_INFO("MAIN", "外部时钟频率: 32kHz（DS3231 32K输出）");
        LOG_INFO("MAIN", "TIM1计数范围: 0-999（每31.25ms计数一次）");
        
        /* 验证TIM1配置 */
        LOG_INFO("MAIN", "TIM1配置验证:");
        LOG_INFO("MAIN", "  CR1=0x%04X (CEN=%d)", TIM1->CR1, (TIM1->CR1 & TIM_CR1_CEN) ? 1 : 0);
        LOG_INFO("MAIN", "  SMCR=0x%04X", TIM1->SMCR);
        LOG_INFO("MAIN", "    SMS=%d (Slave Mode)", (TIM1->SMCR & TIM_SMCR_SMS) >> 0);
        LOG_INFO("MAIN", "    TS=%d (Trigger Selection)", (TIM1->SMCR & TIM_SMCR_TS) >> 4);
        LOG_INFO("MAIN", "    ECE=%d (External Clock Enable)", (TIM1->SMCR & TIM_SMCR_ECE) ? 1 : 0);
        LOG_INFO("MAIN", "    ETP=%d (ETR Polarity)", (TIM1->SMCR & TIM_SMCR_ETP) ? 1 : 0);
        LOG_INFO("MAIN", "    ETF=%d (ETR Filter)", (TIM1->SMCR & 0x0F00) >> 8);
        LOG_INFO("MAIN", "    ETPS=%d (ETR Prescaler)", (TIM1->SMCR & 0x0030) >> 4);
        LOG_INFO("MAIN", "  CNT=%d", TIM1->CNT);
        LOG_INFO("MAIN", "  PSC=%d, ARR=%d", TIM1->PSC, TIM1->ARR);
        
        /* 检查PA12引脚状态（用于调试） */
        LOG_INFO("MAIN", "PA12引脚状态: IDR=0x%04X (Bit12=%d)", GPIOA->IDR, (GPIOA->IDR & GPIO_Pin_12) ? 1 : 0);
        
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "TIM1 Ready");
        }
    }
    else
    {
        LOG_ERROR("MAIN", "TIM1初始化失败: %d", tim1_status);
        ErrorHandler_Handle(tim1_status, "TIM1");
        if (oled_status == OLED_OK)
        {
            OLED_ShowString(3, 1, "TIM1 Init Fail!");
        }
        while(1) { Delay_ms(1000); }
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤12：主循环 ========== */
    LOG_INFO("MAIN", "=== 开始读取TIM1计数值（外部时钟：32kHz） ===");
    LOG_INFO("MAIN", "TIM1工作原理说明：");
    LOG_INFO("MAIN", "  - TIM1使用外部时钟源（DS3231 32K输出，32kHz时钟）");
    LOG_INFO("MAIN", "  - ETR引脚（PA12）接收DS3231的32K输出作为时钟源");
    LOG_INFO("MAIN", "  - PA12已配置为输入模式（TIM1_ETR）");
    LOG_INFO("MAIN", "  - 外部时钟频率：32kHz（每秒32000个上升沿）");
    LOG_INFO("MAIN", "  - TIM1每接收到一个上升沿，CNT寄存器加1");
    LOG_INFO("MAIN", "  - CNT从0计数到999，然后自动回到0（溢出）");
    LOG_INFO("MAIN", "  - 每31.25ms完成一次0-999计数循环（1000/32000秒）");
    
    start_time = TIM2_TimeBase_GetTick();
    
    while (1)
    {
        uint32_t current_time;
        uint32_t run_time_ms;
        
        /* 读取TIM1当前计数值和溢出次数 */
        count_value = TIM1_GetCount(&overflow_count);
        
        /* 计算运行时间 */
        current_time = TIM2_TimeBase_GetTick();
        run_time_ms = current_time - start_time;
        
        /* 更新OLED显示（每100ms更新一次，32kHz时钟更新很快） */
        if ((current_time - last_oled_update_time) >= 100)
        {
            UpdateOLEDDisplay(count_value, overflow_count, run_time_ms);
            last_oled_update_time = current_time;
        }
        
        /* UART输出详细日志（每1秒输出一次） */
        if ((current_time - last_log_time) >= 1000)
        {
            uint8_t percent = (count_value * 100) / 1000;
            uint32_t overflow_per_sec = overflow_count * 1000 / (run_time_ms > 0 ? run_time_ms : 1);  /* 每秒溢出次数 */
            
            LOG_INFO("TIM1", "=== TIM1状态（外部时钟：32kHz） ===");
            LOG_INFO("TIM1", "当前计数值: %d / 999 (%d%%)", count_value, percent);
            LOG_INFO("TIM1", "溢出次数: %lu (约%lu次/秒)", overflow_count, overflow_per_sec);
            LOG_INFO("TIM1", "运行时间: %lu.%03lu 秒", run_time_ms / 1000, run_time_ms % 1000);
            LOG_INFO("TIM1", "外部时钟频率: 32kHz（DS3231 32K输出）");
            LOG_INFO("TIM1", "计数速度: 每31.25ms完成一次0-999计数");
            
            /* 调试信息：检查TIM1状态寄存器 */
            LOG_INFO("TIM1", "TIM1寄存器状态:");
            LOG_INFO("TIM1", "  CR1=0x%04X (CEN=%d)", TIM1->CR1, (TIM1->CR1 & TIM_CR1_CEN) ? 1 : 0);
            LOG_INFO("TIM1", "  SMCR=0x%04X (SMS=%d, ETP=%d, ECE=%d)", 
                     TIM1->SMCR, 
                     (TIM1->SMCR & TIM_SMCR_SMS) >> 0,
                     (TIM1->SMCR & TIM_SMCR_ETP) ? 1 : 0,
                     (TIM1->SMCR & TIM_SMCR_ECE) ? 1 : 0);
            LOG_INFO("TIM1", "  CNT=%d (直接读取)", TIM1->CNT);
            LOG_INFO("TIM1", "  PSC=%d, ARR=%d", TIM1->PSC, TIM1->ARR);
            
            /* 检查PA12引脚状态 */
            LOG_INFO("TIM1", "PA12引脚状态: IDR=0x%04X (Bit12=%d)", GPIOA->IDR, (GPIOA->IDR & GPIO_Pin_12) ? 1 : 0);
            
            LOG_INFO("TIM1", "---");
            last_log_time = current_time;
        }
        
        /* 延时降低CPU占用率 */
        Delay_ms(10);
    }
}
