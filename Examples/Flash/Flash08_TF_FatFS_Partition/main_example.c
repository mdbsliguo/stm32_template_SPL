/**
 * @file main_example.c
 * @brief Flash08示例主函数
 * @details 演示TF卡集成FatFS文件系统（单分区方案）
 * 
 * @note 业务逻辑已封装到flash08_app.c模块
 */

#include "stm32f10x.h"
#include "delay.h"
#include "led.h"
#include "config.h"
#include "flash08_app.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 主函数
 */
int main(void)
{
    Flash08_AppStatus_t app_status;
    char mount_path[8];
    
    /* ========== 步骤1：系统初始化 ========== */
    app_status = Flash08_AppInit();
    if (app_status != FLASH08_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤2：SD卡初始化 ========== */
    app_status = Flash08_InitSDCard();
    if (app_status != FLASH08_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤3：挂载文件系统 ========== */
    app_status = Flash08_MountFileSystem(mount_path, sizeof(mount_path));
    if (app_status != FLASH08_APP_OK) {
        while(1) { 
            LED1_Toggle();
            Delay_ms(500); 
        }
    }
    
    /* ========== 步骤4：显示文件系统信息 ========== */
    Flash08_ShowFileSystemInfo(mount_path);
    
    /* ========== 步骤5：运行测试 ========== */
    app_status = Flash08_RunTests();
    if (app_status != FLASH08_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤6：运行主循环（约30秒） ========== */
    app_status = Flash08_RunMainLoop(mount_path, 30000);  /* 30秒 */
    if (app_status != FLASH08_APP_OK) {
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤7：程序结束流程 ========== */
    app_status = Flash08_Shutdown();
    
    /* 结束程序 */
    while(1) {
        Delay_ms(1000);
    }
}
