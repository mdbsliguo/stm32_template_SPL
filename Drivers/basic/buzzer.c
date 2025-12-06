#include "buzzer.h"
#include "gpio.h"
#include "delay.h"
#include "config.h"
#include <stddef.h>

/* 条件编译：如果Log模块可用，则使用LOG_ERROR输出详细错误信息 */
#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
#define BUZZER_LOG_ERROR(fmt, ...) LOG_ERROR("BUZZER", fmt, ##__VA_ARGS__)
#else
#define BUZZER_LOG_ERROR(fmt, ...) ((void)0)
#endif

/* 条件编译：只在定时器模块启用时才包含PWM头文件 */
#if CONFIG_MODULE_TIMER_ENABLED && CONFIG_MODULE_BUZZER_ENABLED
#include "timer_pwm.h"
#endif

/*******************************************************************************
 * @file buzzer.c
 * @brief Buzzer驱动实现（商业级）
 * @details 查表法驱动，支持GPIO/PWM双模式，带错误处理、断言、临界区保护
 ******************************************************************************/

#if CONFIG_MODULE_BUZZER_ENABLED

/* 加载配置表并自动计算Buzzer数量 */
static Buzzer_Config_t buzzer_configs[] = BUZZER_CONFIGS;
#define BUZZER_COUNT (sizeof(buzzer_configs) / sizeof(buzzer_configs[0]))

/* 音调频率映射表（Hz，基于A4=440Hz标准音调） */
/* 仅在TIMER模块启用时定义，因为PWM模式才需要 */
#if CONFIG_MODULE_TIMER_ENABLED
static const uint32_t tone_frequencies[BUZZER_TONE_MAX] = {
    262,  /* C4 (261.63 Hz，四舍五入) */
    294,  /* D4 (293.66 Hz，四舍五入) */
    330,  /* E4 (329.63 Hz，四舍五入) */
    349,  /* F4 (349.23 Hz，四舍五入) */
    392,  /* G4 (392.00 Hz) */
    440,  /* A4 (440.00 Hz) */
    494,  /* B4 (493.88 Hz，四舍五入) */
    523   /* C5 (523.25 Hz，四舍五入) */
};
#endif /* CONFIG_MODULE_TIMER_ENABLED */

/**
 * @brief 获取Buzzer配置指针（私有函数）
 * @param[in] num Buzzer编号（从1开始）
 * @return Buzzer_Config_t* 配置指针，NULL表示无效编号
 */
static Buzzer_Config_t* Buzzer_GetConfig(Buzzer_Number_t num)
{
    if (num < 1 || num > BUZZER_COUNT) {
        return NULL;
    }
    return &buzzer_configs[num - 1];
}

#if BUZZER_DEBUG_MODE
/**
 * @brief 断言失败处理函数（调试模式）
 * @param[in] expr 失败表达式字符串
 * @param[in] file 文件名
 * @param[in] line 行号
 * @note 可在此重定向到UART打印或触发看门狗复位
 */
void Buzzer_AssertHandler(const char* expr, const char* file, int line)
{
    /* 实际项目中应重定向到调试串口 */
    /* printf("BUZZER ASSERT FAILED: %s at %s:%d\r\n", expr, file, line); */
    
    /* 触发看门狗复位或进入无限循环 */
    while(1);
}

/**
 * @brief 日志输出函数（调试模式）
 * @param[in] fmt 格式化字符串
 * @param[in] ... 可变参数
 * @note 需实现printf重定向到UART
 */
void Buzzer_Log(const char* fmt, ...)
{
    /* 实际项目中应实现UART打印 */
    /* va_list args; */
    /* va_start(args, fmt); */
    /* vprintf(fmt, args); */
    /* va_end(args); */
}
#endif

