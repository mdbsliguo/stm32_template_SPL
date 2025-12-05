/**
 * @file main_example.c
 * @brief 案例 - PWM频率、占空比、分辨率三参数演示
 * @example Examples/PWM/PWM02_TB6612_ThreeDevicesDemo/main_example.c
 * @details 通过三设备联动（马达+LED+无源蜂鸣器）演示PWM的频率、占空比、分辨率三个核心参数的影响
 *
 * 硬件要求：
 * - 无源蜂鸣器：连接PA6（TIM3 CH1）
 * - LED1：PA1（可通过PWM控制亮度）
 * - LED2：PA2（可通过PWM控制亮度）
 * - 马达：通过TB6612驱动，PWMA连接PA7（TIM3 CH2），AIN1=PB3，AIN2=PB4，STBY=PB5
 *
 * 硬件配置：
 * 在 Examples/PWM/PWM02_TB6612_ThreeDevicesDemo/board.h 中配置
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/PWM/PWM02_TB6612_ThreeDevicesDemo/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/PWM/PWM02_TB6612_ThreeDevicesDemo/board.h 中的配置
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
#include "led.h"
#include "buzzer.h"
#include "timer_pwm.h"
#include "tb6612.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "printf_wrapper.h"
#include <stdint.h>
#include <string.h>

/* ==================== 演示阶段定义 ==================== */

/**
 * @brief 阶段0：初始化基准
 * @param freq 频率（Hz）
 * @param duty 占空比（%）
 * @param resolution 分辨率（PWM_RESOLUTION_8BIT或PWM_RESOLUTION_16BIT）
 */
/**
 * @brief 设置LED1目标占空比（用于统一接口）
 * @param duty 占空比（0.0 ~ 100.0）
 * @note 这个函数目前只用于统一接口，实际占空比在ControlLED1ByDuty中使用
 * @note 当前实现为空函数，保留接口以便将来扩展（如主循环中持续更新LED状态）
 */
static void SetLED1Duty(float duty)
{
    /* 当前实现为空，保留接口以便将来扩展 */
    /* 参数校验，确保占空比在有效范围内 */
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 100.0f) duty = 100.0f;
    /* 暂时未使用duty，但保留参数校验以便将来扩展 */
    (void)duty;
}

/**
 * @brief 根据占空比控制LED1亮度（GPIO模拟PWM效果）
 * @param duty 占空比（0.0 ~ 100.0）
 * @note 这个函数执行一个周期的LED闪烁，用于在延时期间持续调用
 */
static void ControlLED1ByDuty(float duty)
{
    /* 参数校验 */
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 100.0f) duty = 100.0f;
    
    if (duty <= 0.0f) {
        LED1_Off();
    } else if (duty >= 100.0f) {
        LED1_On();
    } else {
        /* 简单的PWM模拟：快速闪烁实现亮度控制 */
        /* 注意：这不是真正的PWM，只是演示效果 */
        /* 计算一个周期的时间（例如50ms），然后根据占空比分配开启和关闭时间 */
        uint32_t period_ms = 50;  /* 一个周期50ms，闪烁频率约20Hz，人眼可见但不会太闪烁 */
        uint32_t on_time = (uint32_t)(period_ms * duty / 100.0f);
        uint32_t off_time = period_ms - on_time;
        
        /* 确保最小开启时间至少1ms，否则LED看起来会完全熄灭 */
        if (on_time < 1 && duty > 0.0f) {
            on_time = 1;
            off_time = period_ms - on_time;
        }
        
        /* 执行一个周期的闪烁 */
        LED1_On();
        if (on_time > 0) {
            Delay_ms(on_time);
        }
        LED1_Off();
        if (off_time > 0) {
            Delay_ms(off_time);
        }
    }
}

/**
 * @brief 带LED闪烁的延时函数
 * @param ms 延时时间（毫秒）
 * @param duty LED占空比（0.0 ~ 100.0）
 * @note 在延时期间持续闪烁LED，模拟PWM效果
 */
static void Delay_ms_WithLED(uint32_t ms, float duty)
{
    if (duty <= 0.0f) {
        LED1_Off();
        Delay_ms(ms);
    } else if (duty >= 100.0f) {
        LED1_On();
        Delay_ms(ms);
    } else {
        /* 将长延时分解为多个短周期，在每个周期内闪烁LED */
        uint32_t period_ms = 50;  /* 一个周期50ms */
        uint32_t cycles = ms / period_ms;  /* 计算需要多少个周期 */
        uint32_t remainder = ms % period_ms;  /* 剩余时间 */
        
        for (uint32_t i = 0; i < cycles; i++) {
            ControlLED1ByDuty(duty);
        }
        
        /* 处理剩余时间 */
        if (remainder > 0) {
            Delay_ms(remainder);
        }
    }
}

