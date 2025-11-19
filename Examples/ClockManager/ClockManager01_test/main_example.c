/**
 * @file main_example.c
 * @brief 案例5 - 时钟管理自动调频测试
 * @details 测试clock_manager模块的自动调频功能
 * @status 已调试完成 ✅
 *
 * 功能说明：
 * - 自动调频策略：
 *   - CPU使用率 < 30%：降频1个系数，最低8MHz，降频间隔5秒
 *   - CPU使用率 > 50%：升频3个系数，升频间隔1秒，最高72MHz
 * - CPU负载模拟：60秒内随机时间创造CPU使用率60-70%，每次运行1-5秒不等
 * - OLED显示：CPU使用率、当前系数、任务程序是否运行
 * - LED1：每500ms闪烁一次（慢速，用于观察延时）
 * - LED2：闪烁速度与频率成正比（系数1闪9次/秒，系数9闪1次/秒）
 *
 * 模块依赖：
 * - base_TIM2：TIM2外设定时器，提供1ms时间基准
 * - delay：阻塞式延时（SysTick）+ 非阻塞式延时（base_TIM2）
 * - clock_manager：时钟管理模块，支持自动调频
 * - OLED：SSD1306软件I2C驱动
 *
 * 硬件要求：
 * - LED1连接到PA1
 * - LED2连接到PA2
 * - OLED显示屏（SSD1306，I2C接口）
 *   - SCL连接到PB8
 *   - SDA连接到PB9
 * - 外部晶振（HSE，8MHz，可选，无则使用HSI）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "clock_manager.h"
#include "base_TIM2.h"
#include "oled_ssd1306.h"
#include "cpu_load_simulator.h"

/**
 * @brief 快速检测HSE是否可用（不等待太长时间）
 * @return 1=HSE可用，0=HSE不可用
 */
static uint8_t CheckHSEAvailable(void)
{
    /* 启动HSE */
    RCC->CR |= RCC_CR_HSEON;

    /* 等待HSE就绪，但使用较短的超时时间（约1ms @72MHz） */
    uint32_t timeout = 72000; /* 约1ms @72MHz，如果HSE可用应该很快就绪 */
    while (!(RCC->CR & RCC_CR_HSERDY) && --timeout)
        ;

    if (timeout == 0)
    {
        /* HSE未就绪，关闭HSE */
        RCC->CR &= ~RCC_CR_HSEON;
        return 0;
    }

    return 1; /* HSE可用 */
}


