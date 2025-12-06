/**
 * @file bts7960.c
 * @brief BTS7960大电流H桥电机驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的BTS7960驱动，支持正反转、制动、PWM速度控制、电流报警检测
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_BTS7960_ENABLED

/* Include our header */
#include "bts7960.h"

#include "gpio.h"
#include "timer_pwm.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include <stdbool.h>
#include <stddef.h>

/* 从board.h加载配置 */
static BTS7960_Config_t g_bts7960_configs[BTS7960_INSTANCE_MAX] __attribute__((unused)) = BTS7960_CONFIGS;

/* 初始化标志 */
static bool g_bts7960_initialized[BTS7960_INSTANCE_MAX] __attribute__((unused)) = {false, false};

/* 当前速度（用于方向切换时保持速度） */
static float g_bts7960_speed[BTS7960_INSTANCE_MAX] __attribute__((unused)) = {0.0f, 0.0f};

/* 当前方向 */
static BTS7960_Direction_t g_bts7960_direction[BTS7960_INSTANCE_MAX] __attribute__((unused)) = {BTS7960_DIR_STOP, BTS7960_DIR_STOP};

/* PWM频率（默认20kHz，≤25kHz限制） */
static uint32_t g_bts7960_pwm_frequency[BTS7960_INSTANCE_MAX] __attribute__((unused)) = {20000, 20000};

/* 电流报警回调函数 */
static BTS7960_CurrentAlarmCallback_t g_bts7960_alarm_callback[BTS7960_INSTANCE_MAX] __attribute__((unused)) = {NULL, NULL};

/**
 * @brief 获取GPIO时钟使能值
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值，失败返回0
 */
static uint32_t BTS7960_GetGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    if (port == GPIOE) return RCC_APB2Periph_GPIOE;
    if (port == GPIOF) return RCC_APB2Periph_GPIOF;
    if (port == GPIOG) return RCC_APB2Periph_GPIOG;
    return 0;
}

