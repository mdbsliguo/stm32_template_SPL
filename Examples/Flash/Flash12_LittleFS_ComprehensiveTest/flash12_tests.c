/**
 * @file flash12_tests.c
 * @brief Flash12测试函数实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details Flash12案例的所有测试函数实现
 */

#include "flash12_tests.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "littlefs_wrapper.h"
#include "fs_wrapper.h"
#include "log.h"
#include "config.h"
#include <stdint.h>
#include <string.h>

/* ==================== 辅助函数 ==================== */

/**
 * @brief 检查文件系统状态并显示诊断信息（简化版）
 */
void check_filesystem_status(void)
{
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    LittleFS_Status_t status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    
    if (status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "FS: %lluKB free/%lluKB total", 
                 (unsigned long long)free_bytes / 1024, 
                 (unsigned long long)total_bytes / 1024);
        
        if (free_bytes < 1024)
        {
            LOG_ERROR("TEST", "FS space < 1KB!");
        }
    }
}

/* ==================== 测试函数实现 ==================== */

/**
 * @brief 测试12：FS_WRAPPER接口验证（可选）
 */
void test12_fs_wrapper(void)
{
    #ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
    #if CONFIG_MODULE_FS_WRAPPER_ENABLED
    LOG_INFO("TEST", "=== 测试12：FS_WRAPPER接口验证 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test12: FS_WRAPPER");
    
    /* 初始化FS_WRAPPER */
    error_code_t fs_status = FS_Init();
    if (fs_status == FS_WRAPPER_OK) {
        LOG_INFO("TEST", "FS_WRAPPER初始化成功");
        OLED_ShowString(2, 1, "FS_WRAPPER OK");
        
        /* 测试写入文件 */
        const char* test_data = "Hello from FS_WRAPPER!";
        fs_status = FS_WriteFile(FS_DIR_CONFIG, "test_wrapper.txt", test_data, strlen(test_data));
        if (fs_status == FS_WRAPPER_OK) {
            LOG_INFO("TEST", "FS_WRAPPER写入成功");
            OLED_ShowString(3, 1, "Write OK");
            
            /* 测试读取文件 */
            char read_buf[64];
            fs_status = FS_ReadFile(FS_DIR_CONFIG, "test_wrapper.txt", 0, read_buf, sizeof(read_buf));
            if (fs_status == FS_WRAPPER_OK) {
                LOG_INFO("TEST", "FS_WRAPPER读取成功: %s", read_buf);
                OLED_ShowString(4, 1, "Read OK");
            } else {
                LOG_ERROR("TEST", "FS_WRAPPER读取失败: %d", fs_status);
                OLED_ShowString(4, 1, "Read Failed");
            }
        } else {
            LOG_ERROR("TEST", "FS_WRAPPER写入失败: %d", fs_status);
            OLED_ShowString(3, 1, "Write Failed");
        }
    } else {
        LOG_ERROR("TEST", "FS_WRAPPER初始化失败: %d", fs_status);
        OLED_ShowString(2, 1, "Init Failed");
    }
    
    Delay_ms(2000);
    #endif
    #endif
}

/* ==================== 测试函数实现 ==================== */

void test1_filesystem_init(void)
{
    LOG_INFO("TEST", "=== 测试1：文件系统初始化测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test1: Init");
    
    /* 检查文件系统是否已挂载 */
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    LittleFS_Status_t status = LittleFS_GetInfo(&total_bytes, &free_bytes);
    
    if (status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "测试1通过: 文件系统已挂载");
        LOG_INFO("TEST", "  总空间: %llu KB", (unsigned long long)total_bytes / 1024);
        LOG_INFO("TEST", "  空闲空间: %llu KB", (unsigned long long)free_bytes / 1024);
        OLED_ShowString(2, 1, "PASS");
        OLED_ShowString(3, 1, "Mounted OK");
    }
    else
    {
        LOG_ERROR("TEST", "测试1失败: 文件系统未挂载 (错误: %d)", status);
        OLED_ShowString(2, 1, "FAIL");
        OLED_ShowString(3, 1, "Not Mounted");
    }
    
    Delay_ms(2000);
}

