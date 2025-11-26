/**
 * @file main_example.c
 * @brief 案例 - PWM频率、占空比手动控制
 * @example Examples/PWM/PWM03_FreqDutyResControl/main_example.c
 * @details 通过旋转编码器手动调节PWM的频率和占空比，OLED显示三个参数值（频率、占空比、分辨率），按钮切换选中项
 *
 * 硬件要求：
 * - 旋转编码器：通道A连接PB0（EXTI Line 0），通道B连接PB1（EXTI Line 1），按钮连接PA4（EXTI Line 4）
 * - 蜂鸣器：PA6（TIM3 CH1）
 * - 电机（TB6612）：PWMA连接PA7（TIM3 CH2），AIN1=PB3，AIN2=PB4，STBY=PB5
 * - LED1：PA1（GPIO模拟PWM），LED2：PA2（GPIO模拟PWM）
 * - OLED显示屏：PB8(SCL), PB9(SDA)
 * - UART1：PA9(TX), PA10(RX)，115200波特率
 *
 * 硬件配置：
 * 在 Examples/PWM/PWM03_FreqDutyResControl/board.h 中配置
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/PWM/PWM03_FreqDutyResControl/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/PWM/PWM03_FreqDutyResControl/board.h 中的配置
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
#include "timer_pwm.h"
#include "tb6612.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "exti.h"
#include "gpio.h"
#include "stm32f10x_gpio.h"
#include "printf_wrapper.h"
#include "board.h"
#include <stdint.h>
#include <string.h>

/* ==================== 参数定义 ==================== */

/**
 * @brief 参数选择枚举
 */
typedef enum {
    PARAM_SELECT_FREQ = 0,   /**< 选中频率 */
    PARAM_SELECT_DUTY = 1,   /**< 选中占空比 */
    PARAM_SELECT_RES = 2,    /**< 选中分辨率 */
    PARAM_SELECT_MAX         /**< 最大选择项 */
} Param_Select_t;

/* ==================== 全局变量 ==================== */

/* PWM参数 */
static uint32_t g_pwm_freq = 2000;         /**< PWM频率（Hz），范围：1000Hz ~ 20000Hz，步进：1000Hz */
static float g_pwm_duty = 0.0f;            /**< PWM占空比（%），范围：0.0% ~ 99.9%，步进：5% */
static uint32_t g_pwm_arr = 32768;         /**< PWM ARR值（分辨率），范围：256 ~ 65536，步进：256，初始值：32768（65536的一半） */

/* 参数选择 */
static Param_Select_t g_current_select = PARAM_SELECT_FREQ;  /**< 当前选中的参数 */

/* 编码器相关 */
static volatile int32_t g_encoder_counter = 0;      /**< 编码器计数器（中断和主循环都会访问） */
static volatile uint8_t g_encoder_last_state = 0xFF;  /**< 编码器上一次状态（初始化为无效状态） */

/* 按钮相关 */
static volatile uint8_t g_button_pressed = 0;       /**< 按钮按下标志（中断设置） */
static uint8_t g_button_last_state = 1;             /**< 按钮上次状态（1=释放，0=按下） */
static uint32_t g_button_last_process_time = 0;     /**< 按钮上次处理时间（用于消抖，tick值） */

/* 显示更新标志 */
static uint8_t g_update_display = 1;                /**< 显示更新标志 */

/* LED GPIO模拟PWM相关 */
static uint32_t g_led_counter = 0;                   /**< LED PWM计数器（0到period-1） */
static const uint32_t LED_PWM_PERIOD = 20;          /**< LED PWM周期（计数器最大值，主循环10ms调用一次，20*10ms=200ms，对应5Hz） */

/* ==================== LED GPIO模拟PWM ==================== */

/**
 * @brief 更新LED亮度（GPIO模拟PWM）
 * @note 在主循环中定期调用，根据占空比控制LED开关
 * @note 使用计数器方式实现非阻塞PWM模拟
 * @note 周期固定为50ms（20Hz），适合人眼观察，不会太闪烁
 */
static void Update_LED_PWM(void)
{
    /* 根据占空比决定LED状态 */
    if (g_pwm_duty <= 0.0f) {
        /* 占空比为0%，LED关闭 */
        LED1_Off();
        LED2_Off();
        g_led_counter = 0;  /* 重置计数器 */
    } else if (g_pwm_duty >= 100.0f) {
        /* 占空比为100%，LED常亮 */
        LED1_On();
        LED2_On();
        g_led_counter = 0;  /* 重置计数器 */
    } else {
        /* 占空比在0%-100%之间，使用PWM模拟 */
        /* 计算开启时间对应的计数器阈值 */
        uint32_t on_threshold = (uint32_t)(LED_PWM_PERIOD * g_pwm_duty / 100.0f);
        
        /* 根据计数器位置决定LED状态 */
        if (g_led_counter < on_threshold) {
            /* 在开启时间内 */
            LED1_On();
            LED2_On();
        } else {
            /* 在关闭时间内 */
            LED1_Off();
            LED2_Off();
        }
        
        /* 递增计数器，达到周期值时重置 */
        g_led_counter++;
        if (g_led_counter >= LED_PWM_PERIOD) {
            g_led_counter = 0;
        }
    }
}

/* ==================== 编码器状态机处理 ==================== */

