#include "led.h"
#include "gpio.h"
#include "delay.h"
#include <stddef.h>

/*******************************************************************************
 * @file led.c
 * @brief LED驱动实现（商业级）
 * @details 查表法驱动，带错误处理、断言、临界区保护
 ******************************************************************************/

/* 加载配置表并自动计算LED数量 */
static LED_Config_t led_configs[] = LED_CONFIGS;
#define LED_COUNT (sizeof(led_configs) / sizeof(led_configs[0]))

/**
 * @brief 获取LED配置指针（私有函数）
 * @param[in] num LED编号（从1开始）
 * @return LED_Config_t* 配置指针，NULL表示无效编号
 */
static LED_Config_t* LED_GetConfig(LED_Number_t num)
{
    if (num < 1 || num > LED_COUNT) {
        return NULL;
    }
    return &led_configs[num - 1];
}

#if LED_DEBUG_MODE
/**
 * @brief 断言失败处理函数（调试模式）
 * @param[in] expr 失败表达式字符串
 * @param[in] file 文件名
 * @param[in] line 行号
 * @note 可在此重定向到UART打印或触发看门狗复位
 */
void LED_AssertHandler(const char* expr, const char* file, int line)
{
    /* 实际项目中应重定向到调试串口 */
    /* printf("LED ASSERT FAILED: %s at %s:%d\r\n", expr, file, line); */
    
    /* 触发看门狗复位或进入无限循环 */
    while(1);
}

/**
 * @brief 日志输出函数（调试模式）
 * @param[in] fmt 格式化字符串
 * @param[in] ... 可变参数
 * @note 需实现printf重定向到UART
 */
void LED_Log(const char* fmt, ...)
{
    /* 实际项目中应实现UART打印 */
    /* va_list args; */
    /* va_start(args, fmt); */
    /* vprintf(fmt, args); */
    /* va_end(args); */
}
#endif

/*******************************************************************************
 * 函数名称：LED_Init
 * 功能描述：初始化所有启用的LED（配置GPIO并关闭）
 * 返回值：LED_OK或LED_ERROR_INIT_FAILED
 * 实现细节：
 *   - 遍历配置表，仅处理enabled=1的LED
 *   - 执行参数校验和空指针检查
 *   - 根据active_level设置正确的初始关闭状态
 ******************************************************************************/
LED_Status_t LED_Init(void)
{
    LED_LOG("LED_Init start, total count: %d\r\n", LED_COUNT);
    
    for (int i = 0; i < LED_COUNT; i++) {
        LED_Config_t* cfg = &led_configs[i];
        
        if (cfg->enabled) {
            LED_ASSERT(cfg->port != NULL);
            LED_ASSERT(cfg->pin != 0);
            
            /* 初始化GPIO为推挽输出 */
            GPIO_Init_Output(cfg->port, cfg->pin);
            
            /* 设置初始关闭状态（与有效电平相反） */
            if (cfg->active_level == Bit_SET) {
                GPIO_ResetPin(cfg->port, cfg->pin);
            } else {
                GPIO_SetPin(cfg->port, cfg->pin);
            }
            
            LED_LOG("LED%d initialized on port 0x%p, pin 0x%04X\r\n", 
                    i + 1, cfg->port, cfg->pin);
        }
    }
    
    LED_LOG("LED_Init completed\r\n");
    return LED_OK;
}

/*******************************************************************************
 * 函数名称：LED_Deinit
 * 功能描述：反初始化LED驱动（关闭所有LED）
 * 返回值：LED_OK
 * 实现细节：
 *   - 将所有LED设置为关闭状态
 *   - 不释放GPIO配置（保持引脚状态）
 ******************************************************************************/
LED_Status_t LED_Deinit(void)
{
    LED_LOG("LED_Deinit start\r\n");
    
    for (int i = 0; i < LED_COUNT; i++) {
        LED_Config_t* cfg = &led_configs[i];
        if (cfg->enabled) {
            /* 关闭LED */
            if (cfg->active_level == Bit_SET) {
                GPIO_ResetPin(cfg->port, cfg->pin);
            } else {
                GPIO_SetPin(cfg->port, cfg->pin);
            }
        }
    }
    
    LED_LOG("LED_Deinit completed\r\n");
    return LED_OK;
}