void test2_basic_file_ops(void)
{
    LOG_INFO("TEST", "=== 测试2：基础文件操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test2: Basic");
    
    const char* test_file = "/test2.txt";
    const char* test_data = "Hello LittleFS!";
    char read_buf[64] = {0};
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 写入文件 */
    LOG_INFO("TEST", "打开文件: '%s'", test_file);
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        LOG_INFO("TEST", "文件打开成功，写入 %lu 字节", (unsigned long)strlen(test_data));
        uint32_t written = 0;
        status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        LOG_INFO("TEST", "写入结果: status=%d, written=%lu, expected=%lu", 
                 status, (unsigned long)written, (unsigned long)strlen(test_data));
        if (status == LITTLEFS_OK && written == strlen(test_data))
        {
            LOG_INFO("TEST", "开始同步文件...");
            status = LittleFS_FileSync(&file);
            LOG_INFO("TEST", "同步结果: status=%d", status);
            if (status == LITTLEFS_OK)
            {
                LittleFS_FileClose(&file);
            
                /* 读取文件 */
                memset(&file, 0, sizeof(file));
                status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
                if (status == LITTLEFS_OK)
                {
                    uint32_t read = 0;
                    status = LittleFS_FileRead(&file, read_buf, sizeof(read_buf) - 1, &read);
                    LittleFS_FileClose(&file);
                    
                    /* 添加字符串结束符 */
                    if (status == LITTLEFS_OK && read < sizeof(read_buf)) {
                        read_buf[read] = '\0';
                    }
                    
                    if (status == LITTLEFS_OK && strcmp(read_buf, test_data) == 0)
                {
                    LOG_INFO("TEST", "测试2通过: 文件读写成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Read/Write OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试2失败: 数据不匹配");
                    LOG_ERROR("TEST", "  写入数据: '%s' (len=%lu)", test_data, (unsigned long)strlen(test_data));
                    LOG_ERROR("TEST", "  读取数据: '%s' (len=%lu, read=%lu)", read_buf, (unsigned long)strlen(read_buf), (unsigned long)read);
                        OLED_ShowString(2, 1, "FAIL");
                        OLED_ShowString(3, 1, "Data Mismatch");
                    }
                }
                else
                {
                    LOG_ERROR("TEST", "测试2失败: 打开文件失败 (错误: %d)", status);
                    OLED_ShowString(2, 1, "FAIL");
                    OLED_ShowString(3, 1, "Open Failed");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试2失败: 同步失败 (错误: %d)", status);
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "FAIL");
                OLED_ShowString(3, 1, "Sync Failed");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试2失败: 写入失败 (错误: %d, written=%lu, expected=%lu)", 
                     status, (unsigned long)written, (unsigned long)strlen(test_data));
            LittleFS_FileClose(&file);
            OLED_ShowString(2, 1, "FAIL");
            OLED_ShowString(3, 1, "Write Failed");
        }
    }
    else
    {
        LOG_ERROR("TEST", "测试2失败: 创建文件失败 (错误: %d)", status);
        OLED_ShowString(2, 1, "FAIL");
        OLED_ShowString(3, 1, "Create Failed");
    }
    
    Delay_ms(2000);
}