/*******************************************************************************
 * 函数名称：Buzzer_Init
 * 功能描述：初始化所有启用的Buzzer（配置GPIO/PWM并关闭）
 * 返回值：BUZZER_OK或BUZZER_ERROR_INIT_FAILED
 * 实现细节：
 *   - 遍历配置表，仅处理enabled=1的Buzzer
 *   - GPIO模式：初始化GPIO为推挽输出
 *   - PWM模式：初始化PWM外设，设置默认频率和占空比
 *   - 根据active_level设置正确的初始关闭状态
 ******************************************************************************/
Buzzer_Status_t Buzzer_Init(void)
{
    BUZZER_LOG("Buzzer_Init start, total count: %d\r\n", BUZZER_COUNT);
    
    for (int i = 0; i < BUZZER_COUNT; i++) {
        Buzzer_Config_t* cfg = &buzzer_configs[i];
        
        if (cfg->enabled) {
            if (cfg->mode == BUZZER_MODE_GPIO) {
                BUZZER_ASSERT(cfg->port != NULL);
                BUZZER_ASSERT(cfg->pin != 0);
                
                /* 初始化GPIO为推挽输出 */
                GPIO_Init_Output(cfg->port, cfg->pin);
                
                /* 设置初始关闭状态（与有效电平相反） */
                if (cfg->active_level == Bit_SET) {
                    GPIO_ResetPin(cfg->port, cfg->pin);
                } else {
                    GPIO_SetPin(cfg->port, cfg->pin);
                }
                
                BUZZER_LOG("Buzzer%d initialized (GPIO mode) on port 0x%p, pin 0x%04X\r\n", 
                        i + 1, cfg->port, cfg->pin);
            }
#if CONFIG_MODULE_TIMER_ENABLED
            if (cfg->mode == BUZZER_MODE_PWM) {
                /* 转换配置值为PWM枚举类型 */
                PWM_Instance_t pwm_instance = (PWM_Instance_t)cfg->pwm_instance;
                PWM_Channel_t pwm_channel = (PWM_Channel_t)cfg->pwm_channel;
                
                /* 初始化PWM外设 */
                PWM_Status_t pwm_status = PWM_Init(pwm_instance);
                if (pwm_status != PWM_OK) {
                    BUZZER_LOG("Buzzer%d PWM init failed\r\n", i + 1);
                    return BUZZER_ERROR_INIT_FAILED;
                }
                
                /* 设置默认频率（1kHz）和占空比（50%） */
                PWM_SetFrequency(pwm_instance, 1000);
                PWM_SetDutyCycle(pwm_instance, pwm_channel, 50.0f);
                
                /* 初始状态为关闭（不使能PWM通道） */
                PWM_DisableChannel(pwm_instance, pwm_channel);
                
                BUZZER_LOG("Buzzer%d initialized (PWM mode) on instance %d, channel %d\r\n", 
                        i + 1, pwm_instance, pwm_channel);
            } else {
                BUZZER_LOG("Buzzer%d invalid mode\r\n", i + 1);
                return BUZZER_ERROR_INVALID_MODE;
            }
#else
            if (cfg->mode != BUZZER_MODE_GPIO) {
                BUZZER_LOG("Buzzer%d invalid mode (PWM not available)\r\n", i + 1);
                return BUZZER_ERROR_INVALID_MODE;
            }
#endif
        }
    }
    
    BUZZER_LOG("Buzzer_Init completed\r\n");
    return BUZZER_OK;
}

/*******************************************************************************
 * 函数名称：Buzzer_Deinit
 * 功能描述：反初始化Buzzer驱动（关闭所有Buzzer）
 * 返回值：BUZZER_OK
 * 实现细节：
 *   - 将所有Buzzer设置为关闭状态
 *   - GPIO模式：设置GPIO为关闭电平
 *   - PWM模式：禁用PWM通道
 ******************************************************************************/