/**
 * @brief 编码器状态机处理函数（根据状态变化判断方向）
 * @param current_state 当前状态（bit0=通道A，bit1=通道B）
 * @note 编码器旋转一个步进会产生4个状态变化，但只在特定状态变化时计数一次
 *       正转：00 -> 01 -> 11 -> 10 -> 00（只在00->01时计数+1）
 *       反转：00 -> 10 -> 11 -> 01 -> 00（只在00->10时计数-1）
 * @note 防抖原理：只在从00状态变化时计数，因为00是稳定状态，可以避免抖动导致的重复计数
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
    
    /* 如果状态没有变化，忽略（防抖：避免重复处理相同状态） */
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
    
    /* 只在从00状态变化时计数（防抖：避免重复计数） */
    if (last_state == 0x00)
    {
        if (current_state == 0x01)  /* 00 -> 01（正转开始） */
        {
            g_encoder_counter++;  /* 正转：+1 */
        }
        else if (current_state == 0x02)  /* 00 -> 10（反转开始） */
        {
            g_encoder_counter--;  /* 反转：-1 */
        }
        /* 其他状态变化不计数（避免重复计数） */
    }
    /* 其他状态变化不计数（这些是中间状态变化，防抖处理） */
    
    /* 更新上一次状态 */
    g_encoder_last_state = current_state;
}

/* ==================== EXTI中断回调函数 ==================== */

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

/**
 * @brief EXTI4中断回调函数（按钮：PA4）
 * @param line EXTI线号
 * @param user_data 用户数据
 * @note 在中断中只做简单操作，消抖在主循环中处理
 */
static void Button_Callback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    
    /* 下降沿触发中断，说明按钮被按下（从高电平变为低电平）
     * 只设置标志，具体处理在主循环中进行（带消抖和状态检测） */
    
    /* 设置按钮按下标志（主循环中会进行消抖和状态检测） */
    g_button_pressed = 1;
    
    /* 清除EXTI挂起标志 */
    EXTI_ClearPending(EXTI_LINE_4);
}

/* ==================== 参数更新函数 ==================== */

/**
 * @brief 更新PWM频率
 * @param delta 变化量（正数增加，负数减少）
 */
static void Update_Frequency(int32_t delta)
{
    uint32_t old_freq = g_pwm_freq;
    
    /* 固定步进：1000Hz */
    const uint32_t step = 1000;
    
    /* 计算新频率 */
    int32_t new_freq = (int32_t)g_pwm_freq + (delta * (int32_t)step);
    
    /* 边界检查：1000Hz ~ 20000Hz */
    if (new_freq < 1000) {
        new_freq = 1000;
    } else if (new_freq > 20000) {
        new_freq = 20000;
    }
    
    g_pwm_freq = (uint32_t)new_freq;
    
    /* 更新PWM频率（频率改变会影响所有通道，包括蜂鸣器和电机） */
    PWM_Status_t pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, g_pwm_freq);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置频率失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
        g_pwm_freq = old_freq;  /* 恢复原值 */
    } else {
        /* 频率改变后，PWM_SetFrequency会改变ARR值，需要同步g_pwm_arr为实际ARR值 */
        TIM_TypeDef *tim_periph = PWM_GetPeriph(PWM_INSTANCE_TIM3);
        if (tim_periph != NULL) {
            uint32_t actual_arr = tim_periph->ARR + 1;
            /* 向下取整到256的倍数 */
            uint32_t synced_arr = (actual_arr / 256) * 256;
            if (synced_arr < 256) synced_arr = 256;
            if (synced_arr > 65536) synced_arr = 65536;
            
            if (g_pwm_arr != synced_arr) {
                LOG_INFO("MAIN", "频率改变后，ARR已从显示值%d同步为实际值%d", 
                         g_pwm_arr, synced_arr);
                g_pwm_arr = synced_arr;
            }
        }
        
        /* 频率改变后，需要重新设置蜂鸣器、电机和LED的占空比 */
        /* 重新设置蜂鸣器占空比（TIM3 CH1） */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, g_pwm_duty);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "PWM重新设置蜂鸣器占空比失败: %d", pwm_status);
            ErrorHandler_Handle(pwm_status, "PWM");
        }
        
        /* 重新设置电机速度（TB6612使用TIM3 CH2） */
        TB6612_Status_t tb6612_status = TB6612_SetSpeed(TB6612_INSTANCE_1, g_pwm_duty);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "TB6612重新设置速度失败: %d", tb6612_status);
            ErrorHandler_Handle(tb6612_status, "TB6612");
        }
        
        /* 重新设置LED亮度（TIM1 CH1和CH4） */
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM1, PWM_CHANNEL_1, g_pwm_duty);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "PWM重新设置LED1占空比失败: %d", pwm_status);
            ErrorHandler_Handle(pwm_status, "PWM");
        }
        
        pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM1, PWM_CHANNEL_4, g_pwm_duty);
        if (pwm_status != PWM_OK) {
            LOG_ERROR("MAIN", "PWM重新设置LED2占空比失败: %d", pwm_status);
            ErrorHandler_Handle(pwm_status, "PWM");
        }
        
        LOG_INFO("MAIN", "频率已更新: %dHz -> %dHz", old_freq, g_pwm_freq);
    }
}

/**
 * @brief 更新PWM占空比
 * @param delta 变化量（正数增加，负数减少）
 */