static void Demo_Stage0_Init(uint32_t freq, float duty, PWM_Resolution_t resolution)
{
    PWM_Status_t pwm_status;
    TB6612_Status_t tb6612_status;
    
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "=== 阶段0：初始化基准 ===");
    LOG_INFO("MAIN", "参数：频率=%dHz，占空比=%.1f%%，分辨率=%s", 
             freq, duty, (resolution == PWM_RESOLUTION_8BIT) ? "8位" : "16位");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "【PWM基础概念】");
    LOG_INFO("MAIN", "  - 频率：PWM波形每秒的周期数（Hz），决定开关速度");
    LOG_INFO("MAIN", "  - 占空比：高电平时间占周期的百分比（%%），决定平均功率");
    LOG_INFO("MAIN", "  - 分辨率：占空比可调节的精度等级，8位=256级，16位=65536级");
    LOG_INFO("MAIN", "");
    
    /* 初始化PWM（TIM3） */
    pwm_status = PWM_Init(PWM_INSTANCE_TIM3);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM初始化失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
        return;
    }
    
    /* 设置分辨率 */
    pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, resolution);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置分辨率失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
        return;
    }
    
    /* 设置频率 */
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, freq);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置频率失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
        return;
    }
    
    /* 蜂鸣器：设置占空比并使能 */
    pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, duty);
    if (pwm_status == PWM_OK) {
        pwm_status = PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);
    }
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "蜂鸣器PWM配置失败: %d", pwm_status);
    }
    
    /* LED1：根据占空比控制亮度（使用GPIO模拟PWM效果） */
    SetLED1Duty(duty);
    /* 注意：LED闪烁在Delay_ms_WithLED中进行，这里只设置占空比 */
    
    /* 马达：设置方向和速度 */
    /* 注意：TB6612已经在main函数中初始化并使能，这里只需要设置方向和速度 */
    tb6612_status = TB6612_SetDirection(TB6612_INSTANCE_1, TB6612_DIR_FORWARD);
    if (tb6612_status != TB6612_OK) {
        LOG_ERROR("MAIN", "马达设置方向失败: %d", tb6612_status);
        ErrorHandler_Handle(tb6612_status, "TB6612");
    } else {
        LOG_INFO("MAIN", "马达方向已设置为正转（AIN1=高，AIN2=低）");
    }
    
    tb6612_status = TB6612_SetSpeed(TB6612_INSTANCE_1, duty);
    if (tb6612_status != TB6612_OK) {
        LOG_ERROR("MAIN", "马达设置速度失败: %d", tb6612_status);
        ErrorHandler_Handle(tb6612_status, "TB6612");
    } else {
        LOG_INFO("MAIN", "马达速度已设置为%.1f%%", duty);
    }
    
    LOG_INFO("MAIN", "基准状态：马达平稳转动，LED中等亮度，蜂鸣器标准音调");
    LOG_INFO("MAIN", "========================================");
    Delay_ms_WithLED(10000, duty);  /* 停留10秒，期间LED持续闪烁，便于理解原理 */
}

/**
 * @brief 阶段1：频率变化演示
 */