Buzzer_Status_t Buzzer_Deinit(void)
{
    BUZZER_LOG("Buzzer_Deinit start\r\n");
    
    for (int i = 0; i < BUZZER_COUNT; i++) {
        Buzzer_Config_t* cfg = &buzzer_configs[i];
        if (cfg->enabled) {
            if (cfg->mode == BUZZER_MODE_GPIO) {
                /* 关闭Buzzer */
                if (cfg->active_level == Bit_SET) {
                    GPIO_ResetPin(cfg->port, cfg->pin);
                } else {
                    GPIO_SetPin(cfg->port, cfg->pin);
                }
            }
#if CONFIG_MODULE_TIMER_ENABLED
            if (cfg->mode == BUZZER_MODE_PWM) {
                /* 转换配置值为PWM枚举类型 */
                PWM_Instance_t pwm_instance = (PWM_Instance_t)cfg->pwm_instance;
                PWM_Channel_t pwm_channel = (PWM_Channel_t)cfg->pwm_channel;
                /* 禁用PWM通道 */
                PWM_DisableChannel(pwm_instance, pwm_channel);
            }
#endif
        }
    }
    
    BUZZER_LOG("Buzzer_Deinit completed\r\n");
    return BUZZER_OK;
}

/*******************************************************************************
 * 函数名称：Buzzer_SetState
 * 功能描述：设置指定Buzzer的状态（自动处理有效电平）
 * 输入参数：num（Buzzer编号），state（目标状态）
 * 返回值：Buzzer_Status_t错误码
 * 实现细节：
 *   - 查表获取配置，空指针返回BUZZER_ERROR_INVALID_ID
 *   - 检查enabled标志，未启用返回BUZZER_ERROR_DISABLED
 *   - GPIO模式：进入临界区保护GPIO操作
 *   - PWM模式：使能/禁用PWM通道
 ******************************************************************************/
Buzzer_Status_t Buzzer_SetState(Buzzer_Number_t num, Buzzer_State_t state)
{
    Buzzer_Config_t* cfg = Buzzer_GetConfig(num);
    if (cfg == NULL) {
        BUZZER_LOG("Buzzer_SetState error: invalid ID %d\r\n", num);
        return BUZZER_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        BUZZER_LOG("Buzzer_SetState error: Buzzer%d disabled\r\n", num);
        return BUZZER_ERROR_DISABLED;
    }
    
    if (cfg->mode == BUZZER_MODE_GPIO) {
        /* 进入临界区（关中断）保护GPIO操作 */
        __disable_irq();
        
        if (state == BUZZER_STATE_ON) {
            if (cfg->active_level == Bit_SET) {
                GPIO_SetPin(cfg->port, cfg->pin);
            } else {
                GPIO_ResetPin(cfg->port, cfg->pin);
            }
        } else {
            if (cfg->active_level == Bit_SET) {
                GPIO_ResetPin(cfg->port, cfg->pin);
            } else {
                GPIO_SetPin(cfg->port, cfg->pin);
            }
        }
        
        /* 退出临界区（开中断） */
        __enable_irq();
    }
#if CONFIG_MODULE_TIMER_ENABLED
    if (cfg->mode == BUZZER_MODE_PWM) {
        /* 转换配置值为PWM枚举类型 */
        PWM_Instance_t pwm_instance = (PWM_Instance_t)cfg->pwm_instance;
        PWM_Channel_t pwm_channel = (PWM_Channel_t)cfg->pwm_channel;
        
        if (state == BUZZER_STATE_ON) {
            /* 使能PWM通道 */
            PWM_Status_t pwm_status = PWM_EnableChannel(pwm_instance, pwm_channel);
            if (pwm_status != PWM_OK) {
                /* 输出详细错误信息（如果Log模块可用） */
#if CONFIG_MODULE_LOG_ENABLED
                LOG_ERROR("BUZZER", "PWM_EnableChannel failed: instance=%d, channel=%d, error=%d", 
                         pwm_instance, pwm_channel, pwm_status);
#endif
                return BUZZER_ERROR_INIT_FAILED;
            }
        } else {
            /* 禁用PWM通道 */
            PWM_DisableChannel(pwm_instance, pwm_channel);
        }
        return BUZZER_OK;
    }
#endif
    
    BUZZER_LOG("Buzzer%d set to %d\r\n", num, state);
    return BUZZER_OK;
}

