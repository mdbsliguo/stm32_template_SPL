/**
 * @file main_example.c
 * @brief Flash09最小化版本示例主函数
 * @details 演示TF卡集成FatFS文件系统（单分区方案）- 最小化版本
 * 
 * @note 业务逻辑已封装到flash09_app.c模块
 * @note 最小化版本：移除OLED、UART、Log等模块，只保留核心功能
 */

#include "stm32f10x.h"
#include "delay.h"
#include "led.h"
#include "config.h"
#include "flash09_app.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 主函数
 */
int main(void)
{
    Flash09_AppStatus_t app_status;
    char mount_path[8];
    
    /* ========== 步骤1：系统初始化 ========== */
    app_status = Flash09_AppInit();
    if (app_status != FLASH09_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤2：SD卡初始化 ========== */
    app_status = Flash09_InitSDCard();
    if (app_status != FLASH09_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤3：挂载文件系统 ========== */
    app_status = Flash09_MountFileSystem(mount_path, sizeof(mount_path));
    if (app_status != FLASH09_APP_OK) {
        while(1) { 
            LED1_Toggle();
            Delay_ms(500); 
        }
    }
    
    /* ========== 步骤4：显示文件系统信息 ========== */
    Flash09_ShowFileSystemInfo(mount_path);
    
    /* ========== 步骤5：运行主循环（约30秒） ========== */
    app_status = Flash09_RunMainLoop(mount_path, 30000);  /* 30秒 */
    if (app_status != FLASH09_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤6：程序结束流程 ========== */
    app_status = Flash09_Shutdown();
    
    /* 结束程序 */
    while(1) {
        Delay_ms(1000);
    }
}
