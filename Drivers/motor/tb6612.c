/**
 * @file tb6612.c
 * @brief TB6612双路直流电机驱动模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details 基于STM32标准外设库的TB6612驱动，支持正反转、制动、PWM速度控制
 */

/* Include config.h first to get module enable flags */
#include "config.h"

#if CONFIG_MODULE_TB6612_ENABLED

/* Include our header */
#include "tb6612.h"

#include "gpio.h"
#include "timer_pwm.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include <stdbool.h>
#include <stddef.h>

/* 从board.h加载配置 */
static TB6612_Config_t g_tb6612_configs[TB6612_INSTANCE_MAX] __attribute__((unused)) = TB6612_CONFIGS;

/* 初始化标志 */
static bool g_tb6612_initialized[TB6612_INSTANCE_MAX] __attribute__((unused)) = {false, false};

/**
 * @brief 获取GPIO时钟使能值
 * @param[in] port GPIO端口指针
 * @return uint32_t GPIO时钟使能值，失败返回0
 */
static uint32_t TB6612_GetGPIOClock(GPIO_TypeDef *port)
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
 * @brief TB6612初始化
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Init(TB6612_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (g_tb6612_initialized[instance]) {
        return TB6612_OK;  /* 已初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    if (!cfg->enabled) {
        return TB6612_ERROR_INVALID_PARAM;  /* 配置未启用 */
    }
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* ========== 0. 禁用JTAG功能（如果使用PB3/PB4） ========== */
    /* PB3和PB4在STM32F103上默认是JTAG的TDO和NTRST引脚 */
    /* 如果使用PB3或PB4作为GPIO，需要禁用JTAG功能 */
    if ((cfg->ain1_port == GPIOB && (cfg->ain1_pin == GPIO_Pin_3 || cfg->ain1_pin == GPIO_Pin_4)) ||
        (cfg->ain2_port == GPIOB && (cfg->ain2_pin == GPIO_Pin_3 || cfg->ain2_pin == GPIO_Pin_4)) ||
        (cfg->stby_port == GPIOB && (cfg->stby_pin == GPIO_Pin_3 || cfg->stby_pin == GPIO_Pin_4))) {
        /* 使能AFIO时钟 */
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        /* 禁用JTAG，但保留SWD（用于调试） */
        GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    }
    
    /* ========== 1. 配置方向控制引脚（AIN1/AIN2） ========== */
    if (cfg->ain1_port != NULL && cfg->ain1_pin != 0) {
        uint32_t gpio_clock = TB6612_GetGPIOClock(cfg->ain1_port);
        if (gpio_clock == 0) {
            return TB6612_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->ain1_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->ain1_port, &GPIO_InitStructure);
        GPIO_ResetPin(cfg->ain1_port, cfg->ain1_pin);  /* 初始状态：低电平 */
    }
    
    if (cfg->ain2_port != NULL && cfg->ain2_pin != 0) {
        uint32_t gpio_clock = TB6612_GetGPIOClock(cfg->ain2_port);
        if (gpio_clock == 0) {
            return TB6612_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->ain2_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->ain2_port, &GPIO_InitStructure);
        GPIO_ResetPin(cfg->ain2_port, cfg->ain2_pin);  /* 初始状态：低电平 */
    }
    
    /* ========== 2. 配置待机控制引脚（STBY） ========== */
    if (cfg->stby_port != NULL && cfg->stby_pin != 0) {
        uint32_t gpio_clock = TB6612_GetGPIOClock(cfg->stby_port);
        if (gpio_clock == 0) {
            return TB6612_ERROR_GPIO_FAILED;
        }
        RCC_APB2PeriphClockCmd(gpio_clock, ENABLE);
        
        GPIO_InitStructure.GPIO_Pin = cfg->stby_pin;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(cfg->stby_port, &GPIO_InitStructure);
        GPIO_ResetPin(cfg->stby_port, cfg->stby_pin);  /* 初始状态：待机（低电平） */
    }
    
    /* ========== 3. 初始化PWM（如果配置了PWM） ========== */
    /* 检查PWM配置是否有效 */
    if (cfg->pwm_instance >= PWM_INSTANCE_MAX || cfg->pwm_channel >= PWM_CHANNEL_MAX) {
        /* PWM配置无效，返回错误 */
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* 初始化PWM */
    PWM_Status_t pwm_status = PWM_Init((PWM_Instance_t)cfg->pwm_instance);
    if (pwm_status != PWM_OK) {
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* 设置初始占空比为0 */
    pwm_status = PWM_SetDutyCycle((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel, 0.0f);
    if (pwm_status != PWM_OK) {
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* 初始状态：PWM通道禁用（因为占空比为0） */
    pwm_status = PWM_DisableChannel((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel);
    if (pwm_status != PWM_OK) {
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* ========== 4. 保存状态 ========== */
    g_tb6612_initialized[instance] = true;
    
    return TB6612_OK;
}

/**
 * @brief TB6612反初始化
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Deinit(TB6612_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_tb6612_initialized[instance]) {
        return TB6612_OK;  /* 未初始化，直接返回成功 */
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    
    /* 停止电机 */
    TB6612_SetDirection(instance, TB6612_DIR_STOP);
    TB6612_Disable(instance);
    
    /* 禁用PWM通道 */
    if (cfg->pwm_instance < PWM_INSTANCE_MAX && cfg->pwm_channel < PWM_CHANNEL_MAX) {
        PWM_DisableChannel((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel);
    }
    
    /* 清除初始化标志 */
    g_tb6612_initialized[instance] = false;
    
    return TB6612_OK;
}

/**
 * @brief 设置电机方向
 * @param[in] instance TB6612实例索引
 * @param[in] direction 方向（TB6612_DIR_FORWARD/BACKWARD/STOP/BRAKE）
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_SetDirection(TB6612_Instance_t instance, TB6612_Direction_t direction)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    if (direction > TB6612_DIR_BRAKE) {
        return TB6612_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否已初始化 */
    if (!g_tb6612_initialized[instance]) {
        return TB6612_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    
    /* 根据方向设置AIN1和AIN2引脚 */
    switch (direction) {
        case TB6612_DIR_STOP:
            /* 停止：AIN1=0, AIN2=0 */
            if (cfg->ain1_port != NULL && cfg->ain1_pin != 0) {
                GPIO_ResetPin(cfg->ain1_port, cfg->ain1_pin);
            }
            if (cfg->ain2_port != NULL && cfg->ain2_pin != 0) {
                GPIO_ResetPin(cfg->ain2_port, cfg->ain2_pin);
            }
            /* 停止时，将速度设为0（禁用PWM通道） */
            if (cfg->pwm_instance < PWM_INSTANCE_MAX && cfg->pwm_channel < PWM_CHANNEL_MAX) {
                (void)TB6612_SetSpeed(instance, 0.0f);  /* 忽略返回值，因为方向引脚已正确设置 */
            }
            break;
            
        case TB6612_DIR_FORWARD:
            /* 正转：AIN1=1, AIN2=0 */
            if (cfg->ain1_port != NULL && cfg->ain1_pin != 0) {
                GPIO_SetPin(cfg->ain1_port, cfg->ain1_pin);
            }
            if (cfg->ain2_port != NULL && cfg->ain2_pin != 0) {
                GPIO_ResetPin(cfg->ain2_port, cfg->ain2_pin);
            }
            break;
            
        case TB6612_DIR_BACKWARD:
            /* 反转：AIN1=0, AIN2=1 */
            if (cfg->ain1_port != NULL && cfg->ain1_pin != 0) {
                GPIO_ResetPin(cfg->ain1_port, cfg->ain1_pin);
            }
            if (cfg->ain2_port != NULL && cfg->ain2_pin != 0) {
                GPIO_SetPin(cfg->ain2_port, cfg->ain2_pin);
            }
            break;
            
        case TB6612_DIR_BRAKE:
            /* 制动：AIN1=1, AIN2=1 */
            if (cfg->ain1_port != NULL && cfg->ain1_pin != 0) {
                GPIO_SetPin(cfg->ain1_port, cfg->ain1_pin);
            }
            if (cfg->ain2_port != NULL && cfg->ain2_pin != 0) {
                GPIO_SetPin(cfg->ain2_port, cfg->ain2_pin);
            }
            /* 制动时，将速度设为0（禁用PWM通道） */
            if (cfg->pwm_instance < PWM_INSTANCE_MAX && cfg->pwm_channel < PWM_CHANNEL_MAX) {
                (void)TB6612_SetSpeed(instance, 0.0f);  /* 忽略返回值，因为方向引脚已正确设置 */
            }
            break;
            
        default:
            return TB6612_ERROR_INVALID_PARAM;
    }
    
    return TB6612_OK;
}

/**
 * @brief 设置电机速度（PWM占空比）
 * @param[in] instance TB6612实例索引
 * @param[in] speed 速度（0.0 ~ 100.0，单位：百分比）
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_SetSpeed(TB6612_Instance_t instance, float speed)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    if (speed < 0.0f || speed > 100.0f) {
        return TB6612_ERROR_INVALID_PARAM;  /* 速度范围：0.0 ~ 100.0 */
    }
    
    /* 检查是否已初始化 */
    if (!g_tb6612_initialized[instance]) {
        return TB6612_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    
    /* 检查PWM配置是否有效 */
    if (cfg->pwm_instance >= PWM_INSTANCE_MAX || cfg->pwm_channel >= PWM_CHANNEL_MAX) {
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* 设置PWM占空比 */
    PWM_Status_t pwm_status = PWM_SetDutyCycle((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel, speed);
    if (pwm_status != PWM_OK) {
        return TB6612_ERROR_PWM_FAILED;
    }
    
    /* 根据速度值决定使能或禁用PWM通道 */
    if (speed > 0.0f) {
        /* 速度>0时，使能PWM通道 */
        pwm_status = PWM_EnableChannel((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel);
        if (pwm_status != PWM_OK) {
            return TB6612_ERROR_PWM_FAILED;
        }
    } else {
        /* 速度=0时，禁用PWM通道（节省功耗） */
        pwm_status = PWM_DisableChannel((PWM_Instance_t)cfg->pwm_instance, (PWM_Channel_t)cfg->pwm_channel);
        if (pwm_status != PWM_OK) {
            return TB6612_ERROR_PWM_FAILED;
        }
    }
    
    return TB6612_OK;
}

/**
 * @brief 使能TB6612（退出待机模式）
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Enable(TB6612_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_tb6612_initialized[instance]) {
        return TB6612_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    
    /* 设置STBY引脚为高电平（退出待机模式） */
    if (cfg->stby_port != NULL && cfg->stby_pin != 0) {
        GPIO_SetPin(cfg->stby_port, cfg->stby_pin);
    }
    
    return TB6612_OK;
}

/**
 * @brief 禁用TB6612（进入待机模式）
 * @param[in] instance TB6612实例索引
 * @return TB6612_Status_t 错误码
 */
TB6612_Status_t TB6612_Disable(TB6612_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return TB6612_ERROR_INVALID_INSTANCE;
    }
    
    /* 检查是否已初始化 */
    if (!g_tb6612_initialized[instance]) {
        return TB6612_ERROR_NOT_INITIALIZED;
    }
    
    /* 获取配置 */
    TB6612_Config_t *cfg = &g_tb6612_configs[instance];
    
    /* 设置STBY引脚为低电平（进入待机模式） */
    if (cfg->stby_port != NULL && cfg->stby_pin != 0) {
        GPIO_ResetPin(cfg->stby_port, cfg->stby_pin);
    }
    
    return TB6612_OK;
}

/**
 * @brief 检查TB6612是否已初始化
 * @param[in] instance TB6612实例索引
 * @return uint8_t 1-已初始化，0-未初始化
 */
uint8_t TB6612_IsInitialized(TB6612_Instance_t instance)
{
    /* ========== 参数校验 ========== */
    if (instance >= TB6612_INSTANCE_MAX) {
        return 0;  /* 无效实例返回0（未初始化） */
    }
    
    return g_tb6612_initialized[instance] ? 1 : 0;
}

#endif /* CONFIG_MODULE_TB6612_ENABLED */
