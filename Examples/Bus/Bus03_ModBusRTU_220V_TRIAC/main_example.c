/**
 * @file main_example.c
 * @brief Bus03 - ModBusRTU可控硅调压器控制示例
 * @details 通过UART2（RS485）使用ModBusRTU协议控制JAF22013型可控硅数字调压器，通过编码器控制输出百分比
 * @version 1.0.0
 * @date 2024-01-01
 *
 * 功能说明：
 * - 使用ModBusRTU协议控制JAF22013型可控硅数字调压器
 * - 通过编码器（PB4/PB5，TIM3部分重映射）控制输出百分比
 * - 输出百分比范围：0%, 20%~80%, 100%（按5%步进：20%, 25%, 30%, ..., 80%, 100%）
 * - 在OLED上显示当前输出百分比和设备状态
 * - 读取和显示设备参数（地址、波特率、最小输出档位等）
 *
 * 模块依赖：
 * - system_init：系统初始化模块
 * - uart：UART驱动模块
 * - debug：Debug模块（UART输出功能）
 * - log：日志模块
 * - error_handler：错误处理模块
 * - modbus_rtu：ModBusRTU协议栈模块
 * - oled_ssd1306：OLED显示模块
 * - timer_encoder：定时器编码器模块
 * - delay：延时模块
 *
 * 硬件要求：
 * - UART1：PA9(TX), PA10(RX)，115200波特率（用于Debug输出）
 * - UART2：PA2(TX), PA3(RX)，9600波特率（用于ModBusRTU通信，连接RS485）
 * - OLED：PB8(SCL), PB9(SDA)，软件I2C
 * - 编码器：PB4(TIM3_CH1), PB5(TIM3_CH2)，TIM3部分重映射
 * - RS485模块：连接UART2，自动方向控制
 * - JAF22013型可控硅数字调压器：默认地址1，9600波特率
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
#include "modbus_rtu.h"
#include "timer_encoder.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 可控硅调压器配置 ==================== */

#define TRIAC_SLAVE_ADDRESS     1       /**< 可控硅调压器默认地址 */
#define TRIAC_BAUDRATE          9600     /**< 可控硅调压器默认波特率 */

/* 寄存器地址定义（根据开发资料） */
#define TRIAC_REG_MIN_OUTPUT        0x0000  /**< 最小输出档位（下限值E） */
#define TRIAC_REG_BAUDRATE          0x0001  /**< 波特率设置 */
#define TRIAC_REG_SLAVE_ADDR        0x0002  /**< MODBUS地址 */
#define TRIAC_REG_OUTPUT_PERCENT    0x0003  /**< 输出值（百分比） */
#define TRIAC_REG_POWER_ON_OUTPUT   0x0004  /**< 上电默认输出 */
#define TRIAC_REG_CALIBRATION       0x0005  /**< 输出校准 */
#define TRIAC_REG_RESET             0x0020  /**< 复位重启（写入0x00AA触发复位） */

/* 输出百分比有效值定义（根据开发资料） */
#define TRIAC_OUTPUT_0_PERCENT      0x0000  /**< 0% */
#define TRIAC_OUTPUT_MIN_PERCENT    0x0014  /**< 20%（最小值，对应下限值E） */
#define TRIAC_OUTPUT_MAX_PERCENT    0x0050  /**< 80%（设备支持的最大值，20%~100%之间） */
#define TRIAC_OUTPUT_100_PERCENT    0x0064  /**< 100% */

/* ==================== 编码器配置 ==================== */

#define ENCODER_INSTANCE        ENCODER_INSTANCE_TIM3  /**< 编码器实例（TIM3） */
#define ENCODER_MODE            ENCODER_MODE_TI12     /**< 编码器模式（4倍频） */

/* 编码器计数值到百分比的映射参数 */
#define ENCODER_STEP_SIZE       5      /**< 步进大小（5%） */
#define ENCODER_MAX_COUNT       400    /**< 编码器最大计数值（对应100%） */
#define ENCODER_COUNT_PER_5_PERCENT  20     /**< 每5%步进对应的编码器计数值 */

/* ==================== 全局变量 ==================== */

