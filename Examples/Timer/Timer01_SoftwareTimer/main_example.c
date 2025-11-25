/**
 * @file main_example.c
 * @brief 案例 - 软件定时器示例
 * @example Examples/Timer/Timer01_SoftwareTimer/main_example.c
 * @status 待调试
 * @details 软件定时器功能演示，包括创建、启动、暂停/恢复、状态查询等功能
 *
 * 硬件要求：
 * - LED1连接到PA1（可选，用于视觉反馈）
 * - LED2连接到PA2（可选，用于视觉反馈）
 * - 按钮连接到PA4（暂停/继续控制，上拉输入）
 * - OLED显示屏（SSD1306，I2C接口）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 * 
 * 功能说明：
 * - 定时器1：1秒周期，周期模式，用于累计跑秒
 * - OLED显示累计运行秒数（跑秒）
 * - 按钮按下：暂停/恢复定时器
 *   - 暂停时：累计秒数停止累加
 *   - 恢复时：累计秒数继续累加
 * - 直观显示累计运行时间，方便观察定时器功能
 *
 * 硬件配置：
 * 在 Examples/Timer/Timer01_SoftwareTimer/board.h 中配置：
 * - LED_CONFIGS: LED配置表
 * - BUTTON_PORT/BUTTON_PIN: 按钮引脚定义
 * - OLED_I2C_CONFIG: OLED I2C配置
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Timer/Timer01_SoftwareTimer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Timer/Timer01_SoftwareTimer/board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 */

#include "system_init.h"
#include "TIM_sw.h"
#include "TIM2_TimeBase.h"  /* 用于非阻塞延时（定时器3演示） */
#include "led.h"
#include "gpio.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "stm32f10x.h"
#include "board.h"
#include <stddef.h>  /* 定义NULL */

/* 定时器句柄 */
static TIM_SW_Handle_t g_timer1 = TIM_SW_HANDLE_INVALID;

/* 按钮状态 */
static uint8_t g_button_last_state = Bit_SET;  /* 上次按钮状态（上拉输入，初始为高电平） */
static bool g_timers_paused = false;            /* 定时器暂停状态标志 */

/* 累计跑秒相关变量 */
static uint32_t g_total_elapsed_ms = 0;         /* 累计运行时间（毫秒），由软件定时器产生 */

/* 定时器1回调函数（1秒周期，用于累计跑秒） */
void timer1_callback(TIM_SW_Handle_t handle, void *user_data)
{
    /* 每1秒触发一次，累加1秒（1000ms） */
    /* 这是软件定时器产生的累计时间 */
    if (!g_timers_paused)
    {
        g_total_elapsed_ms += 1000;  /* 软件定时器每1秒触发一次，累加1秒 */
    }
    (void)handle;
    (void)user_data;
}

/**
 * @brief 更新累计运行时间（由软件定时器回调函数产生，无需手动更新）
 * @note 累计时间由软件定时器的回调函数timer1_callback()产生
 * @note 每1秒触发一次回调，累加1秒（1000ms）
 * @note 暂停时回调函数不会累加，恢复时继续累加
 */
static void UpdateElapsedTime(void)
{
    /* 累计时间由软件定时器的回调函数产生，这里不需要手动计算 */
    /* 软件定时器每1秒触发一次timer1_callback()，在回调中累加1秒 */
}

/**
 * @brief 更新OLED显示累计跑秒（时分秒格式）
 */
static void UpdateOLEDDisplay(void)
{
    uint32_t total_seconds;  /* 累计秒数 */
    uint32_t hours, minutes, seconds;  /* 时分秒 */
    static bool first_update = true;  /* 首次更新标志 */
    static bool last_pause_state = false;  /* 上次暂停状态 */
    
    /* 更新累计运行时间 */
    UpdateElapsedTime();
    
    /* 转换为秒数 */
    total_seconds = g_total_elapsed_ms / 1000;
    
    /* 转换为时分秒格式 */
    hours = total_seconds / 3600;        /* 小时 */
    minutes = (total_seconds % 3600) / 60; /* 分钟 */
    seconds = total_seconds % 60;         /* 秒 */
    
    /* 第1行：标题和暂停状态（只在状态变化或首次更新时刷新） */
    if (first_update || last_pause_state != g_timers_paused)
    {
        if (g_timers_paused)
        {
            OLED_ShowString(1, 1, "Timer [PAUSE]    ");
        }
        else
        {
            OLED_ShowString(1, 1, "Timer [RUN]      ");
        }
        last_pause_state = g_timers_paused;
    }
    
    /* 第2行：显示时分秒格式（00:00:00） */
    if (first_update)
    {
        OLED_ShowString(2, 1, "Time: ");
        OLED_ShowNum(2, 7, hours, 2);
        OLED_ShowString(2, 9, ":");
        OLED_ShowNum(2, 10, minutes, 2);
        OLED_ShowString(2, 12, ":");
        OLED_ShowNum(2, 13, seconds, 2);
    }
    else
    {
        /* 只更新时分秒数字部分 */
        OLED_ShowNum(2, 7, hours, 2);
        OLED_ShowNum(2, 10, minutes, 2);
        OLED_ShowNum(2, 13, seconds, 2);
    }
    
    /* 第3行：显示提示信息 */
    if (first_update)
    {
        OLED_ShowString(3, 1, "Press PA4 to    ");
        OLED_ShowString(4, 1, "pause/resume    ");
    }
    
    first_update = false;  /* 首次更新完成 */
}

