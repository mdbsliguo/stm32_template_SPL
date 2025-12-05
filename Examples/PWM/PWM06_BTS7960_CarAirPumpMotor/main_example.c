/**
 * @file main_example.c
 * @brief 案例 - BTS7960电机参数手动控制
 * @example Examples/PWM/PWM06_BTS7960_CarAirPumpMotor/main_example.c
 * @details 通过旋转编码器手动调节BTS7960电机的PWM占空比，OLED实时显示参数值
 *
 * 硬件要求：
 * - BTS7960电机驱动模块（只使用正转方向）：
 *   - R_EN=PA5（STM32控制）
 *   - L_EN：硬件直接连接到VCC（5V），不能悬空
 *   - RPWM=PA8 (TIM1 CH1)
 *   - R_IS：未使用（board.h中配置为NULL, 0）
 * - 旋转编码器：PB4/PB5（TIM3 CH1/CH2，部分重映射），4倍频模式
 * - OLED显示屏：PB8(SCL), PB9(SDA)
 * - UART1：PA9(TX), PA10(RX)，115200波特率
 * - LED1：PA1
 *
 * 操作说明：
 * - 旋转编码器：调节PWM占空比
 *   - 0%往上调：直接跳到30%
 *   - 30%往下调：直接跳到0%
 *   - 其他情况：按5%步进调整（30% ~ 100%）
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
#include "bts7960.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "gpio.h"
#include "timer_pwm.h"
#include "timer_encoder.h"
#include "board.h"
#include "app_init.h"
#include "app_encoder.h"
#include <stdint.h>
#include <string.h>

/* ==================== 全局变量 ==================== */

/* PWM参数（固定值） */
#define PWM_FREQ_FIXED      20000        /**< PWM频率（Hz），固定20kHz */
#define PWM_ARR_FIXED       1023         /**< PWM ARR值（分辨率），固定1023（10位分辨率，1024级） */

/* PWM参数（可调） */
static float g_pwm_duty = 0.0f;             /**< PWM占空比（%），范围：0%（停止）或 30.0% ~ 100.0%（30%对应7.6V），由旋钮直接控制，初始：0%（停止） */

/* 编码器相关（编码器接口模式，轮询方式） */
static int32_t g_encoder_last_count = 0;  /**< 编码器上次读取的计数值 */
static int32_t g_encoder_accumulated = 0;  /**< 编码器累积变化量（用于防抖，累积到4个计数才更新） */

/* 显示更新标志 */
static uint8_t g_update_display = 1;                /**< 显示更新标志 */

/* ==================== 前向声明 ==================== */
static void Update_Parameter(int32_t delta);

/* ==================== 参数更新函数 ==================== */

/**
 * @brief 更新PWM占空比（由旋钮直接控制）
 * @param delta 变化量（正数增加，负数减少，单位：物理步进）
 * @note 占空比范围：0%（停止）或 30.0% ~ 100.0%（30%对应7.6V，低于此值电机可能受损）
 * @note 步进：5%（每1个物理步进对应5%占空比变化）
 * @note 特殊逻辑：0%往上调直接跳到30%，30%往下调直接跳到0%
 * @note 4倍频模式：1个物理步进 = 4个编码器计数 = 5%占空比
 */