static void Update_DutyCycle(int32_t delta)
{
    float old_duty = g_pwm_duty;
    
    /* 固定步进：5% */
    const float step = 5.0f;
    
    /* 计算新占空比 */
    float new_duty = g_pwm_duty + (delta * step);
    
    /* 边界检查：0.0% ~ 99.9% */
    if (new_duty < 0.0f) {
        new_duty = 0.0f;
    } else if (new_duty > 99.9f) {
        new_duty = 99.9f;
    }
    
    g_pwm_duty = new_duty;
    
    /* 更新蜂鸣器占空比（TIM3 CH1） */
    PWM_Status_t pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, g_pwm_duty);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置蜂鸣器占空比失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
        g_pwm_duty = old_duty;  /* 恢复原值 */
        return;
    }
    
    /* 更新电机速度（使用TB6612控制，TIM3 CH2） */
    LOG_DEBUG("MAIN", "准备更新电机速度：%.1f%% -> %.1f%%", old_duty, g_pwm_duty);
    TB6612_Status_t tb6612_status = TB6612_SetSpeed(TB6612_INSTANCE_1, g_pwm_duty);
    if (tb6612_status != TB6612_OK) {
        LOG_ERROR("MAIN", "TB6612设置速度失败: %d (占空比=%.1f%%)", tb6612_status, g_pwm_duty);
        ErrorHandler_Handle(tb6612_status, "TB6612");
        g_pwm_duty = old_duty;  /* 恢复原值 */
        return;
    }
    
    /* LED使用GPIO模拟PWM，在主循环中更新，这里不需要操作 */
    
    LOG_INFO("MAIN", "占空比已更新: %.1f%% -> %.1f%% (频率=%dHz)", old_duty, g_pwm_duty, g_pwm_freq);
    LOG_INFO("MAIN", "  - 蜂鸣器占空比已更新（TIM3 CH1, PA6）");
    LOG_INFO("MAIN", "  - 电机速度已更新（TIM3 CH2, PA7, TB6612）");
    LOG_INFO("MAIN", "  - LED亮度已更新（GPIO模拟PWM, PA1和PA2，在主循环中自动更新）");
    
    /* 提示：电机可能需要更高的占空比才能启动（通常需要20-30%以上） */
    if (g_pwm_duty == 0.0f) {
        LOG_INFO("MAIN", "提示：占空比为0%，电机PWM通道已禁用，电机不会转动");
        LOG_INFO("MAIN", "提示：LED不亮是正常的（占空比0%），请继续旋转编码器调高占空比（步进5%）");
    } else if (g_pwm_duty < 20.0f) {
        LOG_INFO("MAIN", "提示：占空比较低(%.1f%%)，电机可能需要更高的占空比才能启动", g_pwm_duty);
        LOG_INFO("MAIN", "提示：建议调到20-30%以上，电机应该可以转动");
        LOG_INFO("MAIN", "提示：LED亮度会随占空比变化，占空比越高，LED越亮");
    } else {
        LOG_INFO("MAIN", "电机速度已设置为%.1f%%，PWM通道已启用，电机应该可以转动", g_pwm_duty);
        LOG_INFO("MAIN", "如果电机还是不转，请检查硬件连接和电源");
    }
}

/**
 * @brief 更新PWM分辨率（直接操作ARR和CCR）
 * @param delta 变化量（正数增加ARR，负数减少ARR）
 * @note ARR范围：256 ~ 65536，步进：256
 * @note 直接操作ARR和CCR寄存器，保持频率和占空比百分比不变
 * @note 如果ARR值不适合（导致PSC超出范围），会自动修正
 * @note 如果不能再调大时（达到上限或PSC限制），不允许调大
 */