static void Demo_Stage1_Frequency(void)
{
    uint32_t frequencies[] = {20, 100, 1000, 10000};
    const char* freq_names[] = {"20Hz", "100Hz", "1kHz", "10kHz"};
    PWM_Status_t pwm_status;
    uint32_t current_freq;
    PWM_Resolution_t current_res;
    
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "=== 阶段1：频率变化演示 ===");
    LOG_INFO("MAIN", "固定参数：占空比=50%%，分辨率=16位");
    LOG_INFO("MAIN", "观察重点：主要观察马达（抖动/平稳）、无源蜂鸣器（音调变化）；次要观察LED（闪烁）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "【频率影响原理】");
    LOG_INFO("MAIN", "  - 频率决定PWM波形的周期时间：T=1/f（秒）");
    LOG_INFO("MAIN", "  - 低频率（<100Hz）：周期长，设备能感知到开关变化");
    LOG_INFO("MAIN", "  - 中频率（100Hz-1kHz）：周期适中，部分设备仍有感知");
    LOG_INFO("MAIN", "  - 高频率（>1kHz）：周期短，人眼/人耳无法感知，效果平滑");
    LOG_INFO("MAIN", "");
    
    /* 获取当前分辨率 */
    PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
    
    for (int i = 0; i < 4; i++) {
        LOG_INFO("MAIN", "----------------------------------------");
        LOG_INFO("MAIN", "步骤1-%d：频率=%s", i+1, freq_names[i]);
        
        /* 设置频率 */
        pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, frequencies[i]);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "设置频率失败: %d", pwm_status);
            continue;
        }
        
        /* 验证频率 */
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        LOG_INFO("MAIN", "当前频率: %dHz", current_freq);
        
        /* 频率改变后，需要重新设置蜂鸣器和马达的占空比，确保PWM输出正确 */
        /* 蜂鸣器：设置占空比（固定50%）并启用通道 */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 50.0f);
        if (pwm_status == PWM_OK) {
            PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
        }
        
        /* 马达：设置速度（固定50%） */
        (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 50.0f);
        
        /* LED1：根据占空比（固定50%）控制亮度 */
        SetLED1Duty(50.0f);
        
        /* 描述预期效果和原理 */
        switch (i) {
            case 0:  /* 20Hz */
                LOG_INFO("MAIN", "【20Hz原理】周期=50ms，设备能清晰感知开关变化");
                LOG_INFO("MAIN", "  - 马达：每50ms开关一次，产生剧烈抖动（启动-停止循环）");
                LOG_INFO("MAIN", "  - LED：每50ms闪烁一次，人眼明显可见（闪烁频率=20Hz）");
                LOG_INFO("MAIN", "  - 蜂鸣器：每50ms振动一次，产生低沉\"咔咔\"声（音调=20Hz）");
                /* 低频率时，LED闪烁明显，使用较慢的闪烁周期模拟 */
                Delay_ms_WithLED(10000, 50.0f);  /* 每步停留10秒，期间LED持续闪烁 */
                break;
            case 1:  /* 100Hz */
                LOG_INFO("MAIN", "【100Hz原理】周期=10ms，设备仍有感知但较平滑");
                LOG_INFO("MAIN", "  - 马达：每10ms开关一次，转动但震动明显（启动-停止循环）");
                LOG_INFO("MAIN", "  - LED：每10ms闪烁一次，人眼轻微可见（闪烁频率=100Hz）");
                LOG_INFO("MAIN", "  - 蜂鸣器：每10ms振动一次，产生清晰低音调（音调=100Hz）");
                /* 中频率时，LED闪烁较明显，使用中等闪烁周期 */
                Delay_ms_WithLED(10000, 50.0f);  /* 每步停留10秒，期间LED持续闪烁 */
                break;
            case 2:  /* 1kHz */
                LOG_INFO("MAIN", "【1kHz原理】周期=1ms，人眼/人耳无法感知，效果平滑");
                LOG_INFO("MAIN", "  - 马达：每1ms开关一次，人眼无法感知，转动平稳");
                LOG_INFO("MAIN", "  - LED：每1ms闪烁一次，人眼无法感知，看起来恒亮");
                LOG_INFO("MAIN", "  - 蜂鸣器：每1ms振动一次，产生标准中音调（音调=1kHz）");
                /* 高频率时，LED看起来恒亮，停止闪烁，直接点亮 */
                LED1_On();
                Delay_ms(10000);  /* 每步停留10秒，LED恒亮 */
                break;
            case 3:  /* 10kHz */
                LOG_INFO("MAIN", "【10kHz原理】周期=0.1ms，开关极快，效果最平滑");
                LOG_INFO("MAIN", "  - 马达：每0.1ms开关一次，转动最平稳，无震动感");
                LOG_INFO("MAIN", "  - LED：每0.1ms闪烁一次，完全恒亮，无闪烁感");
                LOG_INFO("MAIN", "  - 蜂鸣器：每0.1ms振动一次，产生尖锐高音调（音调=10kHz）");
                /* 极高频率时，LED完全恒亮，停止闪烁 */
                LED1_On();
                Delay_ms(10000);  /* 每步停留10秒，LED恒亮 */
                break;
        }
    }
    LOG_INFO("MAIN", "========================================");
}

/**
 * @brief 阶段2：占空比变化演示
 */