static uint8_t g_triac_address = TRIAC_SLAVE_ADDRESS;  /**< 可控硅调压器地址 */
static uint16_t g_current_output = 0;                  /**< 当前输出百分比（寄存器值） */
static uint16_t g_min_output = TRIAC_OUTPUT_MIN_PERCENT;  /**< 最小输出档位（下限值E） */
static int32_t g_encoder_count = 0;                    /**< 编码器当前计数值 */
static int32_t g_last_encoder_count = 0;                /**< 编码器上次计数值 */
static uint32_t g_last_update_time = 0;                /**< 上次更新时间（用于防抖） */

/* ==================== 私有函数 ==================== */

/**
 * @brief 读取可控硅调压器输出百分比
 * @return ModBusRTU_Status_t 状态码
 */
static ModBusRTU_Status_t ReadTriacOutput(void)
{
    ModBusRTU_Status_t status;
    
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_triac_address,
                                           TRIAC_REG_OUTPUT_PERCENT, 1, &g_current_output, 1000);
    
    if (status == ModBusRTU_OK) {
        /* 读取成功，不输出日志（减少串口输出） */
    } else {
        LOG_ERROR("TRIAC", "读取输出百分比失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
    
    return status;
}

/**
 * @brief 读取可控硅调压器最小输出档位
 * @return ModBusRTU_Status_t 状态码
 */
static ModBusRTU_Status_t ReadTriacMinOutput(void)
{
    ModBusRTU_Status_t status;
    
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_2, g_triac_address,
                                           TRIAC_REG_MIN_OUTPUT, 1, &g_min_output, 1000);
    
    if (status == ModBusRTU_OK) {
        LOG_INFO("TRIAC", "最小输出档位: %d%%", g_min_output);
    } else {
        LOG_ERROR("TRIAC", "读取最小输出档位失败: %d", status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
    
    return status;
}

/**
 * @brief 设置可控硅调压器输出百分比
 * @param[in] output_value 输出值（寄存器值：0x0000, 20%~100%按5%步进）
 * @return ModBusRTU_Status_t 状态码
 */
static ModBusRTU_Status_t SetTriacOutput(uint16_t output_value)
{
    ModBusRTU_Status_t status;
    uint16_t remainder;
    
    /* 参数校验：输出值必须是有效值 */
    if (output_value == TRIAC_OUTPUT_0_PERCENT) {
        /* 0%是有效的 */
    } else if (output_value == TRIAC_OUTPUT_100_PERCENT) {
        /* 100%是有效的 */
    } else if (output_value >= TRIAC_OUTPUT_MIN_PERCENT && output_value <= 100) {
        /* 20%~100%之间，检查是否是设备支持的值 */
        if (output_value == 100) {
            /* 100%是有效的，直接使用 */
        } else if (output_value > TRIAC_OUTPUT_MAX_PERCENT && output_value < 100) {
            /* 85%~95%不在设备支持范围内，自动调整到80%或100% */
            if (output_value >= 90) {
                /* 90%及以上，跳到100% */
                output_value = TRIAC_OUTPUT_100_PERCENT;
                LOG_WARN("TRIAC", "输出值 %d%% 不在设备支持范围，自动调整为100%%", output_value);
            } else {
                /* 85%，跳到80% */
                output_value = TRIAC_OUTPUT_MAX_PERCENT;  /* 80% */
                LOG_WARN("TRIAC", "输出值 %d%% 不在设备支持范围，自动调整为80%%", output_value);
            }
        } else {
            /* 20%~80%之间，检查是否是5%的倍数 */
            remainder = (output_value - 20) % ENCODER_STEP_SIZE;
            if (remainder != 0) {
                /* 不是5%的倍数，自动调整到最近的5%步进值 */
                uint16_t step_index = (output_value - 20 + ENCODER_STEP_SIZE / 2) / ENCODER_STEP_SIZE;
                output_value = 20 + step_index * ENCODER_STEP_SIZE;
                if (output_value > TRIAC_OUTPUT_MAX_PERCENT) {
                    output_value = TRIAC_OUTPUT_MAX_PERCENT;  /* 限制为80% */
                }
                LOG_WARN("TRIAC", "输出值自动调整为5%步进值: %d%%", output_value);
            }
        }
    } else {
        LOG_ERROR("TRIAC", "无效的输出值: 0x%04X (%d%%)，允许范围: 0, 20~100(5%步进)", 
                output_value, output_value);
        return ModBusRTU_ERROR_INVALID_PARAM;
    }
    
    /* 检查是否低于最小输出档位（但允许0%和100%） */
    if (output_value != TRIAC_OUTPUT_0_PERCENT &&
        output_value != TRIAC_OUTPUT_100_PERCENT &&
        output_value < g_min_output) {
        LOG_WARN("TRIAC", "输出值 %d%% 低于最小输出档位 %d%%，自动调整为 %d%%", 
                output_value, g_min_output, g_min_output);
        output_value = g_min_output;
    }
    
    status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_2, g_triac_address,
                                           TRIAC_REG_OUTPUT_PERCENT, output_value, 500);
    
    if (status == ModBusRTU_OK) {
        g_current_output = output_value;
        LOG_INFO("TRIAC", "设置输出百分比成功: 0x%04X (%d%%)", output_value, output_value);
    } else {
        LOG_ERROR("TRIAC", "设置输出百分比失败: 0x%04X (%d%%)，错误码: %d", output_value, output_value, status);
        ErrorHandler_Handle(status, "ModBusRTU");
    }
    
    return status;
}

/**
 * @brief 将编码器计数值转换为输出百分比（寄存器值），基于当前输出值和旋转方向
 * @param[in] encoder_count 编码器计数值
 * @param[in] current_output 当前输出百分比（寄存器值）
 * @return uint16_t 输出百分比（寄存器值）
 * @note 步进优化：0% -> 20% -> ±5% -> 100%，扭一下就要有效果
 */
static uint16_t EncoderCountToOutput(int32_t encoder_count, uint16_t current_output)
{
    int32_t encoder_delta;
    uint16_t output_value;
    uint16_t current_percent;
    int16_t direction;  /* 1=顺时针（增加），-1=逆时针（减少） */
    
    /* 计算编码器计数值变化量 */
    encoder_delta = encoder_count - g_last_encoder_count;
    
    /* 确定旋转方向 */
    if (encoder_delta > 0) {
        direction = 1;  /* 顺时针（增加） */
    } else if (encoder_delta < 0) {
        direction = -1;  /* 逆时针（减少） */
    } else {
        /* 没有变化，返回当前值 */
        return current_output;
    }
    
    /* 将当前输出值转换为百分比 */
    if (current_output == TRIAC_OUTPUT_0_PERCENT) {
        current_percent = 0;
    } else if (current_output == TRIAC_OUTPUT_100_PERCENT) {
        current_percent = 100;
    } else {
        current_percent = current_output;  /* 20%~80%直接使用 */
    }
    
    /* 根据当前输出值和旋转方向决定下一个输出值 */
    /* 注意：设备只支持 0%, 20%~80%, 100%，所以需要映射到这些值 */
    if (current_percent == 0) {
        /* 当前是0% */
        if (direction > 0) {
            /* 顺时针：跳到20% */
            output_value = TRIAC_OUTPUT_MIN_PERCENT;  /* 20% */
        } else {
            /* 逆时针：保持0%（已经是0%） */
            output_value = TRIAC_OUTPUT_0_PERCENT;
        }
    } else if (current_percent == 100) {
        /* 当前是100% */
        if (direction > 0) {
            /* 顺时针：保持100%（已经是100%） */
            output_value = TRIAC_OUTPUT_100_PERCENT;
        } else {
            /* 逆时针：跳到80%（设备支持的最大值） */
            output_value = TRIAC_OUTPUT_MAX_PERCENT;  /* 80% */
        }
    } else if (current_percent == 20) {
        /* 当前是20% */
        if (direction > 0) {
            /* 顺时针：跳到25% */
            output_value = 25;
        } else {
            /* 逆时针：跳到0% */
            output_value = TRIAC_OUTPUT_0_PERCENT;
        }
    } else if (current_percent >= 80) {
        /* 当前是80%或以上（但设备最大支持80%，除非是100%） */
        if (direction > 0) {
            /* 顺时针：跳到100% */
            output_value = TRIAC_OUTPUT_100_PERCENT;
        } else {
            /* 逆时针：减少5%，但不超过80% */
            if (current_percent > 25) {
                output_value = current_percent - ENCODER_STEP_SIZE;
                /* 确保不超过80% */
                if (output_value > 80) {
                    output_value = TRIAC_OUTPUT_MAX_PERCENT;  /* 80% */
                }
            } else {
                output_value = TRIAC_OUTPUT_MIN_PERCENT;  /* 20% */
            }
        }
    } else {
        /* 当前是25%~75%之间，按5%步进 */
        if (direction > 0) {
            /* 顺时针：增加5% */
            output_value = current_percent + ENCODER_STEP_SIZE;
            /* 如果超过80%，跳到100% */
            if (output_value > 80) {
                output_value = TRIAC_OUTPUT_100_PERCENT;
            }
        } else {
            /* 逆时针：减少5% */
            if (current_percent > ENCODER_STEP_SIZE) {
                output_value = current_percent - ENCODER_STEP_SIZE;
            } else {
                output_value = TRIAC_OUTPUT_MIN_PERCENT;  /* 限制为20% */
            }
        }
    }
    
    return output_value;
}

/**
 * @brief 将输出百分比（寄存器值）转换为编码器计数值
 * @param[in] output_value 输出百分比（寄存器值）
 * @return int32_t 编码器计数值
 * @note 步进优化：0% -> 20% -> ±5% -> 100%，使用简单的线性映射
 */
static int32_t OutputToEncoderCount(uint16_t output_value)
{
    uint16_t output_percent;
    int32_t encoder_count;
    uint16_t step_index;
    
    /* 将寄存器值转换为百分比 */
    if (output_value == TRIAC_OUTPUT_0_PERCENT) {
        /* 0%：映射到编码器计数值0（或保持当前负数） */
        encoder_count = 0;
    } else if (output_value == TRIAC_OUTPUT_100_PERCENT) {
        /* 100%：映射到编码器计数值400 */
        encoder_count = ENCODER_MAX_COUNT;
    } else {
        /* 20%~95%：映射到编码器计数值，按5%步进 */
        output_percent = output_value;  /* 20%~95%直接使用 */
        
        /* 计算步进索引：从20%开始，每5%一步 */
        step_index = (output_percent - 20) / ENCODER_STEP_SIZE;
        
        /* 将步进索引映射到编码器计数值：每5%对应约20个计数值 */
        encoder_count = 20 + step_index * ENCODER_COUNT_PER_5_PERCENT;
        
        /* 确保不超过400 */
        if (encoder_count > ENCODER_MAX_COUNT) {
            encoder_count = ENCODER_MAX_COUNT;
        }
    }
    
    /* 边界检查：确保编码器计数值在有效范围内（但允许负数） */
    if (encoder_count > ENCODER_MAX_COUNT) {
        encoder_count = ENCODER_MAX_COUNT;
    }
    
    return encoder_count;
}

/**
 * @brief 在OLED上显示状态信息
 */
static void DisplayStatusOnOLED(void)
{
    char buffer[32];
    uint16_t output_percent;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Bus03 ModBusRTU");
    
    /* 显示设备地址 */
    snprintf(buffer, sizeof(buffer), "Addr: %d", g_triac_address);
    OLED_ShowString(2, 1, buffer);
    
    /* 显示输出百分比 */
    if (g_current_output == TRIAC_OUTPUT_0_PERCENT) {
        output_percent = 0;
    } else if (g_current_output == TRIAC_OUTPUT_100_PERCENT) {
        output_percent = 100;
    } else {
        output_percent = g_current_output;  /* 20%~99%直接使用（按5%步进） */
    }
    
    snprintf(buffer, sizeof(buffer), "Output: %d%%", output_percent);
    OLED_ShowString(3, 1, buffer);
    
    /* 显示编码器计数值 */
    snprintf(buffer, sizeof(buffer), "Enc: %ld", g_encoder_count);
    OLED_ShowString(4, 1, buffer);
}

/**
 * @brief 处理编码器变化
 * @details 读取编码器计数值，转换为输出百分比，并设置到设备
 */
static void ProcessEncoderChange(void)
{
    ENCODER_Status_t encoder_status;
    uint16_t target_output;
    ModBusRTU_Status_t modbus_status;
    uint32_t current_time;
    
    /* 读取编码器计数值 */
    encoder_status = ENCODER_ReadCount(ENCODER_INSTANCE, &g_encoder_count);
    if (encoder_status != ENCODER_OK) {
        LOG_ERROR("ENCODER", "读取编码器计数值失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        return;
    }
    
    /* 检查计数值是否变化 */
    if (g_encoder_count == g_last_encoder_count) {
        return;  /* 没有变化，直接返回 */
    }
    
    /* 防抖：检查时间间隔（至少100ms才更新一次） */
    current_time = Delay_GetTick();
    if (Delay_GetElapsed(current_time, g_last_update_time) < 100) {
        return;  /* 时间间隔太短，忽略 */
    }
    
    /* 转换为输出百分比（基于当前输出值和旋转方向） */
    target_output = EncoderCountToOutput(g_encoder_count, g_current_output);
    
    /* 如果目标输出与当前输出相同，不需要更新 */
    if (target_output == g_current_output) {
        g_last_encoder_count = g_encoder_count;
        return;
    }
    
    /* 设置输出百分比 */
    modbus_status = SetTriacOutput(target_output);
    if (modbus_status == ModBusRTU_OK) {
        g_last_encoder_count = g_encoder_count;
        g_last_update_time = current_time;
        
        /* 更新OLED显示 */
        DisplayStatusOnOLED();
    }
}

/**
 * @brief 初始化编码器
 * @return error_code_t 错误码
 */
static error_code_t InitEncoder(void)
{
    ENCODER_Status_t encoder_status;
    
    /* 设置TIM3部分重映射（PB4/PB5） */
    ENCODER_SetTIM3Remap(true, false);  /* 部分重映射：PB4/PB5 */
    
    /* 初始化编码器 */
    encoder_status = ENCODER_Init(ENCODER_INSTANCE, ENCODER_MODE);
    if (encoder_status != ENCODER_OK) {
        LOG_ERROR("ENCODER", "编码器初始化失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        return encoder_status;
    }
    
    /* 启动编码器 */
    encoder_status = ENCODER_Start(ENCODER_INSTANCE);
    if (encoder_status != ENCODER_OK) {
        LOG_ERROR("ENCODER", "编码器启动失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        return encoder_status;
    }
    
    /* 读取当前计数值并同步到输出值 */
    encoder_status = ENCODER_ReadCount(ENCODER_INSTANCE, &g_encoder_count);
    if (encoder_status == ENCODER_OK) {
        g_last_encoder_count = g_encoder_count;
        
        /* 将当前输出值同步到编码器计数值 */
        /* 特殊处理：如果设备输出是0%，且编码器计数值已经是负数，允许保持负数（不强制同步为0） */
        if (g_current_output == TRIAC_OUTPUT_0_PERCENT && g_encoder_count < 0) {
            /* 设备输出是0%，编码器计数值是负数，这是允许的，不强制同步 */
        } else {
            int32_t sync_count = OutputToEncoderCount(g_current_output);
            /* 边界检查：确保编码器计数值在有效范围内（但允许负数） */
            if (sync_count > ENCODER_MAX_COUNT) {
                sync_count = ENCODER_MAX_COUNT;
            }
            ENCODER_SetCount(ENCODER_INSTANCE, sync_count);
            g_encoder_count = sync_count;
            g_last_encoder_count = sync_count;
        }
        
        LOG_INFO("ENCODER", "编码器初始化成功，当前计数值: %ld", g_encoder_count);
    } else {
        LOG_ERROR("ENCODER", "读取编码器计数值失败: %d", encoder_status);
    }
    
    return ERROR_OK;
}

/* ==================== 主函数 ==================== */

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    ModBusRTU_Status_t modbus_status;
    error_code_t encoder_status;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    /* 初始化UART1（用于Debug输出） */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* 初始化UART2（用于ModBusRTU通信） */
    uart_status = UART_Init(UART_INSTANCE_2);
    if (uart_status != UART_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤3：Debug模块初始化（UART模式） ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_DEBUG;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK) {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Bus03 ModBusRTU可控硅调压器控制示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "UART2已初始化: PA2(TX), PA3(RX), 9600");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK) {
        OLED_Clear();
        OLED_ShowString(1, 1, "Bus03 ModBusRTU");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    } else {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    Delay_ms(1000);
    
    /* ========== 步骤8：初始化编码器 ========== */
    encoder_status = InitEncoder();
    if (encoder_status != ERROR_OK) {
        LOG_ERROR("MAIN", "编码器初始化失败，程序无法继续");
        DisplayStatusOnOLED();
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤9：读取可控硅调压器初始状态 ========== */
    LOG_INFO("MAIN", "读取可控硅调压器初始状态...");
    
    /* 读取最小输出档位 */
    modbus_status = ReadTriacMinOutput();
    if (modbus_status != ModBusRTU_OK) {
        LOG_WARN("MAIN", "读取最小输出档位失败，使用默认值20%%");
        g_min_output = TRIAC_OUTPUT_MIN_PERCENT;
    }
    
    Delay_ms(200);
    
    /* 读取当前输出百分比 */
    modbus_status = ReadTriacOutput();
    if (modbus_status == ModBusRTU_OK) {
        LOG_INFO("MAIN", "当前输出百分比: 0x%04X (%d%%)", 
                g_current_output, 
                (g_current_output == TRIAC_OUTPUT_0_PERCENT) ? 0 :
                (g_current_output == TRIAC_OUTPUT_100_PERCENT) ? 100 : g_current_output);
        
        /* 同步编码器计数值到当前输出值 */
        /* 特殊处理：如果设备输出是0%，且编码器计数值已经是负数，允许保持负数（不强制同步为0） */
        if (g_current_output == TRIAC_OUTPUT_0_PERCENT && g_encoder_count < 0) {
            /* 设备输出是0%，编码器计数值是负数，这是允许的，不强制同步 */
        } else {
            int32_t sync_count = OutputToEncoderCount(g_current_output);
            /* 边界检查：确保编码器计数值在有效范围内（但允许负数） */
            if (sync_count > ENCODER_MAX_COUNT) {
                sync_count = ENCODER_MAX_COUNT;
            }
            ENCODER_SetCount(ENCODER_INSTANCE, sync_count);
            g_encoder_count = sync_count;
            g_last_encoder_count = sync_count;
        }
    } else {
        LOG_WARN("MAIN", "读取当前输出百分比失败，使用默认值0%%");
        g_current_output = TRIAC_OUTPUT_0_PERCENT;
    }
    
    /* 更新OLED显示 */
    DisplayStatusOnOLED();
    
    LOG_INFO("MAIN", "初始化完成，开始主循环...");
    LOG_INFO("MAIN", "旋转编码器可控制输出百分比（0%%, 20%%~80%%, 100%%）");
    
    /* ========== 步骤10：主循环 ========== */
    while (1) {
        /* 处理编码器变化 */
        ProcessEncoderChange();
        
        /* 定期读取设备状态（每2秒读取一次） */
        static uint32_t last_read_time = 0;
        uint32_t current_time = Delay_GetTick();
        if (Delay_GetElapsed(current_time, last_read_time) >= 2000) {
            /* 读取当前输出百分比（用于同步，防止设备被其他方式修改） */
            modbus_status = ReadTriacOutput();
            if (modbus_status == ModBusRTU_OK) {
                /* 如果设备输出值与编码器不同步，更新编码器 */
                /* 特殊处理：如果设备输出是0%，且编码器计数值是负数，允许保持负数（不强制同步为0） */
                if (g_current_output == TRIAC_OUTPUT_0_PERCENT && g_encoder_count < 0) {
                    /* 设备输出是0%，编码器计数值是负数，这是允许的（用户继续逆时针旋转），不强制同步 */
                    /* 只有当编码器计数值变为正数时，才需要同步 */
                } else {
                    int32_t expected_count = OutputToEncoderCount(g_current_output);
                    /* 边界检查：确保编码器计数值在有效范围内（但允许负数） */
                    if (expected_count > ENCODER_MAX_COUNT) {
                        expected_count = ENCODER_MAX_COUNT;
                    }
                    /* 只有当期望值和当前值不同，且不是"0%输出+负数编码器"的情况时才同步 */
                    if (expected_count != g_encoder_count) {
                        LOG_INFO("TRIAC", "检测到设备输出值变化，同步编码器: %ld -> %ld", 
                                g_encoder_count, expected_count);
                        ENCODER_SetCount(ENCODER_INSTANCE, expected_count);
                        g_encoder_count = expected_count;
                        g_last_encoder_count = expected_count;
                        DisplayStatusOnOLED();
                    }
                }
            }
            
            last_read_time = current_time;
        }
        
        /* 短暂延时，避免CPU占用过高 */
        Delay_ms(10);
    }
}