static void Update_Resolution(int32_t delta)
{
    uint32_t old_arr = g_pwm_arr;
    
    /* 获取TIM3外设指针（用于检查PSC限制） */
    TIM_TypeDef *tim_periph = PWM_GetPeriph(PWM_INSTANCE_TIM3);
    if (tim_periph == NULL) {
        LOG_ERROR("MAIN", "获取TIM3外设指针失败");
        return;
    }
    
    /* 获取当前ARR和PSC值（用于计算） */
    uint32_t current_arr = tim_periph->ARR + 1;
    uint32_t current_psc = tim_periph->PSC;
    
    /* 获取TIM3时钟频率 */
    SystemCoreClockUpdate();
    uint32_t apb1_prescaler = (RCC->CFGR >> 8) & 0x07;
    uint32_t apb1_clk;
    if (apb1_prescaler < 4) {
        apb1_clk = SystemCoreClock;
    } else {
        uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
        apb1_clk = SystemCoreClock >> presc_table[apb1_prescaler];
    }
    uint32_t tim_clk = (apb1_prescaler >= 4) ? (apb1_clk * 2) : apb1_clk;
    
    /* 使用全局变量g_pwm_freq作为当前频率（更准确） */
    uint32_t current_freq = g_pwm_freq;
    
    /* 固定步进：256 */
    const uint32_t step = 256;
    
    /* 计算新ARR值 */
    int32_t new_arr = (int32_t)g_pwm_arr + (delta * (int32_t)step);
    
    /* 边界检查：256 ~ 65536，并确保是256的倍数 */
    if (new_arr < 256) {
        new_arr = 256;
    } else if (new_arr > 65536) {
        new_arr = 65536;
    } else {
        /* 确保是256的倍数（向下取整到最近的256倍数） */
        new_arr = (new_arr / 256) * 256;
        if (new_arr < 256) new_arr = 256;
    }
    
    /* 检查调大时是否会超出PSC限制 */
    if (delta > 0 && new_arr > g_pwm_arr) {
        /* 调大ARR，检查PSC是否会超出范围 */
        uint32_t test_psc = (tim_clk / (new_arr * current_freq)) - 1;
        if (test_psc > 65535) {
            /* PSC会超出范围，计算允许的最大ARR值 */
            uint32_t max_arr = tim_clk / (current_freq * 65536);
            /* 向下取整到256的倍数 */
            max_arr = (max_arr / 256) * 256;
            if (max_arr < 256) max_arr = 256;
            if (max_arr > 65536) max_arr = 65536;
            
            /* 如果计算出的最大ARR小于当前ARR，说明已经达到上限 */
            if (max_arr <= g_pwm_arr) {
                LOG_INFO("MAIN", "ARR已达到上限（频率=%dHz时最大ARR=%d），不允许继续调大", 
                         current_freq, max_arr);
                return;  /* 不允许调大 */
            }
            
            /* 限制ARR为允许的最大值 */
            new_arr = max_arr;
            LOG_INFO("MAIN", "ARR调大会导致PSC超出范围，已自动限制为: %d (频率=%dHz时的最大值)", 
                     new_arr, current_freq);
        }
    }
    
    g_pwm_arr = (uint32_t)new_arr;
    
    /* 如果ARR改变了，直接操作ARR和CCR */
    if (g_pwm_arr != old_arr)
    {
        /* 保存旧的PSC值（用于日志输出） */
        uint32_t old_psc = current_psc;
        
        /* 新的ARR值 */
        uint32_t new_arr = g_pwm_arr;
        
        /* 重新计算PSC以保持频率不变 */
        uint32_t new_psc = (tim_clk / (new_arr * current_freq)) - 1;
        
        /* 检查PSC是否超出范围，如果超出则自动修正ARR */
        if (new_psc > 65535) {
            /* PSC太大，说明ARR太小，需要增大ARR，但保持256的倍数 */
            /* 计算所需的最小ARR值：tim_clk / (current_freq * 65536) */
            uint32_t min_arr = tim_clk / (current_freq * 65536);
            
            /* 向上取整到256的倍数 */
            uint32_t adjusted_arr = ((min_arr + 255) / 256) * 256;
            if (adjusted_arr < 256) adjusted_arr = 256;
            if (adjusted_arr > 65536) adjusted_arr = 65536;
            
            /* 自动修正ARR为计算出的值（保持256的倍数） */
            new_arr = adjusted_arr;
            g_pwm_arr = new_arr;  /* 更新全局变量 */
            
            /* 重新计算PSC */
            new_psc = (tim_clk / (new_arr * current_freq)) - 1;
            if (new_psc > 65535) new_psc = 65535;
            
            LOG_INFO("MAIN", "ARR值%d不适合（频率=%dHz时PSC超出范围），已自动修正为: %d (保持256的倍数)", 
                     old_arr, current_freq, g_pwm_arr);
        }
        
        /* 获取当前CCR值（用于重新计算） */
        uint32_t old_ccr1 = tim_periph->CCR1;  /* 蜂鸣器通道 */
        uint32_t old_ccr2 = tim_periph->CCR2;  /* 电机通道 */
        
        /* 计算当前占空比百分比 */
        float old_duty_percent1 = (float)old_ccr1 * 100.0f / (float)current_arr;
        float old_duty_percent2 = (float)old_ccr2 * 100.0f / (float)current_arr;
        
        /* 计算新的CCR值（保持占空比百分比不变） */
        uint32_t new_ccr1 = (uint32_t)((float)new_arr * old_duty_percent1 / 100.0f);
        uint32_t new_ccr2 = (uint32_t)((float)new_arr * old_duty_percent2 / 100.0f);
        
        if (new_ccr1 > new_arr) new_ccr1 = new_arr;
        if (new_ccr2 > new_arr) new_ccr2 = new_arr;
        
        /* 禁用定时器（在修改ARR和PSC时） */
        TIM_Cmd(tim_periph, DISABLE);
        
        /* 设置新的ARR值 */
        TIM_SetAutoreload(tim_periph, new_arr - 1);
        
        /* 设置新的PSC值 */
        TIM_PrescalerConfig(tim_periph, new_psc, TIM_PSCReloadMode_Immediate);
        
        /* 设置新的CCR值 */
        TIM_SetCompare1(tim_periph, new_ccr1);  /* 蜂鸣器 */
        TIM_SetCompare2(tim_periph, new_ccr2);  /* 电机 */
        
        /* 重新使能定时器 */
        TIM_Cmd(tim_periph, ENABLE);
        
        LOG_INFO("MAIN", "分辨率(ARR)已更新: %d -> %d", old_arr, g_pwm_arr);
        LOG_INFO("MAIN", "PSC: %d -> %d, 频率: %dHz (保持不变)", 
                 old_psc, new_psc, current_freq);
        LOG_INFO("MAIN", "CCR1(蜂鸣器): %d -> %d (占空比%.1f%%保持不变)", 
                 old_ccr1, new_ccr1, old_duty_percent1);
        LOG_INFO("MAIN", "CCR2(电机): %d -> %d (占空比%.1f%%保持不变)", 
                 old_ccr2, new_ccr2, old_duty_percent2);
    }
}

/**
 * @brief 更新参数（根据当前选中项）
 * @param delta 变化量（正数增加，负数减少）
 */
static void Update_Parameter(int32_t delta)
{
    switch (g_current_select) {
        case PARAM_SELECT_FREQ:
            Update_Frequency(delta);
            break;
        case PARAM_SELECT_DUTY:
            Update_DutyCycle(delta);
            break;
        case PARAM_SELECT_RES:
            Update_Resolution(delta);
            break;
        default:
            break;
    }
}

/* ==================== OLED显示函数 ==================== */

/**
 * @brief 更新OLED显示
 */