/**
 * @brief BTS7960初始化
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Init(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (g_bts7960_initialized[instance]) {
        return BTS7960_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    if (!cfg->enabled) {
        return BTS7960_ERROR_INVALID_PARAM;  /* 配置未启用 */
    }
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* ========== 0. 禁用JTAG功能（如果使用PB3/PB4） ========== */
    if ((cfg->r_en_port == GPIOB && (cfg->r_en_pin == GPIO_Pin_3 || cfg->r_en_pin == GPIO_Pin_4)) ||
        (cfg->l_en_port == GPIOB && (cfg->l_en_pin == GPIO_Pin_3 || cfg->l_en_pin == GPIO_Pin_4)) ||
        (cfg->r_is_port != NULL && cfg->r_is_port == GPIOB && (cfg->r_is_pin == GPIO_Pin_3 || cfg->r_is_pin == GPIO_Pin_4)) ||
        (cfg->l_is_port != NULL && cfg->l_is_port == GPIOB && (cfg->l_is_pin == GPIO_Pin_3 || cfg->l_is_pin == GPIO_Pin_4))) {
        /* 使能AFIO时钟 */
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        /* 禁用JTAG，但保留SWD（用于调试） */
        GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    }
    
    /* ========== 1. 配置使能引脚（R_EN/L_EN） ========== */
    if (cfg->r_en_port != NULL && cfg->r_en_pin != 0) {
        uint32_t gpio_clock = BTS7960_GetGPIOClock(cfg->r_en_port);
        if (gpio_clock == 0) {
            return BTS7960_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->r_en_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->r_en_port, &GPIO_InitStructure);
        GPIO_ResetPin(cfg->r_en_port, cfg->r_en_pin);  /* 初始状态：低电平（禁用） */
    }
    
    if (cfg->l_en_port != NULL && cfg->l_en_pin != 0) {
        uint32_t gpio_clock = BTS7960_GetGPIOClock(cfg->l_en_port);
        if (gpio_clock == 0) {
            return BTS7960_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->l_en_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->l_en_port, &GPIO_InitStructure);
        GPIO_ResetPin(cfg->l_en_port, cfg->l_en_pin);  /* 初始状态：低电平（禁用） */
    }
    
    /* ========== 2. 配置电流报警输出引脚（R_IS/L_IS，可选） ========== */
    /* 注意：使用下拉输入（GPIO_Mode_IPD），这样如果R_IS/L_IS引脚未连接，会读取到低电平（0），不会被误判为过流 */
    /* 重要：如果R_IS/L_IS引脚连接了BTS7960，BTS7960输出5V高电平，需要电平转换电路（5V转3.3V），否则会损坏STM32 GPIO */
    /* BTS7960的R_IS/L_IS：正常=低电平(0V)，过流=高电平(5V) */
    if (cfg->r_is_port != NULL && cfg->r_is_pin != 0) {
        uint32_t gpio_clock = BTS7960_GetGPIOClock(cfg->r_is_port);
        if (gpio_clock == 0) {
            return BTS7960_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->r_is_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;  /* 下拉输入：未连接时读取低电平(0)，连接BTS7960时正常读取（需要电平转换） */
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->r_is_port, &GPIO_InitStructure);
    }
    
    if (cfg->l_is_port != NULL && cfg->l_is_pin != 0) {
        uint32_t gpio_clock = BTS7960_GetGPIOClock(cfg->l_is_port);
        if (gpio_clock == 0) {
            return BTS7960_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->l_is_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;  /* 下拉输入：未连接时读取低电平(0)，连接BTS7960时正常读取（需要电平转换） */
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->l_is_port, &GPIO_InitStructure);
    }
    
    /* ========== 3. 初始化PWM（RPWM和LPWM） ========== */
    /* 检查RPWM配置是否有效 */
    if (cfg->rpwm_instance >= PWM_INSTANCE_MAX || cfg->rpwm_channel >= PWM_CHANNEL_MAX) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    /* 检查LPWM配置是否有效 */
    if (cfg->lpwm_instance >= PWM_INSTANCE_MAX || cfg->lpwm_channel >= PWM_CHANNEL_MAX) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    /* 初始化RPWM */
    PWM_Status_t pwm_status = PWM_Init((PWM_Instance_t)cfg->rpwm_instance);
    if (pwm_status != PWM_OK) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    /* 初始化LPWM（如果与RPWM不是同一个实例，需要单独初始化） */
    if (cfg->lpwm_instance != cfg->rpwm_instance) {
        pwm_status = PWM_Init((PWM_Instance_t)cfg->lpwm_instance);
        if (pwm_status != PWM_OK) {
            return BTS7960_ERROR_PWM_FAILED;
        }
    }
    
    /* 设置PWM频率（默认20kHz，≤25kHz限制） */
    uint32_t default_freq = 20000;  /* 20kHz */
    pwm_status = PWM_SetFrequency((PWM_Instance_t)cfg->rpwm_instance, default_freq);
    if (pwm_status != PWM_OK) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    if (cfg->lpwm_instance != cfg->rpwm_instance) {
        pwm_status = PWM_SetFrequency((PWM_Instance_t)cfg->lpwm_instance, default_freq);
        if (pwm_status != PWM_OK) {
            return BTS7960_ERROR_PWM_FAILED;
        }
    }
    
    g_bts7960_pwm_frequency[instance] = default_freq;
    
    /* 设置初始占空比为0 */
    pwm_status = PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, 0.0f);
    if (pwm_status != PWM_OK) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    pwm_status = PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, 0.0f);
    if (pwm_status != PWM_OK) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    /* 初始状态：PWM通道禁用（因为占空比为0） */
    PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
    PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
    
    /* ========== 4. 初始化状态 ========== */
    g_bts7960_initialized[instance] = true;
    g_bts7960_speed[instance] = 0.0f;
    g_bts7960_direction[instance] = BTS7960_DIR_STOP;
    g_bts7960_alarm_callback[instance] = NULL;
    
    return BTS7960_OK;
}

/**
 * @brief BTS7960反初始化
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Deinit(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_OK;  /* 未初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 停止电机 */
    BTS7960_SetDirection(instance, BTS7960_DIR_STOP);
    BTS7960_Disable(instance);
    
    /* 禁用PWM通道 */
    if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
        PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
    }
    if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
        PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
    }
    
    /* 清除回调函数 */
    g_bts7960_alarm_callback[instance] = NULL;
    
    /* 清除初始化标志 */
    g_bts7960_initialized[instance] = false;
    
    return BTS7960_OK;
}

