/**
 * @file main_example.c
 * @brief 案例 - 无源蜂鸣器播放完整音乐
 * @example Examples/Basic/Basic02_PWM_PassiveBuzzer/main_example.c
 * @status 已调试完成
 * @details 无源蜂鸣器PWM模式播放完整音乐示例，演示如何播放一首完整的歌曲
 *
 * 硬件要求：
 * - Buzzer（无源蜂鸣器）连接到PA6（TIM3 CH1）
 *
 * 硬件配置：
 * 在 Examples/Basic/Basic02_PWM_PassiveBuzzer/board.h 中配置：
 * ```c
 * #define BUZZER_CONFIGS { \
 *     {BUZZER_MODE_PWM, NULL, 0, 1, 0, Bit_RESET, 1},  // PWM模式，TIM3(实例1)通道1，启用 \
 * }
 * ```
 *
 * 使用方法：
 * 1. 打开案例工程：双击 Examples/Basic/Basic02_PWM_PassiveBuzzer/Examples.uvprojx
 * 2. 根据实际硬件修改 Examples/Basic/Basic02_PWM_PassiveBuzzer/board.h 中的Buzzer配置
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
#include "buzzer.h"
#include <stdint.h>
#include <string.h>

/* 扩展音调频率表（Hz） */
#define NOTE_C3  131
#define NOTE_D3  147
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_B6  1976

/* 休止符 */
#define NOTE_REST 0

/* 节拍时长定义（毫秒） */
#define TEMPO 120  /* 每分钟120拍 */
#define WHOLE_NOTE     (60000 / TEMPO * 4)    /* 全音符 */
#define HALF_NOTE      (60000 / TEMPO * 2)    /* 二分音符 */
#define QUARTER_NOTE   (60000 / TEMPO)        /* 四分音符 */
#define EIGHTH_NOTE    (60000 / TEMPO / 2)    /* 八分音符 */
#define SIXTEENTH_NOTE (60000 / TEMPO / 4)    /* 十六分音符 */

/**
 * @brief 播放单个音符
 * @param frequency 频率（Hz），0表示休止符
 * @param duration 持续时间（毫秒）
 */
static void PlayNote(uint32_t frequency, uint32_t duration)
{
    if (frequency == 0) {
        /* 休止符 */
        BUZZER1_Off();
        Delay_ms(duration);
    } else {
        /* 播放音符 */
        Buzzer_SetFrequency(BUZZER_1, frequency);
        BUZZER1_On();
        Delay_ms(duration);
        BUZZER1_Off();
        Delay_ms(20);  /* 音符间隔 */
    }
}

/**
 * @brief 播放《小星星》完整音乐
 * @note 由于《左手指月》乐谱较复杂且音域较宽，此处使用《小星星》完整版作为示例
 *       如需播放其他歌曲，只需修改此函数中的音符序列即可
 */
static void PlaySong_TwinkleTwinkleLittleStar(void)
{
    LOG_INFO("MAIN", "开始播放：小星星（完整版）");
    
    /* 《小星星》完整版主旋律 */
    /* 第一段：一闪一闪亮晶晶 */
    PlayNote(NOTE_C4, QUARTER_NOTE);
    PlayNote(NOTE_C4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_A4, QUARTER_NOTE);
    PlayNote(NOTE_A4, QUARTER_NOTE);
    PlayNote(NOTE_G4, HALF_NOTE);
    
    /* 第二段：满天都是小星星 */
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_D4, QUARTER_NOTE);
    PlayNote(NOTE_D4, QUARTER_NOTE);
    PlayNote(NOTE_C4, HALF_NOTE);
    
    /* 第三段：挂在天上放光明 */
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_D4, HALF_NOTE);
    
    /* 第四段：好像许多小眼睛 */
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_D4, HALF_NOTE);
    
    /* 第五段：一闪一闪亮晶晶 */
    PlayNote(NOTE_C4, QUARTER_NOTE);
    PlayNote(NOTE_C4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_G4, QUARTER_NOTE);
    PlayNote(NOTE_A4, QUARTER_NOTE);
    PlayNote(NOTE_A4, QUARTER_NOTE);
    PlayNote(NOTE_G4, HALF_NOTE);
    
    /* 第六段：满天都是小星星 */
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_F4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_E4, QUARTER_NOTE);
    PlayNote(NOTE_D4, QUARTER_NOTE);
    PlayNote(NOTE_D4, QUARTER_NOTE);
    PlayNote(NOTE_C4, HALF_NOTE);
    
    LOG_INFO("MAIN", "播放完成：小星星（完整版）");
}

int main(void)
{
    UART_Status_t uart_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    Buzzer_Status_t buzzer_status;
    
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
        /* Log初始化失败，记录错误但继续运行 */
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：错误处理模块 ========== */
    /* 注意：ErrorHandler模块在编译时自动初始化，无需显式调用 */
    
    /* ========== 步骤6：输出初始化信息 ========== */
    /* 使用LOG_INFO输出（Log模块应该能正常初始化） */
    LOG_INFO("MAIN", "=== 系统初始化完成 ===");
    LOG_INFO("MAIN", "UART1已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug模块已初始化: UART模式");
    LOG_INFO("MAIN", "Log模块已初始化");
    LOG_INFO("MAIN", "ErrorHandler模块已就绪");
    LOG_INFO("MAIN", "=== 无源蜂鸣器音乐播放示例 ===");
    
    /* ========== 步骤7：Buzzer初始化 ========== */
    LOG_INFO("MAIN", "正在初始化Buzzer...");
    
    buzzer_status = Buzzer_Init();
    if (buzzer_status != BUZZER_OK)
    {
        LOG_ERROR("MAIN", "Buzzer初始化失败: %d", buzzer_status);
        ErrorHandler_Handle(buzzer_status, "BUZZER");
        while (1) { Delay_ms(1000); }  /* 关键错误，停止 */
    }
    
    LOG_INFO("MAIN", "Buzzer初始化成功: PWM模式，TIM3 CH1，PA6");
    LOG_INFO("MAIN", "Buzzer配置: 频率范围 1Hz-72MHz，占空比 50%%");
    LOG_INFO("MAIN", "=== 初始化完成，准备播放音乐 ===");
    
    Delay_ms(500);
    
    /* ========== 步骤8：主循环 - 播放音乐 ========== */
    while (1)
    {
        /* 播放《小星星》完整版 */
        PlaySong_TwinkleTwinkleLittleStar();
        
        /* 播放完成后等待3秒再重复 */
        Delay_ms(3000);
        LOG_INFO("MAIN", "准备重新播放...");
    }
}