/*******************************************************************************
 * 函数名称：LED_SetState
 * 功能描述：设置LED开关状态（自动处理有效电平）
 * 输入参数：num（LED编号），state（目标状态）
 * 返回值：LED_Status_t错误码
 * 实现细节：
 *   - 查表获取配置，空指针返回LED_ERROR_INVALID_ID
 *   - 检查enabled标志，未启用返回LED_ERROR_DISABLED
 *   - 进入临界区保护GPIO操作
 *   - ON时：输出active_level电平；OFF时：输出相反电平
 ******************************************************************************/
LED_Status_t LED_SetState(LED_Number_t num, LED_State_t state)
{
    LED_Config_t* cfg = LED_GetConfig(num);
    if (cfg == NULL) {
        LED_LOG("LED_SetState error: invalid ID %d\r\n", num);
        return LED_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        LED_LOG("LED_SetState error: LED%d disabled\r\n", num);
        return LED_ERROR_DISABLED;
    }
    
    /* 进入临界区（关中断）保护GPIO操作 */
    __disable_irq();
    
    if (state == LED_STATE_ON) {
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
    
    LED_LOG("LED%d set to %d\r\n", num, state);
    return LED_OK;
}

/*******************************************************************************
 * 函数名称：LED_GetState
 * 功能描述：获取LED当前状态
 * 输入参数：num（LED编号），state（返回状态指针）
 * 返回值：LED_Status_t错误码
 * 实现细节：
 *   - 通过读取GPIO输出数据寄存器ODR判断当前电平
 *   - 根据active_level反推逻辑状态
 ******************************************************************************/
LED_Status_t LED_GetState(LED_Number_t num, LED_State_t* state)
{
    if (state == NULL) {
        return LED_ERROR_NULL_PTR;
    }
    
    LED_Config_t* cfg = LED_GetConfig(num);
    if (cfg == NULL) {
        return LED_ERROR_INVALID_ID;
    }
    
    if (!cfg->enabled) {
        return LED_ERROR_DISABLED;
    }
    
    /* 读取GPIO电平 */
    uint8_t pin_level = (cfg->port->ODR & cfg->pin) ? 1 : 0;
    
    /* 根据有效电平反推逻辑状态 */
    if (cfg->active_level == Bit_SET) {
        *state = (pin_level == 1) ? LED_STATE_ON : LED_STATE_OFF;
    } else {
        *state = (pin_level == 0) ? LED_STATE_ON : LED_STATE_OFF;
    }
    
    return LED_OK;
}

/*******************************************************************************
 * 函数名称：LED_On / LED_Off / LED_Toggle
 * 功能描述：LED状态控制便捷函数
 * 返回值：LED_Status_t错误码（直接转发LED_SetState结果）
 ******************************************************************************/
LED_Status_t LED_On(LED_Number_t num) { return LED_SetState(num, LED_STATE_ON); }

LED_Status_t LED_Off(LED_Number_t num) { return LED_SetState(num, LED_STATE_OFF); }

LED_Status_t LED_Toggle(LED_Number_t num)
{
    LED_Config_t* cfg = LED_GetConfig(num);
    if (cfg == NULL) return LED_ERROR_INVALID_ID;
    if (!cfg->enabled) return LED_ERROR_DISABLED;
    
    __disable_irq();
    GPIO_TogglePin(cfg->port, cfg->pin);
    __enable_irq();
    
    return LED_OK;
}

/*******************************************************************************
 * 函数名称：LED_Blink
 * 功能描述：LED闪烁一次（翻转+阻塞延时）
 * 返回值：LED_Status_t错误码
 * 注意事项：
 *   - 阻塞式延时，执行期间CPU不处理其他任务
 *   - 在长延时中应考虑喂看门狗（WDG）防止复位
 ******************************************************************************/
LED_Status_t LED_Blink(LED_Number_t num, uint32_t delay_ms)
{
    LED_Status_t status = LED_Toggle(num);
    if (status != LED_OK) return status;
    
    /* 在长延时中喂看门狗（如果启用了看门狗） */
    /* WDG_Refresh();  // 每ms喂一次或根据看门狗超时时间调整 */
    
    Delay_ms(delay_ms);
    return LED_OK;
}