/**
 * @brief 设置电机方向
 * @param[in] instance BTS7960实例索引
 * @param[in] direction 方向（BTS7960_DIR_FORWARD/BACKWARD/STOP/BRAKE）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetDirection(BTS7960_Instance_t instance, BTS7960_Direction_t direction)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    if (direction > BTS7960_DIR_BRAKE) {
        return BTS7960_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 保存当前方向 */
    g_bts7960_direction[instance] = direction;
    
    /* 根据方向设置RPWM和LPWM */
    switch (direction) {
        case BTS7960_DIR_STOP:
            /* 停止：RPWM=0, LPWM=0 */
            if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
            }
            if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
            }
            g_bts7960_speed[instance] = 0.0f;
            break;
            
        case BTS7960_DIR_FORWARD:
            /* 正转：RPWM=速度, LPWM=0 */
            if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
            }
            /* 使用当前保存的速度 */
            if (g_bts7960_speed[instance] > 0.0f) {
                if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                    PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, g_bts7960_speed[instance]);
                    PWM_EnableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
                }
            }
            break;
            
        case BTS7960_DIR_BACKWARD:
            /* 反转：RPWM=0, LPWM=速度 */
            if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
            }
            /* 使用当前保存的速度 */
            if (g_bts7960_speed[instance] > 0.0f) {
                if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                    PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, g_bts7960_speed[instance]);
                    PWM_EnableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
                }
            }
            break;
            
        case BTS7960_DIR_BRAKE:
            /* 制动：RPWM=100%, LPWM=100% */
            if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, 100.0f);
                PWM_EnableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
            }
            if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, 100.0f);
                PWM_EnableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
            }
            g_bts7960_speed[instance] = 0.0f;
            break;
            
        default:
            return BTS7960_ERROR_INVALID_PARAM;
    }
    
    return BTS7960_OK;
}

/**
 * @brief 设置电机速度（PWM占空比）
 * @param[in] instance BTS7960实例索引
 * @param[in] speed 速度（0.0 ~ 100.0，单位：百分比）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetSpeed(BTS7960_Instance_t instance, float speed)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    if (speed < 0.0f || speed > 100.0f) {
        return BTS7960_ERROR_INVALID_PARAM;  /* 速度范围：0.0 ~ 100.0 */
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 保存当前速度 */
    g_bts7960_speed[instance] = speed;
    
    /* 根据当前方向设置PWM占空比 */
    switch (g_bts7960_direction[instance]) {
        case BTS7960_DIR_FORWARD:
            /* 正转：RPWM=速度, LPWM=0 */
            /* 先确保LPWM为0 */
            if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
            }
            /* 设置RPWM */
            if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, speed);
                if (speed > 0.0f) {
                    PWM_EnableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
                } else {
                    PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
                }
            }
            break;
            
        case BTS7960_DIR_BACKWARD:
            /* 反转：RPWM=0, LPWM=速度 */
            /* 先确保RPWM为0 */
            if (cfg->rpwm_instance < PWM_INSTANCE_MAX && cfg->rpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel, 0.0f);
                PWM_DisableChannel((PWM_Instance_t)cfg->rpwm_instance, (PWM_Channel_t)cfg->rpwm_channel);
            }
            /* 设置LPWM */
            if (cfg->lpwm_instance < PWM_INSTANCE_MAX && cfg->lpwm_channel < PWM_CHANNEL_MAX) {
                PWM_SetDutyCycle((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel, speed);
                if (speed > 0.0f) {
                    PWM_EnableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
                } else {
                    PWM_DisableChannel((PWM_Instance_t)cfg->lpwm_instance, (PWM_Channel_t)cfg->lpwm_channel);
                }
            }
            break;
            
        case BTS7960_DIR_STOP:
        case BTS7960_DIR_BRAKE:
            /* 停止或制动时，速度自动设为0 */
            g_bts7960_speed[instance] = 0.0f;
            break;
            
        default:
            break;
    }
    
    return BTS7960_OK;
}

/**
 * @brief 使能BTS7960（设置R_EN和L_EN同时为高电平）
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Enable(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 设置R_EN和L_EN同时为高电平（必须同时使能才能工作） */
    if (cfg->r_en_port != NULL && cfg->r_en_pin != 0) {
        GPIO_SetPin(cfg->r_en_port, cfg->r_en_pin);
    }
    if (cfg->l_en_port != NULL && cfg->l_en_pin != 0) {
        GPIO_SetPin(cfg->l_en_port, cfg->l_en_pin);
    }
    
    return BTS7960_OK;
}

/**
 * @brief 禁用BTS7960（设置R_EN和L_EN同时为低电平）
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_Disable(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 设置R_EN和L_EN同时为低电平 */
    if (cfg->r_en_port != NULL && cfg->r_en_pin != 0) {
        GPIO_ResetPin(cfg->r_en_port, cfg->r_en_pin);
    }
    if (cfg->l_en_port != NULL && cfg->l_en_pin != 0) {
        GPIO_ResetPin(cfg->l_en_port, cfg->l_en_pin);
    }
    
    return BTS7960_OK;
}

