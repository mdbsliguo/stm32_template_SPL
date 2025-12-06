/**
 * @file main_example.c
 * @brief Timer05 - 编码器接口示例
 * @example Examples/Timer/Timer05_EncoderInterface/main_example.c
 * @status 已调试完成
 * @details 使用定时器编码器接口模式实现旋转编码器的计数功能，替代EXTI方式
 * 
 * 硬件要求：
 * - 旋转编码器通道A连接到PB6（TIM4 CH1）
 * - 旋转编码器通道B连接到PB7（TIM4 CH2）
 * - LED1连接到PA1（状态指示，计数时闪烁）
 * - OLED显示屏（可选，用于显示计数）
 * - UART1：PA9(TX), PA10(RX)，115200波特率
 * 
 * 硬件配置：
 * 在 Examples/Timer/Timer05_EncoderInterface/board.h 中配置：
 * - LED_CONFIGS: LED配置（PA1）
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8/9，可选）
 * - UART_CONFIGS: UART配置（PA9/10，115200）
 * 
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Timer/Timer05_EncoderInterface/Examples.uvprojx
 * 2. 根据实际硬件修改 board.h 中的配置
 * 3. 编译运行（F7编译，F8下载）
 * 
 * 注意：本案例是独立工程，无需复制文件
 * 
 * 硬件连接说明：
 * - 编码器接口模式只能使用定时器的CH1和CH2
 * - PB0/PB1是TIM3的CH3/CH4，无法用于编码器接口模式
 * - 本案例使用TIM4（PB6/PB7），是最接近PB0/PB1的引脚选项
 * - 编码器接口模式支持4倍频（ENCODER_MODE_TI12）
 * - 4倍频特性：一个物理步进 = 4个计数，需要除以4得到实际步数
 */