void test3_file_seek(void)
{
    LOG_INFO("TEST", "=== 测试3：文件定位测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test3: Seek");
    
    const char* test_file = "/test3.txt";
    const char* test_data = "0123456789ABCDEF";
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
        /* 创建并写入测试文件 */
        LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
        if (status == LITTLEFS_OK)
        {
            uint32_t written = 0;
            LittleFS_Status_t write_status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
            if (write_status == LITTLEFS_OK && written == strlen(test_data))
            {
                LittleFS_FileSync(&file);
                LittleFS_FileClose(&file);
                
                /* 测试SEEK */
                memset(&file, 0, sizeof(file));
                status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
                if (status == LITTLEFS_OK)
                {
                    /* 先读取文件大小验证 */
                    uint32_t file_size = 0;
                    LittleFS_Status_t size_status = LittleFS_FileSize(&file, &file_size);
                    LOG_INFO("TEST", "文件大小: %lu 字节", (unsigned long)file_size);
                    
                    /* 定位到位置5 */
                    LittleFS_Status_t seek_status = LittleFS_FileSeek(&file, 5, LFS_SEEK_SET);
                    if (seek_status == LITTLEFS_OK)
                    {
                        char buf[2] = {0};
                        uint32_t read = 0;
                        LittleFS_Status_t read_status = LittleFS_FileRead(&file, buf, 1, &read);
                        /* 添加字符串结束符 */
                        if (read_status == LITTLEFS_OK && read < sizeof(buf)) {
                            buf[read] = '\0';
                        }
                        if (read_status == LITTLEFS_OK && read == 1 && buf[0] == '5')
                        {
                            LOG_INFO("TEST", "测试3通过: 文件定位成功");
                            OLED_ShowString(2, 1, "PASS");
                            OLED_ShowString(3, 1, "Seek OK");
                        }
                        else
                        {
                            LOG_ERROR("TEST", "测试3失败: 定位后读取错误 (read_status=%d, read=%lu, buf[0]=0x%02X, file_size=%lu)", 
                                     read_status, (unsigned long)read, (unsigned char)buf[0], (unsigned long)file_size);
                            OLED_ShowString(2, 1, "FAIL");
                        }
                    }
                    else
                    {
                        LOG_ERROR("TEST", "测试3失败: SEEK失败 (错误: %d)", seek_status);
                        OLED_ShowString(2, 1, "FAIL");
                    }
                    LittleFS_FileClose(&file);
                }
                else
                {
                    LOG_ERROR("TEST", "测试3失败: 重新打开文件失败 (错误: %d)", status);
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试3失败: 写入失败 (write_status=%d, written=%lu, expected=%lu)", 
                         write_status, (unsigned long)written, (unsigned long)strlen(test_data));
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试3失败: 创建文件失败 (错误: %d)", status);
            OLED_ShowString(2, 1, "FAIL");
        }
    
    Delay_ms(2000);
}

void test4_append_write(void)
{
    LOG_INFO("TEST", "=== 测试4：追加写入测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test4: Append");
    
    const char* test_file = "/test4.txt";
    const char* line1 = "Line1\n";
    const char* line2 = "Line2\n";
    char read_buf[32] = {0};
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 写入第一行 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        LittleFS_FileWrite(&file, line1, strlen(line1), &written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        
        /* 追加第二行 */
        memset(&file, 0, sizeof(file));
        status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_APPEND);
        if (status == LITTLEFS_OK)
        {
            LittleFS_FileWrite(&file, line2, strlen(line2), &written);
            LittleFS_FileSync(&file);
            LittleFS_FileClose(&file);
            
            /* 读取验证 */
            memset(&file, 0, sizeof(file));
            status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
            if (status == LITTLEFS_OK)
            {
                uint32_t read = 0;
                LittleFS_Status_t read_status = LittleFS_FileRead(&file, read_buf, sizeof(read_buf) - 1, &read);
                LittleFS_FileClose(&file);
                
                /* 添加字符串结束符 */
                if (read_status == LITTLEFS_OK && read < sizeof(read_buf)) {
                    read_buf[read] = '\0';
                }
                
                if (read_status == LITTLEFS_OK && strstr(read_buf, "Line1") != NULL && strstr(read_buf, "Line2") != NULL)
                {
                    LOG_INFO("TEST", "测试4通过: 追加写入成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Append OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试4失败: 追加数据不完整");
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
        }
    }
    
    Delay_ms(2000);
}

void test5_file_truncate(void)
{
    LOG_INFO("TEST", "=== 测试9：文件截断测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test9: Truncate");
    
    /* 检查文件系统状态 */
    check_filesystem_status();
    
    const char* test_file = "/test5.txt";
    /* 使用256字节文件进行截断测试 */
    char test_data[256];
    memset(test_data, 'A', sizeof(test_data));
    test_data[sizeof(test_data) - 1] = '\0';
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    LOG_INFO("TEST", "创建文件: '%s', 大小: %lu 字节", test_file, (unsigned long)(sizeof(test_data) - 1));
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        status = LittleFS_FileWrite(&file, test_data, sizeof(test_data) - 1, &written);
        if (status == LITTLEFS_OK && written == sizeof(test_data) - 1)
        {
            status = LittleFS_FileSync(&file);
            if (status == LITTLEFS_OK)
            {
                LittleFS_FileClose(&file);
        
                /* 截断到128字节 */
                LOG_INFO("TEST", "截断文件到128字节...");
                memset(&file, 0, sizeof(file));
                status = LittleFS_FileOpen(&file, test_file, LFS_O_RDWR);
                if (status == LITTLEFS_OK)
                {
                    /* 截断文件到128字节（不需要先定位） */
                    status = LittleFS_FileTruncate(&file, 128);
                    LOG_INFO("TEST", "截断操作结果: %d", status);
                    if (status == LITTLEFS_OK)
                    {
                        /* 检查文件系统状态 */
                        check_filesystem_status();
                        
                        status = LittleFS_FileSync(&file);
                        LOG_INFO("TEST", "同步操作结果: %d", status);
                        if (status == LITTLEFS_OK)
                        {
                            LittleFS_FileClose(&file);
                            
                            /* 验证文件大小 */
                            lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
                            if (lfs != NULL)
                            {
                                struct lfs_info info;
                                int stat_result = lfs_stat(lfs, test_file, &info);
                                LOG_INFO("TEST", "文件状态检查: stat_result=%d, size=%lu", stat_result, (unsigned long)info.size);
                                if (stat_result == 0 && info.size == 128)
                                {
                                    LOG_INFO("TEST", "测试9通过: 文件截断成功 (大小: %lu)", (unsigned long)info.size);
                                    OLED_ShowString(2, 1, "PASS");
                                    OLED_ShowString(3, 1, "Truncate OK");
                                }
                                else
                                {
                                    LOG_ERROR("TEST", "测试9失败: 文件大小不正确 (stat_result=%d, size=%lu, expected=128)", 
                                             stat_result, (unsigned long)info.size);
                                    OLED_ShowString(2, 1, "FAIL");
                                }
                            }
                            else
                            {
                                LOG_ERROR("TEST", "测试9失败: 无法获取LittleFS实例");
                                OLED_ShowString(2, 1, "FAIL");
                            }
                        }
                        else
                        {
                            /* 如果是NOSPC错误，说明文件系统状态异常，但测试可以继续 */
                            if (status == LITTLEFS_ERROR_NOSPC)
                            {
                                LOG_WARN("TEST", "测试9警告: 同步失败 (NOSPC错误: %d)，可能是元数据空间不足", status);
                                LOG_WARN("TEST", "文件截断操作可能已成功，但同步时元数据空间不足");
                                check_filesystem_status();
                            }
                            else
                            {
                                LOG_ERROR("TEST", "测试9失败: 同步失败 (错误: %d)", status);
                                check_filesystem_status();
                            }
                            LittleFS_FileClose(&file);
                            OLED_ShowString(2, 1, "FAIL");
                        }
                    }
                    else
                    {
                        LOG_ERROR("TEST", "测试9失败: 截断失败 (错误: %d)", status);
                        check_filesystem_status();
                        LittleFS_FileClose(&file);
                        OLED_ShowString(2, 1, "FAIL");
                    }
                }
                else
                {
                    LOG_ERROR("TEST", "测试9失败: 打开文件失败 (错误: %d)", status);
                    check_filesystem_status();
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试9失败: 同步失败 (错误: %d)", status);
                check_filesystem_status();
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试9失败: 写入失败 (错误: %d, written=%lu, expected=%lu)", 
                     status, (unsigned long)written, (unsigned long)(sizeof(test_data) - 1));
            check_filesystem_status();
            LittleFS_FileClose(&file);
            OLED_ShowString(2, 1, "FAIL");
        }
    }
    else
    {
        LOG_ERROR("TEST", "测试9失败: 打开文件失败 (错误: %d)", status);
        check_filesystem_status();
        OLED_ShowString(2, 1, "FAIL");
    }
    
    Delay_ms(2000);
}

void test6_file_rename(void)
{
    LOG_INFO("TEST", "=== 测试5：文件重命名测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test5: Rename");
    
    const char* old_name = "/test6_old.txt";
    const char* new_name = "/test6_new.txt";
    const char* test_data = "Rename Test";
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, old_name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        
        /* 重命名 */
        lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
        if (lfs != NULL)
        {
            LOG_INFO("TEST", "开始重命名: '%s' -> '%s'", old_name, new_name);
            int rename_result = lfs_rename(lfs, old_name, new_name);
            LOG_INFO("TEST", "重命名结果: %d", rename_result);
            if (rename_result == 0)
            {
                /* 验证旧文件不存在，新文件存在 */
                struct lfs_info info;
                int old_stat = lfs_stat(lfs, old_name, &info);
                LOG_INFO("TEST", "旧文件状态: %d", old_stat);
                int new_stat = lfs_stat(lfs, new_name, &info);
                LOG_INFO("TEST", "新文件状态: %d", new_stat);
                if (old_stat != 0 && new_stat == 0)
                {
                    LOG_INFO("TEST", "测试5通过: 文件重命名成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Rename OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试5失败: 重命名验证失败 (old_stat=%d, new_stat=%d)", old_stat, new_stat);
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试5失败: 重命名失败 (错误: %d)", rename_result);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试5失败: 无法获取LittleFS实例");
            OLED_ShowString(2, 1, "FAIL");
        }
    }
    
    Delay_ms(2000);
}

void test7_file_delete(void)
{
    LOG_INFO("TEST", "=== 测试6：文件删除测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test6: Delete");
    
    const char* test_file = "/test7.txt";
    const char* test_data = "Delete Test";
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        LittleFS_FileSync(&file);
        LittleFS_FileClose(&file);
        
        /* 删除文件 */
        lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
        if (lfs != NULL)
        {
            int remove_result = lfs_remove(lfs, test_file);
            if (remove_result == 0)
            {
                /* 验证文件不存在 */
                struct lfs_info info;
                if (lfs_stat(lfs, test_file, &info) != 0)
                {
                    LOG_INFO("TEST", "测试6通过: 文件删除成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Delete OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试6失败: 文件仍存在");
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试6失败: 删除失败 (错误: %d)", remove_result);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
    }
    
    Delay_ms(2000);
}

void test8_directory_ops(void)
{
    LOG_INFO("TEST", "=== 测试7：目录操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test7: Directory");
    
    const char* test_dir = "/testdir8";
    const char* test_file = "/testdir8/file.txt";
    const char* test_data = "Dir Test";
    
    lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
    if (lfs != NULL)
    {
        /* 创建目录 */
        int mkdir_result = lfs_mkdir(lfs, test_dir);
        if (mkdir_result == 0 || mkdir_result == LFS_ERR_EXIST)
        {
            /* 在目录中创建文件 */
            lfs_file_t file;
            memset(&file, 0, sizeof(file));
            LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
            if (status == LITTLEFS_OK)
            {
                uint32_t written = 0;
                LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
                LittleFS_FileSync(&file);
                LittleFS_FileClose(&file);
                
                /* 验证文件存在 */
                struct lfs_info info;
                if (lfs_stat(lfs, test_file, &info) == 0)
                {
                    LOG_INFO("TEST", "测试7通过: 目录操作成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Dir OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试7失败: 目录中文件不存在");
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试7失败: 创建文件失败 (错误: %d)", status);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试7失败: 创建目录失败 (错误: %d)", mkdir_result);
            OLED_ShowString(2, 1, "FAIL");
        }
    }
    else
    {
        LOG_ERROR("TEST", "测试7失败: 无法获取LittleFS实例");
        OLED_ShowString(2, 1, "FAIL");
    }
    
    Delay_ms(2000);
}

void test9_file_attributes(void)
{
    LOG_INFO("TEST", "=== 测试8：文件属性测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test8: Attributes");
    
    const char* test_file = "/test9.txt";
    const char* attr_name = "version";
    const char* attr_value = "1.0";
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 创建文件 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        const char* test_data = "Attr Test";
        uint32_t written = 0;
        LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        LittleFS_FileSync(&file);
        
        /* 设置属性 */
        lfs_t* lfs = LittleFS_GetLFS(LITTLEFS_INSTANCE_0);
        if (lfs != NULL)
        {
            int setattr_result = lfs_setattr(lfs, test_file, attr_name[0], attr_value, strlen(attr_value));
            if (setattr_result == 0)
            {
                /* 获取属性 */
                char read_attr[16] = {0};
                int getattr_result = lfs_getattr(lfs, test_file, attr_name[0], read_attr, sizeof(read_attr));
                if (getattr_result == strlen(attr_value) && strcmp(read_attr, attr_value) == 0)
                {
                    LOG_INFO("TEST", "测试8通过: 文件属性操作成功");
                    OLED_ShowString(2, 1, "PASS");
                    OLED_ShowString(3, 1, "Attr OK");
                }
                else
                {
                    LOG_ERROR("TEST", "测试8失败: 属性读取失败");
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试8失败: 属性设置失败 (错误: %d)", setattr_result);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试8失败: 无法获取LittleFS实例");
            OLED_ShowString(2, 1, "FAIL");
        }
        LittleFS_FileClose(&file);
    }
    else
    {
        LOG_ERROR("TEST", "测试8失败: 打开文件失败 (错误: %d)", status);
        OLED_ShowString(2, 1, "FAIL");
    }
    
    Delay_ms(2000);
}

void test10_atomic_operations(void)
{
    LOG_INFO("TEST", "=== 测试10：原子操作测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test10: Atomic");
    
    /* 检查文件系统状态 */
    check_filesystem_status();
    
    const char* test_file = "/test10.txt";
    const char* test_data = "Atomic Write Test";
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 原子写入：写入后立即同步 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        if (status == LITTLEFS_OK && written == strlen(test_data))
        {
            /* 立即同步确保数据落盘 */
            check_filesystem_status();
            status = LittleFS_FileSync(&file);
            LOG_INFO("TEST", "同步操作结果: %d", status);
            if (status == LITTLEFS_OK)
            {
                LittleFS_FileClose(&file);
                
                /* 验证数据 */
                memset(&file, 0, sizeof(file));
                status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
                if (status == LITTLEFS_OK)
                {
                    char read_buf[64] = {0};
                    uint32_t read = 0;
                    LittleFS_Status_t read_status = LittleFS_FileRead(&file, read_buf, sizeof(read_buf) - 1, &read);
                    LittleFS_FileClose(&file);
                    
                    /* 添加字符串结束符 */
                    if (read_status == LITTLEFS_OK && read < sizeof(read_buf)) {
                        read_buf[read] = '\0';
                    }
                    
                    if (read_status == LITTLEFS_OK && strcmp(read_buf, test_data) == 0)
                    {
                        LOG_INFO("TEST", "测试10通过: 原子操作成功");
                        OLED_ShowString(2, 1, "PASS");
                        OLED_ShowString(3, 1, "Atomic OK");
                    }
                    else
                    {
                        LOG_ERROR("TEST", "测试10失败: 数据不一致 (read_status=%d, read=%lu)", 
                                 read_status, (unsigned long)read);
                        LOG_ERROR("TEST", "  写入: '%s'", test_data);
                        LOG_ERROR("TEST", "  读取: '%s'", read_buf);
                        OLED_ShowString(2, 1, "FAIL");
                    }
                }
                else
                {
                    LOG_ERROR("TEST", "测试10失败: 打开文件失败 (错误: %d)", status);
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试10失败: 同步失败 (错误: %d)", status);
                check_filesystem_status();
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试10失败: 写入失败 (错误: %d, written=%lu, expected=%lu)", 
                     status, (unsigned long)written, (unsigned long)strlen(test_data));
            check_filesystem_status();
            LittleFS_FileClose(&file);
            OLED_ShowString(2, 1, "FAIL");
        }
    }
    else
    {
        LOG_ERROR("TEST", "测试10失败: 打开文件失败 (错误: %d)", status);
        OLED_ShowString(2, 1, "FAIL");
    }
    
    Delay_ms(2000);
}

void test11_power_protection(void)
{
    LOG_INFO("TEST", "=== 测试11：断电保护测试 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Test11: Power");
    
    /* 检查文件系统状态 */
    check_filesystem_status();
    
    const char* test_file = "/test11.txt";
    const char* test_data = "Power Protection Test";
    
    lfs_file_t file;
    memset(&file, 0, sizeof(file));
    
    /* 写入数据并同步 */
    LittleFS_Status_t status = LittleFS_FileOpen(&file, test_file, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (status == LITTLEFS_OK)
    {
        uint32_t written = 0;
        status = LittleFS_FileWrite(&file, test_data, strlen(test_data), &written);
        if (status == LITTLEFS_OK && written == strlen(test_data))
        {
            /* 同步确保数据落盘（断电保护） */
            check_filesystem_status();
            status = LittleFS_FileSync(&file);
            LOG_INFO("TEST", "同步操作结果: %d", status);
            if (status == LITTLEFS_OK)
            {
                /* 卸载并重新挂载模拟断电恢复 */
                LittleFS_FileClose(&file);
                LOG_INFO("TEST", "卸载文件系统...");
                status = LittleFS_Unmount();
                if (status == LITTLEFS_OK)
                {
                    Delay_ms(100);
                    
                    LOG_INFO("TEST", "重新挂载文件系统...");
                    status = LittleFS_Mount();
                    if (status == LITTLEFS_OK)
                    {
                        /* 验证数据是否还在 */
                        memset(&file, 0, sizeof(file));
                        status = LittleFS_FileOpen(&file, test_file, LFS_O_RDONLY);
                        if (status == LITTLEFS_OK)
                        {
                            char read_buf[64] = {0};
                            uint32_t read = 0;
                            LittleFS_Status_t read_status = LittleFS_FileRead(&file, read_buf, sizeof(read_buf) - 1, &read);
                            LittleFS_FileClose(&file);
                            
                            /* 添加字符串结束符 */
                            if (read_status == LITTLEFS_OK && read < sizeof(read_buf)) {
                                read_buf[read] = '\0';
                            }
                            
                            if (read_status == LITTLEFS_OK && strcmp(read_buf, test_data) == 0)
                            {
                                LOG_INFO("TEST", "测试11通过: 断电保护成功");
                                OLED_ShowString(2, 1, "PASS");
                                OLED_ShowString(3, 1, "Power OK");
                            }
                            else
                            {
                                LOG_ERROR("TEST", "测试11失败: 数据丢失 (read_status=%d, read=%lu)", 
                                         read_status, (unsigned long)read);
                                LOG_ERROR("TEST", "  写入: '%s'", test_data);
                                LOG_ERROR("TEST", "  读取: '%s'", read_buf);
                                OLED_ShowString(2, 1, "FAIL");
                            }
                        }
                        else
                        {
                            LOG_ERROR("TEST", "测试11失败: 重新挂载后文件不存在 (错误: %d)", status);
                            OLED_ShowString(2, 1, "FAIL");
                        }
                    }
                    else
                    {
                        LOG_ERROR("TEST", "测试11失败: 重新挂载失败 (错误: %d)", status);
                        OLED_ShowString(2, 1, "FAIL");
                    }
                }
                else
                {
                    LOG_ERROR("TEST", "测试11失败: 卸载失败 (错误: %d)", status);
                    OLED_ShowString(2, 1, "FAIL");
                }
            }
            else
            {
                LOG_ERROR("TEST", "测试11失败: 同步失败 (错误: %d)", status);
                check_filesystem_status();
                LittleFS_FileClose(&file);
                OLED_ShowString(2, 1, "FAIL");
            }
        }
        else
        {
            LOG_ERROR("TEST", "测试11失败: 写入失败 (错误: %d, written=%lu, expected=%lu)", 
                     status, (unsigned long)written, (unsigned long)strlen(test_data));
            LittleFS_FileClose(&file);
            OLED_ShowString(2, 1, "FAIL");
        }
    }
    else
    {
        LOG_ERROR("TEST", "测试11失败: 打开文件失败 (错误: %d)", status);
        OLED_ShowString(2, 1, "FAIL");
    }
    
    Delay_ms(2000);
}

/* ==================== 测试函数运行器 ==================== */

/**
 * @brief 运行所有Flash12测试
 */
void run_all_flash12_tests(void)
{
    LOG_INFO("MAIN", "=== 开始运行所有Flash12测试 ===");
    
    test1_filesystem_init();
    test2_basic_file_ops();
    test3_file_seek();
    test4_append_write();
    test6_file_rename();  /* 测试5：文件重命名 */
    test7_file_delete();  /* 测试6：文件删除 */
    test8_directory_ops();  /* 测试7：目录操作 */
    test9_file_attributes();  /* 测试8：文件属性 */
    
    /* 测试9前重新格式化（如果配置了），确保文件截断测试在干净的文件系统上运行 */
    #ifdef CONFIG_LITTLEFS_REFORMAT_BEFORE_TEST10
    #if CONFIG_LITTLEFS_REFORMAT_BEFORE_TEST10
    LOG_INFO("TEST", "=== 测试9前重新格式化文件系统 ===");
    OLED_Clear();
    OLED_ShowString(1, 1, "Reformat...");
    LittleFS_Status_t reformat_status = LittleFS_Unmount();
    if (reformat_status == LITTLEFS_OK)
    {
        Delay_ms(100);
        reformat_status = LittleFS_Format();
        if (reformat_status == LITTLEFS_OK)
        {
            reformat_status = LittleFS_Mount();
            if (reformat_status == LITTLEFS_OK)
            {
                LOG_INFO("TEST", "重新格式化并挂载成功");
                OLED_ShowString(2, 1, "Reformat OK");
                check_filesystem_status();
            }
            else
            {
                LOG_ERROR("TEST", "重新挂载失败: %d", reformat_status);
                OLED_ShowString(2, 1, "Remount Fail");
            }
        }
        else
        {
            LOG_ERROR("TEST", "重新格式化失败: %d", reformat_status);
            OLED_ShowString(2, 1, "Format Fail");
        }
    }
    else
    {
        LOG_ERROR("TEST", "卸载失败: %d", reformat_status);
        OLED_ShowString(2, 1, "Unmount Fail");
    }
    Delay_ms(1000);
    #endif
    #endif
    
    test5_file_truncate();  /* 测试9：文件截断 */
    
    test10_atomic_operations();
    test11_power_protection();
    
    #ifdef CONFIG_MODULE_FS_WRAPPER_ENABLED
    #if CONFIG_MODULE_FS_WRAPPER_ENABLED
    test12_fs_wrapper();
    #endif
    #endif
    
    OLED_Clear();
    OLED_ShowString(2, 1, "All Tests Done");
    OLED_ShowString(3, 1, "LittleFS Ready");
    LOG_INFO("MAIN", "=== 所有测试完成，进入主循环 ===");
    Delay_ms(1000);
}