static void Demo_Stage2_DutyCycle(void)
{
    float duties[] = {0.0f, 10.0f, 50.0f, 90.0f, 99.0f};  /* 注意：100%改为99%，因为100%占空比时无源蜂鸣器不振动 */
    PWM_Status_t pwm_status;
    TB6612_Status_t tb6612_status;
    
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "=== 阶段2：占空比变化演示 ===");
    LOG_INFO("MAIN", "固定参数：频率=1kHz，分辨率=16位");
    LOG_INFO("MAIN", "观察重点：主要观察LED（亮度变化）；次要观察马达（转速）、无源蜂鸣器（音量）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "【占空比影响原理】");
    LOG_INFO("MAIN", "  - 占空比决定平均功率：P_avg = P_max × 占空比");
    LOG_INFO("MAIN", "  - 占空比=0%%：完全关闭，无输出");
    LOG_INFO("MAIN", "  - 占空比=50%%：一半时间开启，平均功率=50%%");
    LOG_INFO("MAIN", "  - 占空比=99%%：接近最大功率（注意：100%%占空比时无源蜂鸣器不振动）");
    LOG_INFO("MAIN", "");
    
    /* 设置频率为1kHz */
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 1000);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "设置频率失败: %d", pwm_status);
        return;
    }
    
    for (int i = 0; i < 5; i++) {
        LOG_INFO("MAIN", "----------------------------------------");
        LOG_INFO("MAIN", "步骤2-%d：占空比=%.1f%%", i+1, duties[i]);
        
        /* 蜂鸣器：设置占空比 */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, duties[i]);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "蜂鸣器设置占空比失败: %d", pwm_status);
        } else {
            /* 根据占空比启用或禁用PWM通道 */
            if (duties[i] > 0.0f) {
                /* 占空比>0%时，启用PWM通道 */
                pwm_status = PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);
                if (pwm_status != PWM_OK) {
                    LOG_ERROR("MAIN", "蜂鸣器启用PWM通道失败: %d", pwm_status);
                }
            } else {
                /* 占空比=0%时，禁用PWM通道 */
                PWM_DisableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);
            }
        }
        
        /* 马达：设置速度 */
        tb6612_status = TB6612_SetSpeed(TB6612_INSTANCE_1, duties[i]);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "马达设置速度失败: %d", tb6612_status);
        }
        
        /* LED1：根据占空比控制亮度（使用GPIO模拟PWM效果） */
        SetLED1Duty(duties[i]);
        
        /* 描述预期效果和原理 */
        switch (i) {
            case 0:  /* 0% */
                LOG_INFO("MAIN", "【0%%占空比原理】平均功率=0%%，完全关闭");
                LOG_INFO("MAIN", "  - 马达：无功率输入，完全停止");
                LOG_INFO("MAIN", "  - LED：无电流通过，熄灭");
                LOG_INFO("MAIN", "  - 蜂鸣器：无振动，静音");
                break;
            case 1:  /* 10% */
                LOG_INFO("MAIN", "【10%%占空比原理】平均功率=10%%，输出很小");
                LOG_INFO("MAIN", "  - 马达：功率不足，可能无法启动（启动扭矩要求）");
                LOG_INFO("MAIN", "  - LED：平均电流=10%%，极暗");
                LOG_INFO("MAIN", "  - 蜂鸣器：振动幅度=10%%，微弱音量");
                break;
            case 2:  /* 50% */
                LOG_INFO("MAIN", "【50%%占空比原理】平均功率=50%%，标准输出");
                LOG_INFO("MAIN", "  - 马达：功率充足，正常转速（转速≈50%%最大转速）");
                LOG_INFO("MAIN", "  - LED：平均电流=50%%，中等亮度");
                LOG_INFO("MAIN", "  - 蜂鸣器：振动幅度=50%%，标准音量");
                break;
            case 3:  /* 90% */
                LOG_INFO("MAIN", "【90%%占空比原理】平均功率=90%%，接近最大输出");
                LOG_INFO("MAIN", "  - 马达：功率很大，接近全速（转速≈90%%最大转速）");
                LOG_INFO("MAIN", "  - LED：平均电流=90%%，很亮");
                LOG_INFO("MAIN", "  - 蜂鸣器：振动幅度=90%%，音量几乎最大");
                break;
            case 4:  /* 99% */
                LOG_INFO("MAIN", "【99%%占空比原理】平均功率=99%%，接近最大输出");
                LOG_INFO("MAIN", "  - 马达：接近最大功率，接近全速（转速≈99%%最大转速）");
                LOG_INFO("MAIN", "  - LED：接近最大电流，几乎最亮");
                LOG_INFO("MAIN", "  - 蜂鸣器：接近最大振动幅度，接近最大音量");
                LOG_INFO("MAIN", "  - 注意：100%%占空比时PWM信号恒为高电平，无电平变化，无源蜂鸣器无法振动");
                break;
        }
        
        Delay_ms_WithLED(10000, duties[i]);  /* 每步停留10秒，期间LED持续闪烁，便于理解原理 */
    }
    LOG_INFO("MAIN", "========================================");
}