static void Update_OLED_Display(void)
{
    /* 第1行：标题 */
    OLED_ShowString(1, 1, "PWM03 Control");
    
    /* 每次更新时，重新显示所有行，确保箭头正确显示 */
    /* 第2行：频率（5位数字，不足补0，支持20kHz） */
    if (g_current_select == PARAM_SELECT_FREQ) {
        Printf_OLED2("Freq:%05dHz <-", g_pwm_freq);
    } else {
        Printf_OLED2("Freq:%05dHz    ", g_pwm_freq);  /* 添加空格清除箭头 */
    }
    
    /* 第3行：占空比（整数部分2位，不足补0） */
    {
        /* 手动格式化：整数部分2位，小数部分1位 */
        uint32_t int_part = (uint32_t)g_pwm_duty;
        uint32_t frac_part = (uint32_t)((g_pwm_duty - (float)int_part) * 10.0f);
        if (g_current_select == PARAM_SELECT_DUTY) {
            Printf_OLED3("Duty:%02d.%01d%% <-", int_part, frac_part);
        } else {
            Printf_OLED3("Duty:%02d.%01d%%    ", int_part, frac_part);  /* 添加空格清除箭头 */
        }
    }
    
    /* 第4行：分辨率（ARR值） */
    if (g_current_select == PARAM_SELECT_RES) {
        Printf_OLED4("ARR: %05d <-", g_pwm_arr);
    } else {
        Printf_OLED4("ARR: %05d    ", g_pwm_arr);  /* 添加空格清除箭头 */
    }
}