/**
 * @brief 暂停定时器（暂停时不累加）
 */
static void PauseAllTimers(void)
{
    if (g_timer1 != TIM_SW_HANDLE_INVALID)
    {
        TIM_SW_Pause(g_timer1);
    }
    
    /* 暂停时，先更新累计时间（计算到暂停时刻的累计时间） */
    UpdateElapsedTime();
    
    g_timers_paused = true;
}

/**
 * @brief 恢复定时器（恢复时继续累加）
 */
static void ResumeAllTimers(void)
{
    if (g_timer1 != TIM_SW_HANDLE_INVALID)
    {
        TIM_SW_Resume(g_timer1);
    }
    
    /* 恢复时，软件定时器继续运行，回调函数会继续累加时间 */
    g_timers_paused = false;
}

int main(void)
{
    OLED_Status_t oled_status;
    uint8_t button_state;
    
    /* 系统初始化
     * System_Init() 会自动初始化：
     * - TIM2_TimeBase（时间基准，1ms中断）
     * - TIM_SW（软件定时器模块）
     * - SysTick延时模块（Delay_Init）
     * - LED驱动（根据配置表自动初始化所有enabled=1的LED）
     */
    System_Init();
    
    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，进入死循环 */
        while (1)
            ;
    }
    
    /* 配置按钮引脚为输入上拉模式
     * 上拉输入：按钮未按下时引脚为高电平，按下时引脚为低电平
     */
    GPIO_Config(BUTTON_PORT, BUTTON_PIN, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_2MHz);
    
    /* 创建软件定时器（1秒周期，用于累计跑秒） */
    g_timer1 = TIM_SW_Create(1000, TIM_SW_MODE_PERIODIC, timer1_callback, NULL);  /* 1秒周期 */
    
    /* 检查定时器创建是否成功 */
    if (g_timer1 == TIM_SW_HANDLE_INVALID)
    {
        /* 创建失败，显示错误信息 */
        OLED_ShowString(1, 1, "Timer Create Fail!");
        while (1)
            ;
    }
    
    /* 初始化累计跑秒相关变量 */
    g_total_elapsed_ms = 0;  /* 累计时间由软件定时器回调函数产生 */
    
    /* 启动定时器 */
    TIM_SW_Start(g_timer1);
    
    /* 显示初始信息（快速显示，不阻塞） */
    OLED_ShowString(1, 1, "Timer Demo Start");
    /* 移除阻塞延时，立即进入主循环，确保按键可以立即响应 */
    
    /* 主循环 */
    while (1)
    {
        /* ========== 按键检测（最优先，放在最前面，立即响应） ========== */
        /* 读取按钮引脚电平 */
        button_state = GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
        
        /* 简化的按键检测：边沿检测（检测按下边沿） */
        /* 检测按钮按下（从高电平变为低电平） */
        if (button_state == Bit_RESET && g_button_last_state == Bit_SET)
        {
            /* 检测到按下边沿：立即处理 */
            /* 切换暂停/继续状态 */
            if (g_timers_paused)
            {
                /* 当前是暂停状态，恢复所有定时器 */
                ResumeAllTimers();
            }
            else
            {
                /* 当前是运行状态，暂停所有定时器 */
                PauseAllTimers();
            }
            
            /* LED快速闪烁反馈（立即反馈，不阻塞） */
            LED1_Toggle();
            LED2_Toggle();
        }
        
        /* 保存当前按钮状态，用于下次比较 */
        g_button_last_state = button_state;
        
        /* 更新OLED显示（大幅降低更新频率，减少阻塞时间，提高按键响应速度） */
        /* 注意：如果按键响应仍然慢，可以暂时注释掉OLED更新来测试 */
        static uint32_t oled_update_counter = 0;
        oled_update_counter++;
        if (oled_update_counter >= 500)  /* 每500次循环更新一次OLED（约500ms更新一次，大幅减少阻塞） */
        {
            UpdateOLEDDisplay();
            oled_update_counter = 0;
        }
        
        /* 延时降低CPU占用率（减少延时时间，让按键响应更快） */
        /* 注意：延时太短会导致CPU占用率高，但响应快；延时太长会导致响应慢 */
        Delay_ms(1);  /* 1ms，提高按键检测频率 */
    }
}