/**
 * @brief 阶段3：分辨率变化演示
 */
static void Demo_Stage3_Resolution(void)
{
    PWM_Resolution_t resolutions[] = {PWM_RESOLUTION_8BIT, PWM_RESOLUTION_16BIT};
    const char* res_names[] = {"8位", "16位"};
    PWM_Status_t pwm_status;
    float test_duty = 5.0f;  /* 低占空比区域测试 */
    
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "=== 阶段3：分辨率变化演示 ===");
    LOG_INFO("MAIN", "固定参数：频率=500Hz，占空比=%.1f%%（低占空比区域）", test_duty);
    LOG_INFO("MAIN", "观察重点：主要观察无源蜂鸣器（音质差异）、马达（平滑度差异）；次要观察LED（渐变）");
    LOG_INFO("MAIN", "");
    LOG_INFO("MAIN", "【分辨率影响原理】");
    LOG_INFO("MAIN", "  - 分辨率决定占空比的调节精度：8位=256级，16位=65536级");
    LOG_INFO("MAIN", "  - 8位分辨率：占空比最小步进=100%%/256≈0.39%%，精度较低");
    LOG_INFO("MAIN", "  - 16位分辨率：占空比最小步进=100%%/65536≈0.0015%%，精度很高");
    LOG_INFO("MAIN", "  - 在低占空比区域（如5%%），分辨率差异最明显");
    LOG_INFO("MAIN", "");
    
    /* 设置频率为500Hz */
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 500);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "设置频率失败: %d", pwm_status);
        return;
    }
    
    for (int i = 0; i < 2; i++) {
        LOG_INFO("MAIN", "----------------------------------------");
        LOG_INFO("MAIN", "步骤3-%d：分辨率=%s", i+1, res_names[i]);
        
        /* 设置分辨率 */
        pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, resolutions[i]);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "设置分辨率失败: %d", pwm_status);
            continue;
        }
        
        /* 设置占空比 */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, test_duty);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "设置占空比失败: %d", pwm_status);
        } else {
            /* 分辨率改变后，需要重新启用蜂鸣器通道 */
            if (test_duty > 0.0f) {
                PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
            } else {
                PWM_DisableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 占空比为0时禁用 */
            }
        }
        
        /* 马达：设置速度 */
        TB6612_SetSpeed(TB6612_INSTANCE_1, test_duty);
        
        /* LED1：根据占空比（固定5%）控制亮度 */
        SetLED1Duty(test_duty);
        
        /* 描述预期效果和原理 */
        if (i == 0) {  /* 8位 */
            LOG_INFO("MAIN", "【8位分辨率原理】占空比最小步进≈0.39%%，精度较低");
            LOG_INFO("MAIN", "  - 马达：在5%%占空比时，可调级数=5/0.39≈13级，转速有明显跳变");
            LOG_INFO("MAIN", "  - LED：亮度调节只有13级，肉眼可见亮度阶梯");
            LOG_INFO("MAIN", "  - 蜂鸣器：音调调节只有13级，有\"跳音\"感（不连续）");
        } else {  /* 16位 */
            LOG_INFO("MAIN", "【16位分辨率原理】占空比最小步进≈0.0015%%，精度很高");
            LOG_INFO("MAIN", "  - 马达：在5%%占空比时，可调级数=5/0.0015≈3333级，转速平滑");
            LOG_INFO("MAIN", "  - LED：亮度调节有3333级，平滑渐变（人眼无法感知阶梯）");
            LOG_INFO("MAIN", "  - 蜂鸣器：音调调节有3333级，连续变化（无跳音感）");
        }
        
        Delay_ms_WithLED(10000, test_duty);  /* 每步停留10秒，期间LED持续闪烁，便于理解原理 */
    }
    
    /* 测试低频率+低分辨率组合 */
    LOG_INFO("MAIN", "----------------------------------------");
    LOG_INFO("MAIN", "步骤3-3：低频率+低分辨率组合（50Hz，8位）");
    LOG_INFO("MAIN", "【组合效果原理】低频率（周期长）+低分辨率（精度低）=双重恶化");
        pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 50);
        if (pwm_status == PWM_OK) {
            pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_8BIT);
            if (pwm_status == PWM_OK) {
                pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, test_duty);
                if (pwm_status == PWM_OK) {
                    PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
                }
                (void)TB6612_SetSpeed(TB6612_INSTANCE_1, test_duty);
                SetLED1Duty(test_duty);
            LOG_INFO("MAIN", "  - 马达：频率低导致抖动 + 分辨率低导致跳变 = 双重恶化");
            LOG_INFO("MAIN", "  - LED：频率低导致闪烁 + 分辨率低导致阶梯 = 闪烁+阶梯");
            LOG_INFO("MAIN", "  - 蜂鸣器：频率低导致断续 + 分辨率低导致跳音 = 沙哑断续音");
            Delay_ms_WithLED(10000, test_duty);  /* 停留10秒，期间LED持续闪烁，便于理解原理 */
        }
    }
    
    /* 测试低频率+高分辨率组合 */
    LOG_INFO("MAIN", "----------------------------------------");
    LOG_INFO("MAIN", "步骤3-4：低频率+高分辨率组合（50Hz，16位）");
    LOG_INFO("MAIN", "【组合效果原理】低频率（周期长）+高分辨率（精度高）=抖动但平滑");
        pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 50);
        if (pwm_status == PWM_OK) {
            pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_16BIT);
            if (pwm_status == PWM_OK) {
                pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, test_duty);
                if (pwm_status == PWM_OK) {
                    PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
                }
                (void)TB6612_SetSpeed(TB6612_INSTANCE_1, test_duty);
                SetLED1Duty(test_duty);
            LOG_INFO("MAIN", "  - 马达：频率低导致抖动，但分辨率高使转速平滑（无跳变）");
            LOG_INFO("MAIN", "  - LED：频率低导致闪烁，但分辨率高使亮度连续（无阶梯）");
            LOG_INFO("MAIN", "  - 蜂鸣器：频率低导致断续，但分辨率高使音调连续（无跳音）");
            Delay_ms_WithLED(10000, test_duty);  /* 停留10秒，期间LED持续闪烁，便于理解原理 */
        }
    }
    LOG_INFO("MAIN", "========================================");
}