/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    SoftI2C_Status_t i2c_status;
    OLED_Status_t oled_status;
    PWM_Status_t pwm_status;
    EXTI_Status_t exti_status;
    int32_t last_encoder_counter = 0;
    uint32_t last_encoder_process_time = 0;  /**< 上次编码器处理时间（用于消抖） */
    
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
    LOG_INFO("MAIN", "=== PWM03 频率、占空比手动控制 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化其他模块（按依赖顺序） ========== */
    
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
        OLED_ShowString(1, 1, "PWM03 Init");
        OLED_ShowString(2, 1, "System Ready");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    
    /* LED初始化（PA1和PA2，使用GPIO模拟PWM） */
    LED_Status_t led_status = LED_Init();
    if (led_status != LED_OK) {
        LOG_ERROR("MAIN", "LED初始化失败: %d", led_status);
        ErrorHandler_Handle(led_status, "LED");
    } else {
        LOG_INFO("MAIN", "LED已初始化: LED1(PA1), LED2(PA2)");
        LOG_INFO("MAIN", "LED使用GPIO模拟PWM控制亮度，周期约200ms（5Hz），与占空比参数同步");
        if (g_pwm_duty == 0.0f) {
            LOG_INFO("MAIN", "提示：当前占空比为0%%，LED不亮是正常的。请旋转编码器增加占空比（步进5%%）");
        } else {
            LOG_INFO("MAIN", "提示：占空比为%.1f%%，LED亮度会根据占空比变化", g_pwm_duty);
        }
    }
    
    /* 初始化TIM3（蜂鸣器和马达） */
    pwm_status = PWM_Init(PWM_INSTANCE_TIM3);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM TIM3初始化失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
    } else {
        LOG_INFO("MAIN", "PWM TIM3已初始化: CH1(PA6蜂鸣器), CH2(PA7马达)");
    }
    
    /* 设置PWM频率（这会设置ARR为默认值，我们需要在之后更新为32768） */
    pwm_status = PWM_SetFrequency(PWM_INSTANCE_TIM3, g_pwm_freq);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置频率失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
    } else {
        LOG_INFO("MAIN", "PWM频率已设置为: %dHz", g_pwm_freq);
    }
    
    /* 设置PWM初始ARR值（32768，65536的一半） */
    /* 使用Update_Resolution来设置ARR值，保持频率和占空比不变 */
    LOG_INFO("MAIN", "设置PWM初始ARR值: %d", g_pwm_arr);
    Update_Resolution(0);  /* delta=0不会改变ARR，但我们需要手动设置 */
    
    /* 手动设置ARR值到定时器 */
    TIM_TypeDef *tim_periph = PWM_GetPeriph(PWM_INSTANCE_TIM3);
    if (tim_periph != NULL) {
        /* 获取当前频率和PSC，重新计算PSC以保持频率不变 */
        uint32_t current_arr = tim_periph->ARR + 1;
        uint32_t current_psc = tim_periph->PSC;
        
        /* 获取TIM3时钟频率 */
        SystemCoreClockUpdate();
        uint32_t apb1_prescaler = (RCC->CFGR >> 8) & 0x07;
        uint32_t apb1_clk;
        if (apb1_prescaler < 4) {
            apb1_clk = SystemCoreClock;
        } else {
            uint8_t presc_table[] = {0, 0, 0, 0, 1, 2, 3, 4};
            apb1_clk = SystemCoreClock >> presc_table[apb1_prescaler];
        }
        uint32_t tim_clk = (apb1_prescaler >= 4) ? (apb1_clk * 2) : apb1_clk;
        
        /* 计算当前频率 */
        uint32_t current_freq = tim_clk / ((current_psc + 1) * current_arr);
        
        /* 重新计算PSC以保持频率不变 */
        uint32_t new_psc = (tim_clk / (g_pwm_arr * current_freq)) - 1;
        if (new_psc > 65535) new_psc = 65535;
        
        /* 获取当前CCR值 */
        uint32_t current_ccr1 = tim_periph->CCR1;
        uint32_t current_ccr2 = tim_periph->CCR2;
        
        /* 计算当前占空比百分比 */
        float duty_percent1 = (float)current_ccr1 * 100.0f / (float)current_arr;
        float duty_percent2 = (float)current_ccr2 * 100.0f / (float)current_arr;
        
        /* 计算新的CCR值（保持占空比百分比不变） */
        uint32_t new_ccr1 = (uint32_t)((float)g_pwm_arr * duty_percent1 / 100.0f);
        uint32_t new_ccr2 = (uint32_t)((float)g_pwm_arr * duty_percent2 / 100.0f);
        if (new_ccr1 > g_pwm_arr) new_ccr1 = g_pwm_arr;
        if (new_ccr2 > g_pwm_arr) new_ccr2 = g_pwm_arr;
        
        /* 禁用定时器 */
        TIM_Cmd(tim_periph, DISABLE);
        
        /* 设置新的ARR值 */
        TIM_SetAutoreload(tim_periph, g_pwm_arr - 1);
        
        /* 设置新的PSC值 */
        TIM_PrescalerConfig(tim_periph, new_psc, TIM_PSCReloadMode_Immediate);
        
        /* 设置新的CCR值 */
        TIM_SetCompare1(tim_periph, new_ccr1);
        TIM_SetCompare2(tim_periph, new_ccr2);
        
        /* 重新使能定时器 */
        TIM_Cmd(tim_periph, ENABLE);
        
        LOG_INFO("MAIN", "PWM初始ARR值已设置为: %d", g_pwm_arr);
    }
    
    /* TB6612初始化（电机驱动，会初始化TIM3 CH2的PWM） */
    /* 注意：TB6612_Init内部会调用PWM_Init，但我们已经初始化了PWM，这应该没问题（PWM_Init应该检查是否已初始化） */
    TB6612_Status_t tb6612_status = TB6612_Init(TB6612_INSTANCE_1);
    if (tb6612_status != TB6612_OK) {
        LOG_ERROR("MAIN", "TB6612初始化失败: %d", tb6612_status);
        ErrorHandler_Handle(tb6612_status, "TB6612");
        LOG_ERROR("MAIN", "=== TB6612诊断信息 ===");
        LOG_ERROR("MAIN", "1. 检查TB6612电源是否连接（VCC和GND）");
        LOG_ERROR("MAIN", "2. 检查PB3(AIN1)、PB4(AIN2)、PB5(STBY)连接是否正确");
        LOG_ERROR("MAIN", "3. 检查PA7(PWMA)连接是否正确");
        LOG_ERROR("MAIN", "4. 检查电机电源是否连接（VM和GND）");
    } else {
        LOG_INFO("MAIN", "TB6612已初始化: PB3(AIN1), PB4(AIN2), PB5(STBY), TIM3 CH2(PA7)");
    }
    
    /* 使能TB6612并设置初始方向 */
    if (tb6612_status == TB6612_OK) {
        tb6612_status = TB6612_Enable(TB6612_INSTANCE_1);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "TB6612使能失败: %d", tb6612_status);
            ErrorHandler_Handle(tb6612_status, "TB6612");
        } else {
            LOG_INFO("MAIN", "TB6612已使能（STBY=高电平）");
        }
        
        /* 设置电机方向为正转 */
        tb6612_status = TB6612_SetDirection(TB6612_INSTANCE_1, TB6612_DIR_FORWARD);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "TB6612设置方向失败: %d", tb6612_status);
            ErrorHandler_Handle(tb6612_status, "TB6612");
        } else {
            LOG_INFO("MAIN", "TB6612方向已设置为正转（AIN1=高，AIN2=低）");
        }
        
        /* 设置电机初始速度（占空比） */
        /* 注意：如果占空比为0%，TB6612_SetSpeed会禁用PWM通道，电机不会转 */
        tb6612_status = TB6612_SetSpeed(TB6612_INSTANCE_1, g_pwm_duty);
        if (tb6612_status != TB6612_OK) {
            LOG_ERROR("MAIN", "TB6612设置速度失败: %d (占空比=%.1f%%)", tb6612_status, g_pwm_duty);
            ErrorHandler_Handle(tb6612_status, "TB6612");
            LOG_ERROR("MAIN", "检查PA7(PWMA)引脚连接是否正确");
        } else {
            LOG_INFO("MAIN", "电机速度已设置为: %.1f%%", g_pwm_duty);
            if (g_pwm_duty == 0.0f) {
                LOG_INFO("MAIN", "提示：占空比为0%，电机PWM通道已禁用，电机不会转动");
                LOG_INFO("MAIN", "提示：请使用旋转编码器调高占空比（步进5%）");
            } else {
                LOG_INFO("MAIN", "电机应该可以转动，如果不动，请检查：");
                LOG_INFO("MAIN", "  1. 电机电源（VM和GND）是否连接");
                LOG_INFO("MAIN", "  2. 电机是否连接到TB6612的A01/A02或B01/B02");
                LOG_INFO("MAIN", "  3. 占空比是否足够高（建议至少20-30%）");
            }
        }
    }
    
    /* 设置蜂鸣器占空比（TIM3 CH1，PA6） */
    pwm_status = PWM_SetDutyCycle(PWM_INSTANCE_TIM3, PWM_CHANNEL_1, g_pwm_duty);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM设置蜂鸣器占空比失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
    } else {
        LOG_INFO("MAIN", "蜂鸣器占空比已设置为: %.1f%%", g_pwm_duty);
    }
    
    /* 使能蜂鸣器PWM通道 */
    pwm_status = PWM_EnableChannel(PWM_INSTANCE_TIM3, PWM_CHANNEL_1);
    if (pwm_status != PWM_OK) {
        LOG_ERROR("MAIN", "PWM使能蜂鸣器通道失败: %d", pwm_status);
        ErrorHandler_Handle(pwm_status, "PWM");
    } else {
        LOG_INFO("MAIN", "蜂鸣器PWM通道已使能");
    }
    
    /* ========== 步骤8：初始化EXTI（编码器和按钮） ========== */
    
    /* 初始化EXTI0（编码器通道A：PB0） */
    exti_status = EXTI_HW_Init(EXTI_LINE_0, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI0初始化失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 重新配置PB0为上拉输入 */
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
    
    /* 初始化EXTI1（编码器通道B：PB1） */
    exti_status = EXTI_HW_Init(EXTI_LINE_1, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI1初始化失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 重新配置PB1为上拉输入 */
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
    
    /* 初始化EXTI4（按钮：PA4） */
    exti_status = EXTI_HW_Init(EXTI_LINE_4, EXTI_TRIGGER_FALLING, EXTI_MODE_INTERRUPT);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI4初始化失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 重新配置PA4为上拉输入 */
    GPIO_Config(GPIOA, GPIO_Pin_4, GPIO_MODE_INPUT_PULLUP, GPIO_SPEED_50MHz);
    
    /* 设置EXTI4中断回调函数 */
    exti_status = EXTI_SetCallback(EXTI_LINE_4, Button_Callback, NULL);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI4回调设置失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 清除可能的挂起标志 */
    EXTI_ClearPending(EXTI_LINE_4);
    
    /* 使能EXTI4中断 */
    exti_status = EXTI_Enable(EXTI_LINE_4);
    if (exti_status != EXTI_OK)
    {
        LOG_ERROR("MAIN", "EXTI4使能失败: %d", exti_status);
        ErrorHandler_Handle(exti_status, "EXTI");
        while(1) { Delay_ms(1000); }
    }
    
    /* 读取初始按钮状态 */
    uint8_t button_init_state = GPIO_ReadPin(GPIOA, GPIO_Pin_4);
    LOG_INFO("MAIN", "EXTI4已初始化: PA4（按钮），下降沿触发");
    LOG_INFO("MAIN", "按钮初始状态: %s (GPIO值=%d)", 
             (button_init_state == Bit_RESET) ? "按下(低电平)" : "释放(高电平)", 
             button_init_state);
    LOG_INFO("MAIN", "提示：如果按钮按下时输出高电平，需要改为上升沿触发");
    
    /* 初始化编码器状态（读取初始状态） */
    {
        uint8_t state_a = GPIO_ReadPin(GPIOB, GPIO_Pin_0) ? 1 : 0;
        uint8_t state_b = GPIO_ReadPin(GPIOB, GPIO_Pin_1) ? 1 : 0;
        g_encoder_last_state = (state_a) | (state_b << 1);
    }
    
    LOG_INFO("MAIN", "编码器初始化完成，开始检测旋转方向");
    LOG_INFO("MAIN", "初始参数：频率=%dHz，占空比=%.1f%%，ARR=%d", 
             g_pwm_freq, g_pwm_duty, g_pwm_arr);
    LOG_INFO("MAIN", "PWM输出引脚：");
    LOG_INFO("MAIN", "  - 蜂鸣器：PA6 (TIM3 CH1, 硬件PWM)");
    LOG_INFO("MAIN", "  - 电机：PA7 (TIM3 CH2, TB6612 PWMA, 硬件PWM)");
    LOG_INFO("MAIN", "LED控制引脚：");
    LOG_INFO("MAIN", "  - LED1：PA1 (GPIO模拟PWM，亮度与占空比同步)");
    LOG_INFO("MAIN", "  - LED2：PA2 (GPIO模拟PWM，亮度与占空比同步)");
    LOG_INFO("MAIN", "TB6612控制引脚：");
    LOG_INFO("MAIN", "  - PB3 (AIN1), PB4 (AIN2), PB5 (STBY)");
    
    /* 测试：强制设置一个较高的占空比（30%），验证电机是否能转 */
    LOG_INFO("MAIN", "=== 电机测试：强制设置占空比为30% ===");
    TB6612_Status_t test_status = TB6612_SetSpeed(TB6612_INSTANCE_1, 30.0f);
    if (test_status == TB6612_OK) {
        LOG_INFO("MAIN", "测试占空比30%已设置，电机应该开始转动");
        LOG_INFO("MAIN", "如果电机不转，请检查：");
        LOG_INFO("MAIN", "  1. TB6612电源（VCC和GND）是否连接");
        LOG_INFO("MAIN", "  2. 电机电源（VM和GND）是否连接，电压是否足够（建议5-12V）");
        LOG_INFO("MAIN", "  3. 电机是否连接到TB6612的A01/A02或B01/B02");
        LOG_INFO("MAIN", "  4. PB3(AIN1)、PB4(AIN2)、PB5(STBY)、PA7(PWMA)连接是否正确");
        LOG_INFO("MAIN", "  5. 使用万用表检查PB3、PB4、PB5的电平是否正确");
        Delay_ms(2000);  /* 等待2秒，观察电机是否转动 */
        /* 恢复初始占空比 */
        TB6612_SetSpeed(TB6612_INSTANCE_1, g_pwm_duty);
        LOG_INFO("MAIN", "测试完成，已恢复初始占空比%.1f%%", g_pwm_duty);
    } else {
        LOG_ERROR("MAIN", "测试失败：TB6612设置速度失败: %d", test_status);
    }
    LOG_INFO("MAIN", "=== 按钮使用说明 ===");
    LOG_INFO("MAIN", "按钮功能：按下按钮切换选中项（频率 -> 占空比 -> 分辨率 -> 频率）");
    LOG_INFO("MAIN", "按钮优化：消抖时间30ms，主循环中直接检测GPIO状态，响应更快");
    LOG_INFO("MAIN", "按钮连接：PA4，按下时输出低电平（连接GND），使用下降沿触发");
    LOG_INFO("MAIN", "按钮提示：按下按钮后，OLED显示的箭头会移动到下一个参数");
    
    /* ========== 步骤9：主循环 ========== */
    while(1)
    {
        /* 1. 检查按钮状态，切换选中项（优化：简化逻辑，提高响应速度） */
        /* 在主循环中直接读取GPIO状态，使用边沿检测 */
        uint8_t button_current_state = GPIO_ReadPin(GPIOA, GPIO_Pin_4);
        
        /* 检测按钮按下边沿（从高电平变为低电平） */
        if (g_button_last_state == 1 && button_current_state == 0)
        {
            /* 检测到按下边沿，立即处理（不等待释放） */
            uint32_t current_time = Delay_GetTick();
            uint32_t time_since_last_process;
            
            /* 计算距离上次处理的时间间隔 */
            if (current_time >= g_button_last_process_time)
            {
                time_since_last_process = current_time - g_button_last_process_time;
            }
            else
            {
                /* 处理溢出情况 */
                time_since_last_process = (0xFFFFFFFF - g_button_last_process_time) + current_time + 1;
            }
            
            /* 消抖时间：10ms（如果距离上次处理时间小于10ms，忽略此次按下） */
            if (time_since_last_process >= 10)
            {
                /* 消抖时间已过，处理按钮按下事件 */
                /* 切换选中项（频率 → 占空比 → 分辨率 → 频率） */
                Param_Select_t old_select = g_current_select;
                g_current_select = (Param_Select_t)((g_current_select + 1) % PARAM_SELECT_MAX);
                g_update_display = 1;
                
                /* 更新上次处理时间 */
                g_button_last_process_time = current_time;
                
                const char* select_names[] = {"频率", "占空比", "分辨率"};
                LOG_INFO("MAIN", "按钮按下！选中项已切换: %s -> %s", 
                         select_names[old_select],
                         select_names[g_current_select]);
            }
        }
        
        /* 更新按钮状态（用于下次检测） */
        g_button_last_state = button_current_state;
        
        /* 清除中断标志（如果设置了） */
        if (g_button_pressed) {
            g_button_pressed = 0;
        }
        
        /* 2. 检查编码器计数变化，更新参数（带时间消抖） */
        if (g_encoder_counter != last_encoder_counter)
        {
            uint32_t current_time = Delay_GetTick();
            uint32_t time_since_last_process = current_time - last_encoder_process_time;
            
            /* 根据当前选中的参数使用不同的消抖时间 */
            uint32_t debounce_time;
            if (g_current_select == PARAM_SELECT_RES) {
                /* ARR参数需要更长的消抖时间（150ms），因为ARR变化会影响PSC和CCR的计算 */
                debounce_time = 150;
            } else {
                /* 频率和占空比使用较短的消抖时间（50ms） */
                debounce_time = 50;
            }
            
            /* 时间消抖：距离上次处理至少debounce_time ms才更新参数（避免快速旋转时参数变化太快） */
            if (time_since_last_process >= debounce_time)
            {
                int32_t delta = g_encoder_counter - last_encoder_counter;
                Update_Parameter(delta);
                last_encoder_counter = g_encoder_counter;
                last_encoder_process_time = current_time;
                g_update_display = 1;
            }
            else
            {
                LOG_DEBUG("MAIN", "编码器消抖：距离上次处理=%dms < %dms，忽略", 
                         time_since_last_process, debounce_time);
            }
        }
        
        /* 3. 检查ARR显示值是否大于实际值，如果是则同步为实际值并刷新OLED */
        /* 场景说明：
         * - 频率改变时，PWM_SetFrequency会改变ARR，但g_pwm_arr不会自动更新
         * - 这会导致OLED显示的ARR值大于实际ARR值
         * - 需要检查并同步，确保显示值与实际值一致
         */
        TIM_TypeDef *tim_periph = PWM_GetPeriph(PWM_INSTANCE_TIM3);
        if (tim_periph != NULL) {
            uint32_t actual_arr = tim_periph->ARR + 1;
            /* 如果显示值大于实际值，将显示值同步为实际值（向下取整到256的倍数） */
            if (g_pwm_arr > actual_arr) {
                /* 向下取整到256的倍数 */
                uint32_t synced_arr = (actual_arr / 256) * 256;
                if (synced_arr < 256) synced_arr = 256;
                if (synced_arr > 65536) synced_arr = 65536;
                
                LOG_INFO("MAIN", "ARR显示值(%d)大于实际值(%d)，已同步为: %d并刷新OLED", 
                         g_pwm_arr, actual_arr, synced_arr);
                g_pwm_arr = synced_arr;
                g_update_display = 1;  /* 触发OLED显示更新 */
            }
        }
        
        /* 4. 更新LED亮度（GPIO模拟PWM） */
        Update_LED_PWM();
        
        /* 5. 更新OLED显示 */
        if (g_update_display)
        {
            Update_OLED_Display();
            g_update_display = 0;
        }
        
        /* 6. 按钮状态监控（用于诊断，每1秒输出一次） */
        static uint32_t last_debug_time = 0;
        uint32_t current_time = Delay_GetTick();
        if ((current_time - last_debug_time) >= 1000)
        {
            uint8_t button_state = GPIO_ReadPin(GPIOA, GPIO_Pin_4);
            LOG_DEBUG("MAIN", "按钮状态监控: GPIO=%d (%s), 按钮标志=%d", 
                     button_state, 
                     (button_state == Bit_RESET) ? "低电平" : "高电平",
                     g_button_pressed);
            last_debug_time = current_time;
        }
        
        /* 6. 延时降低CPU占用率 */
        Delay_ms(10);
    }
}