/**
 * @brief 检查BTS7960是否已初始化
 * @param[in] instance BTS7960实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t BTS7960_IsInitialized(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    return g_bts7960_initialized[instance] ? 1 : 0;
}

/**
 * @brief 获取电流报警状态
 * @param[in] instance BTS7960实例索引
 * @param[out] alarm_status 报警状态指针
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_GetCurrentAlarmStatus(BTS7960_Instance_t instance, BTS7960_CurrentAlarmStatus_t *alarm_status)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    if (alarm_status == NULL) {
        return BTS7960_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 读取R_IS引脚状态（过流时输出报警信号） */
    if (cfg->r_is_port != NULL && cfg->r_is_pin != 0) {
        alarm_status->r_is_alarm = GPIO_ReadPin(cfg->r_is_port, cfg->r_is_pin) ? 1 : 0;
    } else {
        alarm_status->r_is_alarm = 0;  /* 未配置，返回0（正常） */
    }
    
    /* 读取L_IS引脚状态（过流时输出报警信号） */
    if (cfg->l_is_port != NULL && cfg->l_is_pin != 0) {
        alarm_status->l_is_alarm = GPIO_ReadPin(cfg->l_is_port, cfg->l_is_pin) ? 1 : 0;
    } else {
        alarm_status->l_is_alarm = 0;  /* 未配置，返回0（正常） */
    }
    
    return BTS7960_OK;
}

/**
 * @brief 设置电流报警回调函数
 * @param[in] instance BTS7960实例索引
 * @param[in] callback 回调函数指针（NULL表示禁用回调）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetCurrentAlarmCallback(BTS7960_Instance_t instance, BTS7960_CurrentAlarmCallback_t callback)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 保存回调函数 */
    g_bts7960_alarm_callback[instance] = callback;
    
    return BTS7960_OK;
}

/**
 * @brief 使能电流报警中断
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 * @note 配置R_IS/L_IS为EXTI输入模式，支持中断检测
 * @note 需要先配置EXTI模块，这里只提供接口，实际EXTI配置需要用户手动完成
 */
BTS7960_Status_t BTS7960_EnableCurrentAlarmInterrupt(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：EXTI配置需要用户手动完成，这里只提供接口 */
    /* 用户需要：
     * 1. 配置R_IS/L_IS引脚为EXTI输入模式
     * 2. 配置EXTI中断线
     * 3. 在EXTI中断服务程序中调用BTS7960_GetCurrentAlarmStatus()和回调函数
     */
    
    return BTS7960_OK;
}

/**
 * @brief 禁用电流报警中断
 * @param[in] instance BTS7960实例索引
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_DisableCurrentAlarmInterrupt(BTS7960_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 注意：EXTI禁用需要用户手动完成，这里只提供接口 */
    
    return BTS7960_OK;
}

/**
 * @brief 设置PWM频率
 * @param[in] instance BTS7960实例索引
 * @param[in] frequency 频率（Hz），范围：1Hz ~ 25kHz
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_SetPWMFrequency(BTS7960_Instance_t instance, uint32_t frequency)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    if (frequency == 0 || frequency > 25000) {
        return BTS7960_ERROR_FREQ_OUT_OF_RANGE;  /* BTS7960要求PWM频率≤25kHz */
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    BTS7960_Config_t *cfg = &g_bts7960_configs[instance];
    
    /* 设置RPWM频率 */
    PWM_Status_t pwm_status = PWM_SetFrequency((PWM_Instance_t)cfg->rpwm_instance, frequency);
    if (pwm_status != PWM_OK) {
        return BTS7960_ERROR_PWM_FAILED;
    }
    
    /* 设置LPWM频率（如果与RPWM不是同一个实例） */
    if (cfg->lpwm_instance != cfg->rpwm_instance) {
        pwm_status = PWM_SetFrequency((PWM_Instance_t)cfg->lpwm_instance, frequency);
        if (pwm_status != PWM_OK) {
            return BTS7960_ERROR_PWM_FAILED;
        }
    }
    
    /* 保存频率 */
    g_bts7960_pwm_frequency[instance] = frequency;
    
    return BTS7960_OK;
}

/**
 * @brief 获取当前PWM频率
 * @param[in] instance BTS7960实例索引
 * @param[out] frequency 频率指针（Hz）
 * @return BTS7960_Status_t 错误码
 */
BTS7960_Status_t BTS7960_GetPWMFrequency(BTS7960_Instance_t instance, uint32_t *frequency)
{
    /* ========== 参数校验 ========== */
    if (instance >= BTS7960_INSTANCE_MAX) {
        return BTS7960_ERROR_INVALID_INSTANCE;
    }
    if (frequency == NULL) {
        return BTS7960_ERROR_NULL_PTR;
    }
    
    /* 检查是否已初始化 */
    if (!g_bts7960_initialized[instance]) {
        return BTS7960_ERROR_NOT_INITIALIZED;
    }
    
    /* 返回保存的频率 */
    *frequency = g_bts7960_pwm_frequency[instance];
    
    return BTS7960_OK;
}

#endif /* CONFIG_MODULE_BTS7960_ENABLED */