/**
 * @brief 阶段4：参数联动综合演示
 */
static void Demo_Stage4_Comprehensive(void)
{
    PWM_Status_t pwm_status;
    
    LOG_INFO("MAIN", "========================================");
    LOG_INFO("MAIN", "=== 阶段4：参数联动综合演示 ===");
    LOG_INFO("MAIN", "");
    
    /* 场景A：启动过程模拟 */
    LOG_INFO("MAIN", "----------------------------------------");
    LOG_INFO("MAIN", "场景A：启动过程模拟");
    LOG_INFO("MAIN", "初始：频率=20Hz，占空比=0%%，分辨率=8位");
    
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 20);
    if (pwm_status == PWM_OK) {
        pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_8BIT);
        if (pwm_status == PWM_OK) {
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 0.0f);
            if (pwm_status == PWM_OK) {
                PWM_DisableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 占空比为0时禁用蜂鸣器通道 */
            }
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 0.0f);
            LED1_Off();  /* 占空比0%，LED熄灭 */
            Delay_ms(8000);  /* 停留8秒 */
            
            /* 占空比→30% */
            LOG_INFO("MAIN", "占空比→30%%（马达卡死抖动）");
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 30.0f);
            if (pwm_status == PWM_OK) {
                PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
            }
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 30.0f);
            SetLED1Duty(30.0f);
            Delay_ms_WithLED(8000, 30.0f);  /* 停留8秒，期间LED持续闪烁，便于理解原理 */
            
            /* 频率提升至200Hz */
            LOG_INFO("MAIN", "频率提升至200Hz（马达启动）");
            pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 200);
            /* 频率提升后，需要重新设置马达和蜂鸣器的占空比，确保PWM输出正确 */
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 30.0f);  /* 重新设置马达速度 */
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 30.0f);  /* 重新设置蜂鸣器占空比 */
            if (pwm_status == PWM_OK) {
                PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
            }
            /* 频率提升后，LED闪烁变快，但占空比不变，继续闪烁 */
            Delay_ms_WithLED(8000, 30.0f);  /* 停留8秒，期间LED持续闪烁，便于理解原理 */
            
            /* 分辨率切16位 */
            LOG_INFO("MAIN", "分辨率切16位（抖动消失）");
            pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_16BIT);
            /* 分辨率提升后，需要重新设置马达和蜂鸣器的占空比，确保PWM输出正确 */
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 30.0f);  /* 重新设置马达速度 */
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 30.0f);  /* 重新设置蜂鸣器占空比 */
            if (pwm_status == PWM_OK) {
                PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
            }
            /* 分辨率提升，LED亮度更平滑，但占空比不变，继续闪烁 */
            Delay_ms_WithLED(8000, 30.0f);  /* 停留8秒，期间LED持续闪烁，便于理解原理 */
        }
    }
    
    /* 场景B：精密调速对比 */
    LOG_INFO("MAIN", "----------------------------------------");
    LOG_INFO("MAIN", "场景B：精密调速对比");
    LOG_INFO("MAIN", "固定：频率=1kHz，占空比=1%%");
    LOG_INFO("MAIN", "注意：此场景主要演示马达和蜂鸣器，LED在1kHz下几乎不亮（占空比1%%）");
    
        pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 1000);
        if (pwm_status == PWM_OK) {
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 1.0f);
            if (pwm_status == PWM_OK) {
                PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
            }
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 1.0f);
            LED1_Off();  /* 1kHz + 1%占空比，LED几乎不亮，直接熄灭 */
        
        /* 8位分辨率 */
        LOG_INFO("MAIN", "8位分辨率：马达\"抽搐式转动\"，蜂鸣器\"沙沙噪音\"");
        pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_8BIT);
        /* 分辨率改变后，需要重新设置马达和蜂鸣器的占空比，确保PWM输出正确 */
        (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 1.0f);  /* 重新设置马达速度 */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 1.0f);  /* 重新设置蜂鸣器占空比 */
        if (pwm_status == PWM_OK) {
            PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
        }
        Delay_ms(8000);  /* 停留8秒，LED熄灭，主要观察马达和蜂鸣器 */
        
        /* 16位分辨率 */
        LOG_INFO("MAIN", "16位分辨率：马达\"连续微转\"，蜂鸣器\"纯净低音\"");
        pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_16BIT);
        /* 分辨率改变后，需要重新设置马达和蜂鸣器的占空比，确保PWM输出正确 */
        (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 1.0f);  /* 重新设置马达速度 */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 1.0f);  /* 重新设置蜂鸣器占空比 */
        if (pwm_status == PWM_OK) {
            PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
        }
        Delay_ms(8000);  /* 停留8秒，LED熄灭，主要观察马达和蜂鸣器 */
    }
    
    /* 场景C：极限挑战 */
    LOG_INFO("MAIN", "----------------------------------------");
    LOG_INFO("MAIN", "场景C：极限挑战");
    LOG_INFO("MAIN", "参数：频率=10kHz，占空比=1%%，分辨率=16位");
    
        pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, 10000);
        if (pwm_status == PWM_OK) {
            pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_16BIT);
            if (pwm_status == PWM_OK) {
                pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 1.0f);
                if (pwm_status == PWM_OK) {
                    PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 确保蜂鸣器通道启用 */
                }
                (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 1.0f);
                LED1_Off();  /* 10kHz + 1%占空比，LED几乎不亮，直接熄灭 */
            LOG_INFO("MAIN", "预期：马达微转但极平稳，LED几乎不亮，蜂鸣器发出微弱纯音");
            Delay_ms(8000);  /* 停留8秒，LED熄灭，主要观察马达和蜂鸣器 */
            
            /* 切到8位分辨率 */
            LOG_INFO("MAIN", "切到8位分辨率：马达停转，LED熄灭，蜂鸣器静音");
            pwm_status = PWM_SetResolution(PWM_INSTANCE_TIM3, PWM_RESOLUTION_8BIT);
            /* 分辨率改变后，需要重新设置马达和蜂鸣器的占空比，确保PWM输出正确 */
            /* 8位分辨率下，1%占空比可能无法正确表示，设置为0% */
            (void)TB6612_SetSpeed(TB6612_INSTANCE_1, 0.0f);  /* 马达停转 */
            pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, 0.0f);  /* 蜂鸣器静音 */
            if (pwm_status == PWM_OK) {
                PWM_DisableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);  /* 禁用蜂鸣器通道 */
            }
            LED1_Off();  /* 8位分辨率下，1%占空比可能无法正确表示，LED熄灭 */
            Delay_ms(8000);  /* 停留8秒，LED熄灭，主要观察马达和蜂鸣器 */
        }
    }
    LOG_INFO("MAIN", "========================================");
}