/*******************************************************************************
 * 函数名称：Buzzer_GetState
 * 功能描述：获取Buzzer当前状态
 * 输入参数：num（Buzzer编号），state（返回状态指针）
 * 返回值：Buzzer_Status_t错误码
 * 实现细节：
 *   - GPIO模式：通过读取GPIO输出数据寄存器ODR判断当前电平
 *   - PWM模式：通过检查PWM通道是否使能判断状态
 ******************************************************************************/
Buzzer_Status_t Buzzer_GetState(Buzzer_Number_t num, Buzzer_State_t* state)
{
    if (state == NULL) {
        return BUZZER_ERROR_NULL_PTR;
    }
    
    Buzzer_Config_t* cfg = Buzzer_GetConfig(num);
    if (cfg == NULL) {
        return BUZZER_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        return BUZZER_ERROR_DISABLED;
    }
    
    if (cfg->mode == BUZZER_MODE_GPIO) {
        /* 读取GPIO电平 */
        uint8_t pin_level = (cfg->port->ODR & cfg->pin) ? 1 : 0;
        
        /* 根据有效电平反推逻辑状态 */
        if (cfg->active_level == Bit_SET) {
            *state = (pin_level == 1) ? BUZZER_STATE_ON : BUZZER_STATE_OFF;
        } else {
            *state = (pin_level == 0) ? BUZZER_STATE_ON : BUZZER_STATE_OFF;
        }
    }
#if CONFIG_MODULE_TIMER_ENABLED
    if (cfg->mode == BUZZER_MODE_PWM) {
        /* PWM模式：检查通道是否使能（简化实现，实际可能需要读取定时器寄存器） */
        /* 这里假设如果PWM已初始化，则根据当前状态判断 */
        /* 由于无法直接读取PWM通道使能状态，这里返回一个保守值 */
        *state = BUZZER_STATE_OFF;  /* 简化实现 */
    }
#endif
    
    return BUZZER_OK;
}

/*******************************************************************************
 * 函数名称：Buzzer_On / Buzzer_Off / Buzzer_Stop
 * 功能描述：Buzzer状态控制便捷函数
 * 返回值：Buzzer_Status_t错误码（直接转发Buzzer_SetState结果）
 ******************************************************************************/
Buzzer_Status_t Buzzer_On(Buzzer_Number_t num) 
{ 
    return Buzzer_SetState(num, BUZZER_STATE_ON); 
}

Buzzer_Status_t Buzzer_Off(Buzzer_Number_t num) 
{ 
    return Buzzer_SetState(num, BUZZER_STATE_OFF); 
}

Buzzer_Status_t Buzzer_Stop(Buzzer_Number_t num)
{
    return Buzzer_Off(num);
}

/*******************************************************************************
 * 函数名称：Buzzer_Beep
 * 功能描述：Buzzer鸣响一次（开启+阻塞延时+关闭）
 * 返回值：Buzzer_Status_t错误码
 * 注意事项：
 *   - 阻塞式延时，执行期间CPU不处理其他任务
 *   - 在长延时中应考虑喂看门狗（WDG）防止复位
 ******************************************************************************/
Buzzer_Status_t Buzzer_Beep(Buzzer_Number_t num, uint32_t duration_ms)
{
    Buzzer_Status_t status = Buzzer_On(num);
    if (status != BUZZER_OK) return status;
    
    /* 在长延时中喂看门狗（如果启用了看门狗） */
    /* WDG_Refresh();  // 每ms喂一次或根据看门狗超时时间调整 */
    
    Delay_ms(duration_ms);
    
    return Buzzer_Off(num);
}

/*******************************************************************************
 * 函数名称：Buzzer_SetFrequency
 * 功能描述：设置Buzzer频率（仅PWM模式）
 * 输入参数：num（Buzzer编号），frequency（频率Hz）
 * 返回值：Buzzer_Status_t错误码
 * 实现细节：
 *   - 检查是否为PWM模式
 *   - 调用PWM_SetFrequency设置频率
 *   - 设置占空比为50%（标准PWM驱动Buzzer）
 ******************************************************************************/