static void Update_DutyCycle(int32_t delta)
{
    float old_duty = g_pwm_duty;
    float new_duty;
    
    /* 步进：5%（每1个物理步进对应5%占空比变化） */
    /* 4倍频模式：1个物理步进 = 4个编码器计数 = 5%占空比 */
    const float step = 5.0f;
    
    /* 特殊处理：如果当前是0%（停止） */
    if (g_pwm_duty == 0.0f) {
        if (delta > 0) {
            /* 往上调：直接跳到30%（安全最低值） */
            new_duty = 30.0f;
        } else {
            /* 往下调：保持在0%（已经是停止状态） */
            new_duty = 0.0f;
            return;  /* 已经是0%，无需更新 */
        }
    }
    /* 特殊处理：如果当前是30%，往下调直接跳到0%（停止） */
    else if (g_pwm_duty == 30.0f && delta < 0) {
        new_duty = 0.0f;  /* 从30%直接跳到0% */
    }
    /* 正常情况：在30% ~ 100%之间调整 */
    else {
        /* 计算新占空比 */
        new_duty = g_pwm_duty + (delta * step);
        
        /* 边界检查：30.0% ~ 100.0%（30%对应7.6V，低于此值电机可能受损） */
        if (new_duty < 30.0f) {
            new_duty = 30.0f;  /* 最小30%，对应7.6V */
        } else if (new_duty > 100.0f) {
            new_duty = 100.0f;
        }
    }
    
    g_pwm_duty = new_duty;
    
    /* 更新BTS7960速度和方向 */
    /* 如果占空比为0%，停止电机；否则设置方向为正转并设置速度 */
    if (new_duty == 0.0f) {
        /* 占空比为0%，停止电机 */
        BTS7960_Status_t status = BTS7960_SetDirection(BTS7960_INSTANCE_1, BTS7960_DIR_STOP);
        if (status != BTS7960_OK) {
            LOG_ERROR("MAIN", "设置停止失败: %d", status);
            g_pwm_duty = old_duty;  /* 恢复原值 */
            return;
        }
        BTS7960_SetSpeed(BTS7960_INSTANCE_1, 0.0f);
    } else {
        /* 占空比>0%，设置方向为正转并设置速度 */
        /* 如果从停止状态恢复，先设置方向为正转 */
        if (old_duty == 0.0f) {
            BTS7960_Status_t status = BTS7960_SetDirection(BTS7960_INSTANCE_1, BTS7960_DIR_FORWARD);
            if (status != BTS7960_OK) {
                LOG_ERROR("MAIN", "设置方向失败: %d", status);
                g_pwm_duty = old_duty;  /* 恢复原值 */
                return;
            }
        }
        /* 设置速度 */
        BTS7960_Status_t status = BTS7960_SetSpeed(BTS7960_INSTANCE_1, new_duty);
        if (status != BTS7960_OK) {
            LOG_ERROR("MAIN", "设置速度失败: %d", status);
            g_pwm_duty = old_duty;  /* 恢复原值 */
            return;
        }
    }
    
    g_update_display = 1;  /* 标记需要更新显示 */
    
    /* 串口输出占空比值（旋钮扭动时输出） */
    LOG_INFO("MAIN", "占空比: %.1f%%", new_duty);
}


/**
 * @brief 更新参数（旋钮直接控制占空比）
 * @param delta 变化量（正数增加，负数减少）
 */
static void Update_Parameter(int32_t delta)
{
    Update_DutyCycle(delta);
}

/* ==================== OLED显示函数 ==================== */


/**
 * @brief 更新OLED显示（简化版）
 */
static void Update_OLED_Display(void)
{
    char line[21];  /* OLED每行最多20个字符 */
    
    /* 第1行：状态（0%时显示Stop，否则显示Forward） */
    {
        const char* state_name = (g_pwm_duty == 0.0f) ? "Stop" : "Forward";
        sprintf(line, "State: %-10s", state_name);
        OLED_ShowString(1, 1, line);
    }
    
    /* 第2行：占空比（旋钮直接控制） */
    {
        uint32_t int_part = (uint32_t)g_pwm_duty;
        uint32_t frac_part = (uint32_t)((g_pwm_duty - (float)int_part) * 10.0f);
        sprintf(line, "Duty: %03d.%01d%%", int_part, frac_part);
        OLED_ShowString(2, 1, line);
    }
    
    /* 第3行：固定参数（频率和分辨率） */
    {
        sprintf(line, "Freq: 20kHz 10bit");
        OLED_ShowString(3, 1, line);
    }
    
    /* 第4行：IS状态（R_IS未使用，显示固定信息） */
    {
        sprintf(line, "IS: Disabled");
        OLED_ShowString(4, 1, line);
    }
}


/* ==================== 主函数 ==================== */

int main(void)
{
    /* ========== 初始化阶段 ========== */
    App_Init_System();
    App_Init_Hardware();
    App_Init_PWM();
    App_Init_BTS7960();
    
    if (App_Init_Encoder(&g_encoder_last_count) != 0) {
        while(1) { Delay_ms(1000); }
    }
    
    /* 更新OLED显示初始状态 */
    Update_OLED_Display();
    
    LOG_INFO("MAIN", "进入主循环...");
    
    /* ========== 主循环 ========== */
    while(1)
    {
        App_Process_Encoder(&g_encoder_last_count, 
                           &g_encoder_accumulated, 
                           &g_update_display,
                           Update_Parameter);
        
        /* 更新OLED显示 */
        if (g_update_display) {
            Update_OLED_Display();
            g_update_display = 0;
        }
        
        Delay_ms(1);
    }
}



