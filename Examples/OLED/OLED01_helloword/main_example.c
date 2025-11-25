/**
 * @file main_example.c
 * @brief 案例3 - OLED显示测试
 * @example Examples/example3_oled_test/main_example.c
 * @status 已调试完成 ?
 * @details 测试OLED模块的各种显示功能
 *
 * 硬件要求：
 * - OLED显示屏（SSD1306，I2C接口）
 * - SCL连接到PB8
 * - SDA连接到PB9
 *
 * 硬件配置：
 * 在 BSP/board.h 中配置：
 * - OLED_I2C_CONFIG: OLED I2C配置（PB8和PB9）
 *
 * 使用方法：
 * 1. 根据实际硬件修改 BSP/board.h 中的OLED配置
 * 2. 复制此文件到 Core/main.c
 * 3. 编译运行
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "oled_ssd1306.h"
#include "delay.h"

int main(void)
{
    OLED_Status_t oled_status;

    /* 系统初始化 */
    System_Init();

    /* OLED初始化 */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        /* 初始化失败，进入死循环 */
        while (1)
            ;
    }

    /* ========== 测试1：显示字符 ========== */

    /* 在第1行第1列显示字符'A' */
    OLED_ShowChar(1, 1, 'A');

    /* ========== 测试2：显示字符串 ========== */

    /* 在第1行第3列显示字符串"HelloWorld!" */
    OLED_ShowString(1, 3, "HelloWorld!");

    /* ========== 测试3：显示无符号十进制数 ========== */

    /* 在第2行第1列显示十进制数12345，显示长度5 */
    OLED_ShowNum(2, 1, 12345, 5);

    /* ========== 测试4：显示有符号十进制数 ========== */

    /* 在第2行第7列显示有符号十进制数-66，显示长度2 */
    OLED_ShowSignedNum(2, 7, -66, 2);

    /* ========== 测试5：显示十六进制数 ========== */

    /* 在第3行第1列显示十六进制数0xAA55，显示长度4 */
    OLED_ShowHexNum(3, 1, 0xAA55, 4);

    /* ========== 测试6：显示二进制数 ========== */

    /* 在第4行第1列显示二进制数0xAA55，显示长度16 */
    OLED_ShowBinNum(4, 1, 0xAA55, 16);

    /* 主循环 */
    while (1)
    {
        /* 可以在此添加动态显示内容 */
        Delay_ms(1000);
    }
}