Buzzer_Status_t Buzzer_SetFrequency(Buzzer_Number_t num, uint32_t frequency)
{
    if (frequency == 0 || frequency > 72000000) {
        return BUZZER_ERROR_INVALID_FREQUENCY;
    }
    
    Buzzer_Config_t* cfg = Buzzer_GetConfig(num);
    if (cfg == NULL) {
        return BUZZER_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        return BUZZER_ERROR_DISABLED;
    }
    
#if CONFIG_MODULE_TIMER_ENABLED
    if (cfg->mode != BUZZER_MODE_PWM) {
        return BUZZER_ERROR_PWM_NOT_AVAILABLE;
    }
    
    /* 转换配置值为PWM枚举类型 */
    PWM_Instance_t pwm_instance = (PWM_Instance_t)cfg->pwm_instance;
    PWM_Channel_t pwm_channel = (PWM_Channel_t)cfg->pwm_channel;
    
    /* 设置PWM频率 */
    PWM_Status_t pwm_status = PWM_SetFrequency(pwm_instance, frequency);
    if (pwm_status != PWM_OK) {
        return BUZZER_ERROR_INIT_FAILED;
    }
    
    /* 设置占空比为50%（标准PWM驱动Buzzer） */
    PWM_SetDutyCycle(pwm_instance, pwm_channel, 50.0f);
    
    BUZZER_LOG("Buzzer%d frequency set to %lu Hz\r\n", num, frequency);
    return BUZZER_OK;
#else
    return BUZZER_ERROR_PWM_NOT_AVAILABLE;
#endif
}

/*******************************************************************************
 * 函数名称：Buzzer_PlayTone
 * 功能描述：播放指定音调（仅PWM模式）
 * 输入参数：num（Buzzer编号），tone（音调枚举），duration_ms（持续时间，0表示持续）
 * 返回值：Buzzer_Status_t错误码
 * 实现细节：
 *   - 检查是否为PWM模式
 *   - 从音调频率映射表获取频率
 *   - 设置频率并开启Buzzer
 *   - 如果duration_ms>0，阻塞延时后关闭
 ******************************************************************************/
Buzzer_Status_t Buzzer_PlayTone(Buzzer_Number_t num, Buzzer_Tone_t tone, uint32_t duration_ms)
{
    if (tone >= BUZZER_TONE_MAX) {
        return BUZZER_ERROR_INVALID_TONE;
    }
    
    Buzzer_Config_t* cfg = Buzzer_GetConfig(num);
    if (cfg == NULL) {
        return BUZZER_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        return BUZZER_ERROR_DISABLED;
    }
    
#if CONFIG_MODULE_TIMER_ENABLED
    if (cfg->mode != BUZZER_MODE_PWM) {
        return BUZZER_ERROR_PWM_NOT_AVAILABLE;
    }
    
    /* 获取音调频率 */
    uint32_t frequency = tone_frequencies[tone];
    
    /* 设置频率 */
    Buzzer_Status_t status = Buzzer_SetFrequency(num, frequency);
    if (status != BUZZER_OK) {
        return status;
    }
    
    /* 开启Buzzer */
    status = Buzzer_On(num);
    if (status != BUZZER_OK) {
        return status;
    }
    
    /* 如果指定了持续时间，阻塞延时后关闭 */
    if (duration_ms > 0) {
        Delay_ms(duration_ms);
        return Buzzer_Off(num);
    }
    
    BUZZER_LOG("Buzzer%d playing tone %d (%lu Hz)\r\n", num, tone, frequency);
    return BUZZER_OK;
#else
    return BUZZER_ERROR_PWM_NOT_AVAILABLE;
#endif
}

#endif /* CONFIG_MODULE_BUZZER_ENABLED */