int main(void)
{
    OLED_Status_t oled_status;
    CLKM_ErrorCode_t clkm_status;
    uint8_t hse_available = 0;

    /* 系统初始化 */
    System_Init();

    /* OLED初始化（在时钟管理之前，避免时钟切换影响） */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，根据错误码显示不同的LED模式 */
        if (oled_status == OLED_ERROR_INVALID_PARAM)
        {
            while (1)
            {
                LED1_On();
                LED2_Off();
                Delay_ms(200);
                LED1_Off();
                LED2_On();
                Delay_ms(200);
            }
        }
        else if (oled_status == OLED_ERROR_GPIO_FAILED)
        {
            while (1)
            {
                LED1_On();
                Delay_ms(50);
                LED1_Off();
                Delay_ms(50);
            }
        }
        else
        {
            while (1)
            {
                LED1_On();
                LED2_On();
                Delay_ms(200);
                LED1_Off();
                LED2_Off();
                Delay_ms(200);
            }
        }
    }

    /* OLED显示初始化信息 */
    OLED_Clear();
    Delay_ms(100);
    OLED_ShowString(1, 1, "Clock Manager");
    OLED_ShowString(2, 1, "Auto Mode");
    Delay_ms(500);

    /* 初始化时钟管理模块 */
    clkm_status = CLKM_Init();
    if (clkm_status != CLKM_OK)
    {
        OLED_ShowString(3, 1, "CLKM Init Fail");
        while (1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }

    OLED_ShowString(3, 1, "CLKM Init OK");
    Delay_ms(500);

    /* 快速检测HSE是否可用 */
    OLED_ShowString(4, 1, "Check HSE...");
    Delay_ms(200);
    hse_available = CheckHSEAvailable();

    if (hse_available)
    {
        /* HSE可用，尝试切换到72MHz */
        OLED_ShowString(4, 1, "HSE OK, Try 72MHz");
        Delay_ms(200);

        clkm_status = CLKM_SetMode(CLKM_MODE_MANUAL, CLKM_LVL_72MHZ);
        if (clkm_status != CLKM_OK)
        {
            /* 72MHz失败，降级到8MHz */
            OLED_ShowString(4, 1, "72MHz Fail, Use 8MHz");
            Delay_ms(500);
            CLKM_SetMode(CLKM_MODE_MANUAL, CLKM_LVL_8MHZ);
        }
        else
        {
            OLED_ShowString(4, 1, "Clock: 72MHz OK");
            Delay_ms(500);
        }
    }
    else
    {
        /* HSE不可用，直接使用8MHz HSI */
        OLED_ShowString(4, 1, "HSE Not Found");
        Delay_ms(500);
        OLED_ShowString(4, 1, "Use 8MHz HSI");
        Delay_ms(200);

        clkm_status = CLKM_SetMode(CLKM_MODE_MANUAL, CLKM_LVL_8MHZ);
        if (clkm_status != CLKM_OK)
        {
            OLED_ShowString(4, 1, "8MHz Fail");
            while (1)
            {
                LED2_On();
                Delay_ms(100);
                LED2_Off();
                Delay_ms(100);
            }
        }

        OLED_ShowString(4, 1, "Clock: 8MHz OK");
        Delay_ms(500);
    }

    /* 设置为自动模式，最低频率8MHz（系数9） */
    OLED_Clear();
    Delay_ms(100);
    OLED_ShowString(1, 1, "Set Auto Mode");
    Delay_ms(200);

    clkm_status = CLKM_SetMode(CLKM_MODE_AUTO, CLKM_LVL_8MHZ);
    if (clkm_status != CLKM_OK)
    {
        OLED_ShowString(2, 1, "Set Auto Fail");
        while (1)
        {
            LED1_On();
            LED2_On();
            Delay_ms(200);
            LED1_Off();
            LED2_Off();
            Delay_ms(200);
        }
    }

    OLED_ShowString(2, 1, "Auto Mode OK");
    Delay_ms(500);

    /* 清屏，准备显示运行信息 */
    OLED_Clear();
    Delay_ms(100);

    /* ========== 主循环：自动调频测试 ========== */

    /* CPU负载模拟变量 */
    static uint32_t last_task_start_tick = 0; /* 上次任务程序开始时间 */
    static uint32_t task_duration = 0;        /* 任务程序运行时长（ms） */
    static uint32_t task_interval = 0;        /* 下次任务程序开始间隔（ms） */
    static uint8_t task_running = 0;          /* 任务程序是否正在运行 */
    static uint32_t cycle_start_tick = 0;     /* 60秒周期开始时间 */

    /* LED控制变量 */
    static uint32_t led1_start = 0; /* LED1开始时间 */
    static uint32_t led2_start = 0; /* LED2开始时间 */
    static uint8_t led1_state = 0;  /* LED1状态 */
    static uint8_t led2_state = 0;  /* LED2状态 */

    /* OLED更新变量 */
    static uint32_t oled_update_timer = 0; /* OLED更新定时器 */

    /* 跑秒器和平均MHz变量 */
    static uint32_t program_start_tick = 0;    /* 程序开始时间（用于跑秒器） */
    static uint32_t last_freq_sample_tick = 0; /* 上次频率采样时间 */
    static uint64_t freq_sum = 0;              /* 频率累加值（用于计算平均值） */
    static uint32_t freq_sample_count = 0;     /* 频率采样次数 */

    /* 初始化变量 */
    program_start_tick = Delay_GetTick();
    cycle_start_tick = Delay_GetTick();
    last_task_start_tick = Delay_GetTick();
    led1_start = Delay_GetTick();
    led2_start = Delay_GetTick();
    oled_update_timer = Delay_GetTick();
    last_freq_sample_tick = Delay_GetTick();

    /* 随机数种子（使用当前tick作为种子） */
    uint32_t rand_seed = Delay_GetTick();

    /* 主循环 */
    while (1)
    {
        uint32_t current_tick = Delay_GetTick();

        /* ========== CPU负载模拟：60秒内随机时间创造60-70%负载 ========== */

        /* 计算60秒周期内的位置 */
        uint32_t cycle_elapsed = Delay_GetElapsed(current_tick, cycle_start_tick);

        if (cycle_elapsed >= 60000)
        {
            /* 60秒周期结束，重新开始 */
            cycle_start_tick = current_tick;
            cycle_elapsed = 0;
            task_running = 0;
            task_duration = 0;
            task_interval = 0;
        }

        /* 检查是否到了任务程序开始时间 */
        if (!task_running)
        {
            if (task_interval == 0)
            {
                /* 生成随机间隔（0-60秒，但确保在60秒周期内） */
                rand_seed = rand_seed * 1103515245 + 12345; /* 简单线性同余生成器 */
                task_interval = (rand_seed % 60000);        /* 0-59999ms */

                /* 生成随机运行时长（1-5秒） */
                rand_seed = rand_seed * 1103515245 + 12345;
                task_duration = 1000 + (rand_seed % 4000); /* 1000-4999ms */

                last_task_start_tick = current_tick;
            }

            /* 检查是否到了开始时间 */
            uint32_t wait_elapsed = Delay_GetElapsed(current_tick, last_task_start_tick);

            if (wait_elapsed >= task_interval)
            {
                /* 开始任务程序 */
                task_running = 1;
                last_task_start_tick = current_tick;
                task_interval = 0; /* 重置间隔，等待下次生成 */
            }
        }

        /* 如果任务程序正在运行 */
        if (task_running)
        {
            /* 检查是否运行完成 */
            uint32_t task_elapsed = Delay_GetElapsed(current_tick, last_task_start_tick);

            if (task_elapsed >= task_duration)
            {
                /* 任务程序运行完成 */
                task_running = 0;
                task_duration = 0;
                task_interval = 0;
            }
            else
            {
                /* 模拟高负载（60-70%使用率） */
                CPU_SimulateHighLoad50ms(); /* 使用公共函数，每次模拟50ms */
            }
        }
        else
        {
            /* 低负载：调用空闲钩子 */
            CLKM_IdleHook();
        }

        /* ========== 自动调频 ========== */
        CLKM_AdaptiveTask();

        /* ========== 频率采样（每秒采样一次） ========== */
        if (Delay_ms_nonblock(last_freq_sample_tick, 1000))
        {
            uint32_t current_freq = CLKM_GetCurrentFrequency();
            freq_sum += current_freq;
            freq_sample_count++;
            last_freq_sample_tick = current_tick;
        }

        /* ========== OLED显示更新（每500ms更新一次） ========== */
        if (Delay_ms_nonblock(oled_update_timer, 500))
        {
            /* 获取当前状态 */
            uint32_t current_freq = CLKM_GetCurrentFrequency();
            CLKM_FreqLevel_t current_level = CLKM_GetCurrentLevel();
            uint8_t coefficient = current_level + 1; /* level 0-8 对应系数 1-9 */
            uint8_t cpu_load = CLKM_GetCPULoad();

            /* 第1行：CPU使用率 */
            OLED_ShowString(1, 1, "CPU:");
            OLED_ShowNum(1, 5, cpu_load, 3);
            OLED_ShowString(1, 8, "%");

            /* 第2行：当前系数 */
            OLED_ShowString(2, 1, "Coeff:");
            OLED_ShowNum(2, 7, coefficient, 1);
            OLED_ShowString(2, 8, " Freq:");
            OLED_ShowNum(2, 14, current_freq / 1000000, 2);
            OLED_ShowString(2, 16, "MHz");

            /* 第3行：任务程序状态 */
            OLED_ShowString(3, 1, "Task:");
            if (task_running)
            {
                OLED_ShowString(3, 6, "Running ");
            }
            else
            {
                OLED_ShowString(3, 6, "Idle    ");
            }

            /* 第4行：跑秒器和平均MHz */
            /* 计算运行时间（从程序开始累计） */
            uint32_t elapsed_ms = Delay_GetElapsed(current_tick, program_start_tick);
            uint32_t elapsed_sec = elapsed_ms / 1000;
            uint32_t minutes = elapsed_sec / 60;
            uint32_t seconds = elapsed_sec % 60;

            /* 计算平均MHz */
            uint32_t avg_freq_mhz = 0;
            if (freq_sample_count > 0)
            {
                avg_freq_mhz = (uint32_t)(freq_sum / freq_sample_count / 1000000);
            }

            /* 显示：T: MM:SS (XXMHz) */
            OLED_ShowString(4, 1, "T:");
            OLED_ShowNum(4, 3, minutes, 2);
            OLED_ShowString(4, 5, ":");
            OLED_ShowNum(4, 6, seconds, 2);
            OLED_ShowString(4, 8, " (");
            OLED_ShowNum(4, 10, avg_freq_mhz, 2);
            OLED_ShowString(4, 12, "MHz)");

            oled_update_timer = current_tick;
        }

        /* ========== LED1控制：每500ms闪烁一次（慢速） ========== */
        if (Delay_ms_nonblock(led1_start, 500))
        {
            if (led1_state == 0)
            {
                LED1_On();
                led1_state = 1;
            }
            else
            {
                LED1_Off();
                led1_state = 0;
            }
            led1_start = current_tick;
        }

        /* ========== LED2控制：闪烁速度与频率成正比 ========== */
        /* 获取当前频率系数 */
        CLKM_FreqLevel_t current_level = CLKM_GetCurrentLevel();
        uint8_t coefficient = current_level + 1; /* level 0-8 对应系数 1-9 */

        /* 计算LED2每秒闪烁次数：频率高闪得快，频率低闪得慢 */
        /* 系数1（72MHz）→ 每秒闪9次，系数9（8MHz）→ 每秒闪1次 */
        uint8_t led2_blinks_per_sec = 10 - coefficient;
        uint16_t led2_interval_ms = 1000 / led2_blinks_per_sec;

        if (Delay_ms_nonblock(led2_start, led2_interval_ms))
        {
            if (led2_state == 0)
            {
                LED2_On();
                led2_state = 1;
            }
            else
            {
                LED2_Off();
                led2_state = 0;
            }
            led2_start = current_tick;
        }

        /* 短暂延时，避免CPU占用过高 */
        Delay_us(100);
    }
}