/**
 * @brief 更新OLED显示
 * @param stage 阶段编号
 * @param freq 频率
 * @param duty 占空比
 * @param resolution 分辨率
 */
static void UpdateOLEDDisplay(uint8_t stage, uint32_t freq, float duty, PWM_Resolution_t resolution)
{
    /* 第1行：阶段信息 */
    Printf_OLED1("Stage %d Demo", stage);
    
    /* 第2行：频率 */
    Printf_OLED2("Freq:%dHz", freq);
    
    /* 第3行：占空比 */
    Printf_OLED3("Duty:%.1f%%", duty);
    
    /* 第4行：分辨率 */
    Printf_OLED4("Res:%s", (resolution == PWM_RESOLUTION_8BIT) ? "8bit" : "16bit");
}

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    TB6612_Status_t tb6612_status;
    Buzzer_Status_t buzzer_status;
    LED_Status_t led_status;
    PWM_Resolution_t current_res;
    uint32_t current_freq;
    
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
    log_config.level = LOG_LEVEL_INFO;         /* 日志级别：INFO（简化输出） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳 */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出 */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "=== PWM频率、占空比、分辨率三参数演示 ===");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
    /* LED初始化 */
    led_status = LED_Init();
    if (led_status != LED_OK) {
        LOG_ERROR("MAIN", "LED初始化失败: %d", led_status);
        ErrorHandler_Handle(led_status, "LED");
    } else {
        LOG_INFO("MAIN", "LED已初始化");
    }
    
    /* 软件I2C初始化（OLED需要） */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
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
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "PWM Demo Init");
        OLED_ShowString(2, 1, "System Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    
    /* Buzzer初始化 */
    buzzer_status = Buzzer_Init();
    if (buzzer_status != BUZZER_OK) {
        LOG_ERROR("MAIN", "Buzzer初始化失败: %d", buzzer_status);
        ErrorHandler_Handle(buzzer_status, "BUZZER");
    } else {
        LOG_INFO("MAIN", "Buzzer已初始化: PWM模式，TIM3 CH1，PA6");
    }
    
    /* TB6612初始化 */
    tb6612_status = TB6612_Init(TB6612_INSTANCE_1);
    if (tb6612_status != TB6612_OK) {
        LOG_ERROR("MAIN", "TB6612初始化失败: %d", tb6612_status);
        ErrorHandler_Handle(tb6612_status, "TB6612");
    } else {
        LOG_INFO("MAIN", "TB6612已初始化: PB3(AIN1), PB4(AIN2), PB5(STBY), TIM3 CH2(PA7)");
    }
    
    /* 立即使能TB6612并设置初始状态 */
    if (tb6612_status == TB6612_OK) {
        tb6612_status = TB6612_Enable(TB6612_INSTANCE_1);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "TB6612使能失败: %d", tb6612_status);
        } else {
            LOG_INFO("MAIN", "TB6612已使能");
        }
    }
    
    Delay_ms(2000);
    
    /* ========== 步骤8：主循环 - 多阶段演示 ========== */
    LOG_INFO("MAIN", "=== 开始多阶段演示 ===");
    
    while (1)
    {
        /* 阶段0：初始化基准 */
        Demo_Stage0_Init(1000, 50.0f, PWM_RESOLUTION_16BIT);
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
        UpdateOLEDDisplay(0, current_freq, 50.0f, current_res);
        
        /* 阶段1：频率变化演示 */
        Demo_Stage1_Frequency();
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
        UpdateOLEDDisplay(1, current_freq, 50.0f, current_res);
        
        /* 阶段2：占空比变化演示 */
        Demo_Stage2_DutyCycle();
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
        UpdateOLEDDisplay(2, current_freq, 50.0f, current_res);
        
        /* 阶段3：分辨率变化演示 */
        Demo_Stage3_Resolution();
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
        UpdateOLEDDisplay(3, current_freq, 5.0f, current_res);
        
        /* 阶段4：参数联动综合演示 */
        Demo_Stage4_Comprehensive();
        PWM_GetFrequency(PWM_INSTANCE_TIM3, &current_freq);
        PWM_GetResolution(PWM_INSTANCE_TIM3, &current_res);
        UpdateOLEDDisplay(4, current_freq, 1.0f, current_res);
        
        /* 演示完成后等待5秒再重复 */
        LOG_INFO("MAIN", "=== 所有阶段演示完成，5秒后重新开始 ===");
        Delay_ms(5000);
    }
}