#include "system_init.h"
#include "timer_encoder.h"
#include "led.h"
#include "oled_ssd1306.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "board.h"
#include "gpio.h"
#include "stm32f10x.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    OLED_Status_t oled_status;
    ENCODER_Status_t encoder_status;
    int32_t encoder_count = 0;
    int32_t last_count = 0;
    ENCODER_Direction_t direction;
    char direction_str[4] = "--";
    
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
    log_config.level = LOG_LEVEL_DEBUG;        /* 日志级别：DEBUG（显示所有日志） */
    log_config.enable_timestamp = 0;          /* 禁用时间戳（简化输出） */
    log_config.enable_module = 1;              /* 启用模块名显示 */
    log_config.enable_color = 0;               /* 禁用颜色输出（串口助手可能不支持） */
    
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
        /* Log初始化失败，但可以继续运行（使用UART直接输出） */
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== Timer05 编码器接口示例 ===");
    LOG_INFO("MAIN", "系统初始化完成");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    
    /* ========== 步骤7：初始化OLED ========== */
    oled_status = OLED_Init();
    if (oled_status == OLED_OK)
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "Timer05 Demo");
        OLED_ShowString(2, 1, "Encoder IF");
        OLED_ShowString(3, 1, "Counter: 0");
        OLED_ShowString(4, 1, "Dir: --");
        LOG_INFO("MAIN", "OLED已初始化并显示");
    }
    else
    {
        LOG_ERROR("MAIN", "OLED初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    
    /* ========== 步骤8：初始化编码器接口 ========== */
    /* 注意：编码器接口模式只能使用定时器的CH1和CH2 */
    /* PB0/PB1是TIM3的CH3/CH4，无法用于编码器接口模式 */
    /* 可用选项：
     * 1. TIM4：PB6/PB7（CH1/CH2）- 推荐，最接近PB0/PB1
     * 2. TIM3部分重映射：PB4/PB5（CH1/CH2）
     * 3. TIM3完全重映射：PC6/PC7（CH1/CH2）
     * 4. TIM3默认：PA6/PA7（CH1/CH2）
     */
    
    /* 方案1：使用TIM4（PB6/PB7）- 推荐，最接近PB0/PB1 */
    /* 编码器模式说明：
     * - ENCODER_MODE_TI12：4倍频模式，每转一步计数值变化4（精度最高）
     * - ENCODER_MODE_TI1：2倍频模式，每转一步计数值变化2（推荐用于1:1计数）
     * - ENCODER_MODE_TI2：2倍频模式，每转一步计数值变化2（推荐用于1:1计数）
     */
    /* 使用4倍频模式（TI12），与参考代码一致 */
    /* 编码器接口模式在双边沿计数，旋转一次会计数4次（4倍频） */
    /* 通过软件滤波，将4次计数合并为1次，实现1:1计数 */
    encoder_status = ENCODER_Init(ENCODER_INSTANCE_TIM4, ENCODER_MODE_TI12);  /* 4倍频模式（TI12） */
    
    /* 方案2：使用TIM3部分重映射（PB4/PB5）- 如果需要使用PB4/PB5 */
    // ENCODER_SetTIM3Remap(true, false);  /* 部分重映射：PB4/PB5 */
    // encoder_status = ENCODER_Init(ENCODER_INSTANCE_TIM3, ENCODER_MODE_TI12);
    
    /* 方案3：使用TIM3完全重映射（PC6/PC7）- 如果需要使用PC6/PC7 */
    // ENCODER_SetTIM3Remap(true, true);   /* 完全重映射：PC6/PC7 */
    // encoder_status = ENCODER_Init(ENCODER_INSTANCE_TIM3, ENCODER_MODE_TI12);
    
    /* 方案4：使用TIM3默认（PA6/PA7）- 如果需要使用PA6/PA7 */
    // encoder_status = ENCODER_Init(ENCODER_INSTANCE_TIM3, ENCODER_MODE_TI12);
    if (encoder_status != ENCODER_OK)
    {
        LOG_ERROR("MAIN", "编码器接口初始化失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        /* 编码器初始化失败，LED快速闪烁提示 */
        while(1)
        {
            LED1_On();
            Delay_ms(100);
            LED1_Off();
            Delay_ms(100);
        }
    }
    
    /* 启动编码器 */
    encoder_status = ENCODER_Start(ENCODER_INSTANCE_TIM4);  /* 使用TIM4 */
    if (encoder_status != ENCODER_OK)
    {
        LOG_ERROR("MAIN", "编码器启动失败: %d", encoder_status);
        ErrorHandler_Handle(encoder_status, "ENCODER");
        while(1) { Delay_ms(1000); }
    }
    
    /* 清零计数器 */
    encoder_status = ENCODER_ClearCount(ENCODER_INSTANCE_TIM4);  /* 使用TIM4 */
    if (encoder_status != ENCODER_OK)
    {
        LOG_WARN("MAIN", "编码器清零失败: %d（可能功能未实现）", encoder_status);
    }
    
    LOG_INFO("MAIN", "编码器接口已初始化: TIM4，PB6/PB7，4倍频模式（TI12，软件滤波实现1:1计数）");
    LOG_INFO("MAIN", "注意：PB0/PB1无法用于编码器接口模式（它们是CH3/CH4）");
    LOG_INFO("MAIN", "已改用TIM4的PB6/PB7（CH1/CH2），请将编码器改接到PB6/PB7");
    
    /* 调试：读取GPIO状态，检查编码器信号是否到达 */
    uint8_t pb6_state = GPIO_ReadPin(GPIOB, GPIO_Pin_6);
    uint8_t pb7_state = GPIO_ReadPin(GPIOB, GPIO_Pin_7);
    LOG_INFO("MAIN", "GPIO状态检查: PB6=%d, PB7=%d", pb6_state, pb7_state);
    LOG_INFO("MAIN", "提示：如果PB6/PB7始终为0或1，可能是硬件连接问题");
    
    /* ========== 步骤9：主循环 ========== */
    while(1)
    {
        /* 读取编码器计数值 */
        encoder_status = ENCODER_ReadCount(ENCODER_INSTANCE_TIM4, &encoder_count);  /* 使用TIM4 */
        if (encoder_status != ENCODER_OK)
        {
            LOG_ERROR("MAIN", "读取编码器计数值失败: %d", encoder_status);
            ErrorHandler_Handle(encoder_status, "ENCODER");
            Delay_ms(100);
            continue;
        }
        
        /* 检查计数值是否变化 */
        if (encoder_count != last_count)
        {
            /* 
             * STM32编码器接口模式硬件特性说明：
             * - 编码器接口模式会在TI1和TI2的所有边沿（上升沿+下降沿）都计数
             * - 一个物理步进 = 4个边沿（TI1上升、TI1下降、TI2上升、TI2下降）= CNT变化4
             * - 这是正常的硬件特性，不是bug，4倍频提高了分辨率
             * - 解决方案：在读取时直接除以4，得到真实的物理步进数
             */
            
            /* 计算实际物理步进数：原始计数值除以4 */
            /* 注意：对于负数，直接使用除法会自动向下取整（向负无穷方向） */
            /* 例如：-1/4 = 0, -4/4 = -1, -5/4 = -1, -8/4 = -2 */
            int32_t actual_steps = encoder_count / 4;
            
            /* 获取旋转方向 */
            encoder_status = ENCODER_GetDirection(ENCODER_INSTANCE_TIM4, &direction);
            if (encoder_status == ENCODER_OK)
            {
                if (direction == ENCODER_DIR_FORWARD)
                {
                    strcpy(direction_str, "CW ");  /* 顺时针（正转） */
                }
                else
                {
                    strcpy(direction_str, "CCW");  /* 逆时针（反转） */
                }
            }
            else
            {
                /* 根据计数值变化判断方向 */
                if (encoder_count > last_count)
                {
                    strcpy(direction_str, "CW ");  /* 顺时针（正转） */
                }
                else
                {
                    strcpy(direction_str, "CCW");  /* 逆时针（反转） */
                }
            }
            
            /* 更新OLED显示 */
            OLED_ShowSignedNum(3, 10, encoder_count, 5);  /* 显示原始计数值（4倍频） */
            OLED_ShowString(4, 5, direction_str);
            OLED_ShowString(1, 1, "Steps:");  /* 第1行，第1列 */
            OLED_ShowSignedNum(1, 8, actual_steps, 5);  /* 显示实际物理步数（1:1） */
            
            /* UART日志输出 */
            LOG_INFO("ENCODER", "原始计数: %d (4倍频), 实际步数: %d (1:1), 方向: %s", 
                     encoder_count, actual_steps, direction_str);
            
            /* LED闪烁提示 */
            LED1_Toggle();
            
            /* 更新last_count */
            last_count = encoder_count;
        }
        
        /* 每1秒输出一次GPIO状态（用于调试） */
        static uint32_t debug_counter = 0;
        debug_counter++;
        if (debug_counter >= 100)  /* 10ms * 100 = 1秒 */
        {
            debug_counter = 0;
            pb6_state = GPIO_ReadPin(GPIOB, GPIO_Pin_6);
            pb7_state = GPIO_ReadPin(GPIOB, GPIO_Pin_7);
            LOG_DEBUG("ENCODER", "GPIO状态: PB6=%d, PB7=%d, 计数值=%d", pb6_state, pb7_state, encoder_count);
        }
        
        /* 延时降低CPU占用率 */
        Delay_ms(10);
    }
}
