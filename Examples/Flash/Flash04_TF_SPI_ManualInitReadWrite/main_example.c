/**
 * @file main_example.c
 * @brief Flash03 - TF卡（MicroSD卡）SPI读写示例
 * @details 演示完整的SD协议生命周期，包括上电复位、版本识别、初始化、设备识别、状态查询、块读写和验证
 * 
 * 硬件连接：
 * - TF卡（MicroSD卡）连接到SPI2
 *   - CS：PA11（软件NSS模式）
 *   - SCK：PB13（SPI2_SCK）
 *   - MISO：PB14（SPI2_MISO）
 *   - MOSI：PB15（SPI2_MOSI）
 *   - VCC：3.3V
 *   - GND：GND
 * - OLED显示屏（用于显示关键信息）
 *   - SCL：PB8
 *   - SDA：PB9
 * - UART1（用于详细日志输出）
 *   - TX：PA9
 *   - RX：PA10
 * 
 * 功能演示：
 * 1. 上电复位测试（74+时钟脉冲，CMD0）
 * 2. 版本识别测试（CMD8，CMD58读取OCR）
 * 3. 初始化测试（ACMD41循环，识别SDHC/SDXC）
 * 4. 设备识别测试（CMD9读取CSD，CMD10读取CID）
 * 5. 状态查询测试（CMD13，R1/R2响应）
 * 6. 单块写入测试（CMD24，写入令牌0xFE，CRC校验）
 * 7. 单块读取测试（CMD17，起始令牌0xFE）
 * 8. 多块写入测试（CMD25，写入令牌0xFC，停止令牌0xFD）
 * 9. 多块读取测试（CMD18，CMD12停止）
 * 10. 数据验证测试（写入后回读对比）
 * 
 * @note 本示例使用TF_SPI模块的底层命令访问接口，展示SD协议底层细节
 * @note 实际应用应使用TF_SPI模块的高级API（如TF_SPI_Init()、TF_SPI_ReadBlock()等）
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "led.h"
#include "delay.h"
#include "oled_ssd1306.h"
#include "i2c_sw.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "spi_hw.h"
#include "tf_spi.h"
#include "gpio.h"
#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 测试块地址 */
#define TEST_BLOCK_ADDR    0x0000  /* 测试块地址（块0） */
#define TEST_BLOCK_COUNT   4       /* 多块测试的块数量 */

/* 辅助函数：计算校验和 */
static uint32_t CalculateChecksum(const uint8_t *buf, uint32_t len)
{
    uint32_t checksum = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
    {
        checksum += buf[i];
    }
    return checksum;
}

/* SD卡命令定义（用于示例演示）
 * 注意：命令值应该是0x00-0x3F，TF_SPI_SendCMD会在内部自动添加0x40（bit 6=1）
 */
#define SD_CMD_GO_IDLE_STATE       0x00    /* CMD0：复位卡（实际发送0x40） */
#define SD_CMD_SEND_IF_COND        0x08    /* CMD8：检查电压兼容性（实际发送0x48） */
#define SD_CMD_SEND_CSD            0x09    /* CMD9：读取CSD寄存器（实际发送0x49） */
#define SD_CMD_SEND_CID            0x0A    /* CMD10：读取CID寄存器（实际发送0x4A） */
#define SD_CMD_STOP_TRANSMISSION   0x0C    /* CMD12：停止传输（实际发送0x4C） */
#define SD_CMD_SEND_STATUS         0x0D    /* CMD13：发送状态（实际发送0x4D） */
#define SD_CMD_SET_BLOCKLEN        0x10    /* CMD16：设置块长度（实际发送0x50） */
#define SD_CMD_READ_SINGLE_BLOCK   0x11    /* CMD17：读取单个块（实际发送0x51） */
#define SD_CMD_READ_MULTIPLE_BLOCK 0x12    /* CMD18：读取多个块（实际发送0x52） */
#define SD_CMD_WRITE_BLOCK         0x18    /* CMD24：写入单个块（实际发送0x58） */
#define SD_CMD_WRITE_MULTIPLE_BLOCK 0x19   /* CMD25：写入多个块（实际发送0x59） */
#define SD_CMD_APP_CMD             0x37    /* CMD55：应用命令前缀（实际发送0x77） */
#define SD_CMD_READ_OCR            0x3A    /* CMD58：读取OCR寄存器（实际发送0x7A） */
#define SD_ACMD_SD_SEND_OP_COND    0x29    /* ACMD41：初始化SD卡（实际发送0x69） */

/* SD卡响应格式 */
#define SD_R1_IDLE_STATE           0x01    /* R1响应：空闲状态 */

/* SD卡数据令牌 */
#define SD_TOKEN_START_BLOCK       0xFE    /* 数据块开始令牌 */
#define SD_TOKEN_STOP_TRANSMISSION 0xFD   /* 停止传输令牌 */
#define SD_TOKEN_DATA_ACCEPTED     0x05    /* 数据接受令牌 */

/* SD卡块大小 */
#define SD_BLOCK_SIZE              512     /* SD卡块大小（字节） */

/* ==================== 辅助函数 ==================== */

/**
 * @brief 发送应用命令（ACMD）
 * @param[in] cmd 应用命令字节
 * @param[in] arg 命令参数
 * @param[out] response R1响应值
 * @return TF_SPI_Status_t 错误码
 */
static TF_SPI_Status_t tf_example_send_acmd(uint8_t cmd, uint32_t arg, uint8_t *response)
{
    TF_SPI_Status_t status;
    uint8_t r1;
    
    /* 先发送CMD55（应用命令前缀） */
    status = TF_SPI_SendCMD(SD_CMD_APP_CMD, 0, &r1);
    if (status != TF_SPI_OK || r1 != 0x00)
    {
        if (response != NULL)
        {
            *response = r1;
        }
        return (status != TF_SPI_OK) ? status : TF_SPI_ERROR_CMD_FAILED;
    }
    
    /* 再发送实际的应用命令 */
    status = TF_SPI_SendCMD(cmd, arg, &r1);
    if (response != NULL)
    {
        *response = r1;
    }
    
    return status;
}

/**
 * @brief 解析CSD寄存器（CSD版本1.0 - SDSC）
 * @param[in] csd CSD寄存器数据（16字节）
 * @param[out] capacity_mb 容量（MB）
 * @param[out] block_size 块大小（字节）
 * @param[out] block_count 块数量
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t tf_example_parse_csd_v1(const uint8_t *csd, uint32_t *capacity_mb, uint32_t *block_size, uint32_t *block_count)
{
    uint32_t c_size;
    uint8_t c_size_mult;
    uint32_t read_bl_len;
    uint64_t capacity_bytes_64;
    
    if (csd == NULL || capacity_mb == NULL || block_size == NULL || block_count == NULL)
    {
        return 0;
    }
    
    /* 计算C_SIZE */
    c_size = ((uint32_t)(csd[6] & 0x03) << 10) | ((uint32_t)csd[7] << 2) | ((csd[8] >> 6) & 0x03);
    
    /* 计算C_SIZE_MULT */
    c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] >> 7) & 0x01);
    
    /* 计算READ_BL_LEN */
    read_bl_len = csd[5] & 0x0F;
    
    /* 容量 = (C_SIZE + 1) * 512 * 2^(C_SIZE_MULT + 2) 字节 */
    capacity_bytes_64 = ((uint64_t)(c_size + 1)) * (1ULL << (c_size_mult + 2)) * (1ULL << read_bl_len);
    
    /* 检查是否超出32位范围 */
    if (capacity_bytes_64 > UINT32_MAX)
    {
        return 0;
    }
    
    *capacity_mb = (uint32_t)(capacity_bytes_64 / (1024UL * 1024UL));
    *block_size = 512;
    *block_count = (uint32_t)(capacity_bytes_64 / 512);
    
    return 1;
}

/**
 * @brief 解析CSD寄存器（CSD版本2.0 - SDHC/SDXC）
 * @param[in] csd CSD寄存器数据（16字节）
 * @param[out] capacity_mb 容量（MB）
 * @param[out] block_size 块大小（字节）
 * @param[out] block_count 块数量
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t tf_example_parse_csd_v2(const uint8_t *csd, uint32_t *capacity_mb, uint32_t *block_size, uint32_t *block_count)
{
    uint32_t c_size;
    uint64_t capacity_bytes_64;
    
    if (csd == NULL || capacity_mb == NULL || block_size == NULL || block_count == NULL)
    {
        return 0;
    }
    
    /* 计算C_SIZE */
    c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
    
    /* 容量 = (C_SIZE + 1) * 512KB */
    capacity_bytes_64 = ((uint64_t)(c_size + 1)) * 512ULL * 1024ULL;
    
    /* 使用64位计算容量，然后转换为32位（如果超出32位范围则限制） */
    uint64_t capacity_mb_64 = capacity_bytes_64 / (1024ULL * 1024ULL);
    uint64_t block_count_64 = capacity_bytes_64 / 512ULL;
    
    /* 检查是否超出32位范围，如果超出则限制，但尽量显示正确值 */
    if (capacity_mb_64 > UINT32_MAX)
    {
        *capacity_mb = UINT32_MAX;
    }
    else
    {
        *capacity_mb = (uint32_t)capacity_mb_64;
    }
    
    if (block_count_64 > UINT32_MAX)
    {
        *block_count = UINT32_MAX;
    }
    else
    {
        *block_count = (uint32_t)block_count_64;
    }
    
    *block_size = 512;
    
    return 1;
}

/**
 * @brief 解析CID寄存器
 * @param[in] cid CID寄存器数据（16字节）
 * @param[out] manufacturer_id 厂商ID
 * @param[out] oem OEM ID（2字节字符串）
 * @param[out] product_name 产品名（5字节字符串）
 * @param[out] serial_number 序列号（4字节）
 * @return uint8_t 1-成功，0-失败
 */
static uint8_t tf_example_parse_cid(const uint8_t *cid, uint8_t *manufacturer_id, char *oem, char *product_name, uint32_t *serial_number)
{
    uint8_t i;
    
    if (cid == NULL || manufacturer_id == NULL || oem == NULL || product_name == NULL || serial_number == NULL)
    {
        return 0;
    }
    
    /* 厂商ID */
    *manufacturer_id = cid[0];
    
    /* OEM ID（2字节） */
    oem[0] = cid[1];
    oem[1] = cid[2];
    oem[2] = '\0';
    
    /* 产品名（5字节） */
    for (i = 0; i < 5; i++)
    {
        product_name[i] = cid[3 + i];
    }
    product_name[5] = '\0';
    
    /* 序列号（4字节） */
    *serial_number = ((uint32_t)cid[9] << 24) | ((uint32_t)cid[10] << 16) | 
                     ((uint32_t)cid[11] << 8) | cid[12];
    
    return 1;
}

/* ==================== 测试函数 ==================== */

/**
 * @brief 上电复位测试
 */
static void TestPowerOnReset(void)
{
    TF_SPI_Status_t status;
    uint8_t response;
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t dummy = 0xFF;
    uint8_t i;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Power On Reset");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 上电复位测试 ===");
    LOG_INFO("MAIN", "发送74+个时钟脉冲（10个0xFF）");
    
    /* 1. 上电复位：发送74+个时钟脉冲（CS拉高，发送10个0xFF） */
    /* 注意：CS必须拉高，然后发送dummy字节 */
    LOG_INFO("MAIN", "检查SPI初始化状态...");
    if (!SPI_IsInitialized(spi_instance))
    {
        LOG_ERROR("MAIN", "SPI2未初始化！");
        OLED_ShowString(2, 1, "SPI Not Init");
        Delay_ms(2000);
        return;
    }
    
    LOG_INFO("MAIN", "拉高CS（PA11）...");
    SPI_Status_t nss_status = SPI_NSS_High(spi_instance);
    if (nss_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "SPI_NSS_High失败: %d (可能SPI未初始化或CS引脚配置错误)", nss_status);
        OLED_ShowString(2, 1, "NSS High Fail");
        Delay_ms(2000);
        return;
    }
    LOG_INFO("MAIN", "CS已拉高（PA11=1）");
    
    /* 发送10个0xFF作为上电复位时钟脉冲 */
    LOG_INFO("MAIN", "发送10个dummy字节（0xFF）...");
    for (i = 0; i < 10; i++)
    {
        SPI_Status_t tx_status = SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        if (tx_status != SPI_OK)
        {
            LOG_WARN("MAIN", "Dummy字节发送失败: %d", tx_status);
        }
    }
    Delay_ms(10);  /* 等待卡稳定 */
    LOG_INFO("MAIN", "上电复位完成，等待10ms");
    
    LOG_INFO("MAIN", "发送CMD0进入SPI模式...");
    
    /* 2. 发送CMD0进入SPI模式 */
    /* 注意：TF_SPI_SendCMD内部会拉低CS，发送命令，然后拉高CS */
    status = TF_SPI_SendCMD(SD_CMD_GO_IDLE_STATE, 0, &response);
    
    LOG_INFO("MAIN", "CMD0状态: %d, R1响应: 0x%02X", status, response);
    
    if (response == 0xFF)
    {
        LOG_ERROR("MAIN", "CMD0超时或没有响应（0xFF）");
        LOG_ERROR("MAIN", "可能原因：");
        LOG_ERROR("MAIN", "  1. SD卡未插入或未上电");
        LOG_ERROR("MAIN", "  2. CS引脚（PA11）控制有问题");
        LOG_ERROR("MAIN", "  3. SPI通信问题（MISO/MOSI/SCK连接）");
        LOG_ERROR("MAIN", "  4. SPI时钟频率过高（应≤400kHz初始化）");
    }
    
    /* 解析R1响应错误位 */
    if (response != SD_R1_IDLE_STATE && response != 0xFF)
    {
        LOG_ERROR("MAIN", "R1错误位解析:");
        if (response & 0x80) LOG_ERROR("MAIN", "  - bit7: 未定义错误");
        if (response & 0x40) LOG_ERROR("MAIN", "  - bit6: 参数错误");
        if (response & 0x20) LOG_ERROR("MAIN", "  - bit5: 地址错误");
        if (response & 0x08) LOG_ERROR("MAIN", "  - bit3: CRC错误");
        if (response & 0x04) LOG_ERROR("MAIN", "  - bit2: 非法命令");
        if (response & 0x02) LOG_ERROR("MAIN", "  - bit1: 擦除复位");
        if (response & 0x01) LOG_ERROR("MAIN", "  - bit0: 空闲状态");
    }
    
    if (status == TF_SPI_OK && response == SD_R1_IDLE_STATE)
    {
        OLED_ShowString(2, 1, "CMD0: OK");
        OLED_ShowString(3, 1, "R1: 0x01");
        LOG_INFO("MAIN", "CMD0成功，R1响应: 0x%02X (IDLE_STATE)", response);
    }
    else
    {
        OLED_ShowString(2, 1, "CMD0: Failed");
        char buf[17];
        snprintf(buf, sizeof(buf), "R1: 0x%02X", response);
        OLED_ShowString(3, 1, buf);
        LOG_ERROR("MAIN", "CMD0失败，状态: %d, R1响应: 0x%02X", status, response);
        LOG_ERROR("MAIN", "可能原因：1. CS引脚未正确配置 2. SPI通信问题 3. 卡未插入");
    }
    
    /* 在CMD0之后，等待一段时间再发送CMD8 */
    LOG_INFO("MAIN", "等待100ms后发送CMD8...");
    Delay_ms(100);
}

/**
 * @brief 版本识别测试
 */
static void TestVersionIdentification(void)
{
    TF_SPI_Status_t status;
    uint8_t response;
    uint32_t ocr;
    uint8_t is_sd_v2 = 0;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Version ID Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 版本识别测试 ===");
    
    /* 发送CMD8（检查电压兼容性，SD卡v2.0+） */
    /* 注意：CMD8需要特殊处理，因为SD v2.0+会返回R7响应（5字节） */
    /* 而TF_SPI_SendCMD只返回R1响应，所以我们需要手动实现CMD8的发送和R7读取 */
    SPI_Instance_t spi_instance = TF_SPI_SPI_INSTANCE;
    uint8_t cmd8_buf[6];
    uint8_t r7[5];
    uint8_t dummy = 0xFF;
    uint32_t start_tick;
    uint8_t rx_data;
    SPI_Status_t spi_status;
    
    /* 构造CMD8命令包（6字节：命令+地址+CRC） */
    cmd8_buf[0] = (SD_CMD_SEND_IF_COND | 0x40);  /* 命令字节（bit 6=1） */
    cmd8_buf[1] = 0x00;  /* 参数高字节 */
    cmd8_buf[2] = 0x00;
    cmd8_buf[3] = 0x01;  /* 电压范围：2.7-3.6V */
    cmd8_buf[4] = 0xAA;  /* 检查模式：0xAA */
    cmd8_buf[5] = 0x87;  /* CMD8的CRC（参数0x1AA） */
    
    /* 发送CMD8命令 */
    LOG_INFO("MAIN", "发送CMD8命令...");
    
    /* 检查CS状态并拉低CS */
    SPI_Status_t nss_status = SPI_NSS_Low(spi_instance);
    if (nss_status != SPI_OK)
    {
        LOG_ERROR("MAIN", "SPI_NSS_Low失败: %d", nss_status);
        OLED_ShowString(2, 1, "NSS Low Fail");
        Delay_ms(2000);
        return;
    }
    
    /* 注意：CS拉低后，SD卡需要一些时间来准备接收命令 */
    /* 根据SD协议，CS拉低后应该等待至少1个时钟周期 */
    /* 发送一个dummy字节来同步 */
    dummy = 0xFF;
    SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
    
    spi_status = SPI_MasterTransmit(spi_instance, cmd8_buf, 6, 1000);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        LOG_ERROR("MAIN", "CMD8发送失败: %d", spi_status);
        OLED_ShowString(2, 1, "CMD8: TX Fail");
        Delay_ms(2000);
        return;
    }
    
    /* 等待R1响应（最多8字节） */
    /* 注意：SD卡在发送命令后需要一些时间来准备响应 */
    /* 我们需要发送dummy字节（0xFF）来接收响应 */
    /* SD协议规定：响应应该在命令发送后的1-8个字节内返回 */
    LOG_INFO("MAIN", "等待CMD8响应...");
    start_tick = Delay_GetTick();
    response = 0xFF;
    uint8_t retry_count = 0;
    uint32_t elapsed;
    
    /* 最多等待1000ms，最多尝试64次（确保有足够的时间） */
    while (retry_count < 64)
    {
        elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        if (elapsed >= 1000)
        {
            LOG_WARN("MAIN", "CMD8响应等待超时 (尝试次数: %d, 耗时: %d ms)", retry_count, elapsed);
            break;
        }
        
        rx_data = 0xFF;
        /* 使用超时参数0，表示使用SPI默认超时（与tf_spi_wait_response一致） */
        /* 这样可以确保与TF_SPI_Init中的实现一致 */
        spi_status = SPI_MasterTransmitReceive(spi_instance, &rx_data, &response, 1, 0);
        retry_count++;
        
        /* 每8次尝试记录一次状态 */
        if (retry_count % 8 == 0 || retry_count == 1)
        {
            LOG_INFO("MAIN", "CMD8响应等待: 尝试次数=%d, 耗时=%d ms, SPI状态=%d, 响应=0x%02X", 
                     retry_count, elapsed, spi_status, response);
        }
        
        if (spi_status == SPI_OK && response != 0xFF)
        {
            elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
            LOG_INFO("MAIN", "收到CMD8响应: 0x%02X (尝试次数: %d, 耗时: %d ms)", 
                     response, retry_count, elapsed);
            break;
        }
        
        /* 如果SPI通信失败，记录详细信息 */
        if (spi_status != SPI_OK)
        {
            if (retry_count <= 8 || retry_count % 8 == 0)
            {
                LOG_WARN("MAIN", "SPI通信失败: 状态=%d (尝试次数: %d, 耗时: %d ms)", 
                         spi_status, retry_count, elapsed);
            }
        }
    }
    
    if (response == 0xFF)
    {
        elapsed = Delay_GetElapsed(Delay_GetTick(), start_tick);
        LOG_ERROR("MAIN", "CMD8响应超时 (尝试次数: %d, 耗时: %d ms)", retry_count, elapsed);
        LOG_ERROR("MAIN", "可能原因：");
        LOG_ERROR("MAIN", "  1. SD卡是v1.0，不支持CMD8（应该返回0x05 ILLEGAL_CMD）");
        LOG_ERROR("MAIN", "  2. SD卡未正确初始化");
        LOG_ERROR("MAIN", "  3. SPI通信问题（MISO引脚连接）");
        LOG_WARN("MAIN", "建议：尝试使用TF_SPI_Init()来初始化SD卡，验证硬件连接");
        
        /* 如果CMD8失败，假设是SD v1.0，继续后续测试 */
        is_sd_v2 = 0;
        OLED_ShowString(2, 1, "CMD8: Timeout");
        OLED_ShowString(3, 1, "Assume v1.0");
        LOG_WARN("MAIN", "假设SD卡是v1.0，继续后续测试");
    }
    
    if (response == SD_R1_IDLE_STATE)
    {
        /* SD卡v2.0+，读取R7响应的剩余4字节（R1已经读取） */
        /* R7响应格式：R1(1字节) + 保留(1字节) + 保留(1字节) + 电压范围(1字节) + 检查模式(1字节) */
        /* 由于R1已经读取，这里读取剩余的4字节：保留+保留+电压+检查模式 */
        spi_status = SPI_MasterReceive(spi_instance, r7, 4, 1000);
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        
        if (spi_status == SPI_OK)
        {
            is_sd_v2 = 1;
            OLED_ShowString(2, 1, "CMD8: OK");
            OLED_ShowString(3, 1, "SD v2.0+");
            LOG_INFO("MAIN", "CMD8成功，R1响应: 0x%02X (IDLE_STATE)", response);
            LOG_INFO("MAIN", "R7响应: R1=0x%02X, 保留=0x%02X 0x%02X, 电压=0x%02X, 检查模式=0x%02X", 
                     response, r7[0], r7[1], r7[2], r7[3]);
            LOG_INFO("MAIN", "检测到SD卡v2.0+");
            
            /* 验证电压兼容性：r7[2]应为0x01，表示2.7-3.6V */
            if (r7[2] == 0x01)
            {
                LOG_INFO("MAIN", "电压兼容：2.7-3.6V");
            }
            else
            {
                /* 某些SD卡可能返回不同的电压值，只要不是0x00就认为兼容 */
                if (r7[2] != 0x00)
                {
                    LOG_INFO("MAIN", "电压范围：0x%02X (可能兼容)", r7[2]);
                }
                else
                {
                    LOG_WARN("MAIN", "电压不兼容：0x%02X", r7[2]);
                }
            }
            
            /* 验证检查模式：r7[3]应为0xAA */
            if (r7[3] == 0xAA)
            {
                LOG_INFO("MAIN", "检查模式正确：0xAA");
            }
            else
            {
                /* 某些SD卡可能返回0xFF（表示不支持检查模式），这是正常的 */
                if (r7[3] == 0xFF)
                {
                    LOG_INFO("MAIN", "检查模式：0xFF (SD卡不支持检查模式，这是正常的)");
                }
                else
                {
                    LOG_WARN("MAIN", "检查模式：0x%02X (期望0xAA或0xFF)", r7[3]);
                }
            }
        }
        else
        {
            LOG_ERROR("MAIN", "CMD8 R7读取失败: %d", spi_status);
            OLED_ShowString(2, 1, "CMD8: R7 Fail");
            Delay_ms(2000);
            return;
        }
    }
    else if (response == (SD_R1_IDLE_STATE | 0x04))  /* ILLEGAL_CMD = 0x05 */
    {
        /* SD卡v1.0，不支持CMD8 */
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        is_sd_v2 = 0;
        OLED_ShowString(2, 1, "CMD8: ILLEGAL");
        OLED_ShowString(3, 1, "SD v1.0");
        LOG_INFO("MAIN", "CMD8返回ILLEGAL_CMD (0x05)，检测到SD卡v1.0");
    }
    else
    {
        SPI_NSS_High(spi_instance);
        SPI_MasterTransmit(spi_instance, &dummy, 1, 100);
        OLED_ShowString(2, 1, "CMD8: Failed");
        LOG_ERROR("MAIN", "CMD8失败，R1响应: 0x%02X", response);
        if (response == 0xFF)
        {
            LOG_ERROR("MAIN", "CMD8超时，可能原因：");
            LOG_ERROR("MAIN", "  1. SD卡未正确初始化");
            LOG_ERROR("MAIN", "  2. SPI通信问题");
        }
        Delay_ms(2000);
        return;
    }
    
    /* 发送CMD58读取OCR */
    status = TF_SPI_ReadOCR(&ocr);
    if (status == TF_SPI_OK)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "OCR: 0x%08X", ocr);
        OLED_ShowString(4, 1, buf);
        LOG_INFO("MAIN", "OCR寄存器: 0x%08X", ocr);
        LOG_INFO("MAIN", "电压范围: 2.7-3.6V (bit 23-24: 0x%02X)", (ocr >> 23) & 0x03);
        
        if (is_sd_v2 && (ocr & 0x40000000))  /* CCS位（bit 30） */
        {
            LOG_INFO("MAIN", "卡类型: SDHC/SDXC (CCS=1)");
        }
        else if (is_sd_v2)
        {
            LOG_INFO("MAIN", "卡类型: SDSC v2.0 (CCS=0)");
        }
        else
        {
            LOG_INFO("MAIN", "卡类型: SDSC v1.0");
        }
    }
    else
    {
        OLED_ShowString(4, 1, "OCR: Failed");
        LOG_ERROR("MAIN", "读取OCR失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 初始化测试
 */
static void TestInitialization(void)
{
    TF_SPI_Status_t status;
    const tf_spi_dev_t *dev_info;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Init Test");
    OLED_ShowString(2, 1, "Using TF_SPI_Init");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 初始化测试 ===");
    LOG_INFO("MAIN", "使用TF_SPI_Init()来初始化SD卡，验证硬件连接");
    LOG_INFO("MAIN", "TF_SPI_Init()会执行完整的初始化流程：");
    LOG_INFO("MAIN", "  1. 上电复位（74+时钟脉冲，CMD0）");
    LOG_INFO("MAIN", "  2. 版本识别（CMD8，读取R7响应）");
    LOG_INFO("MAIN", "  3. 初始化（ACMD41循环，等待卡就绪）");
    LOG_INFO("MAIN", "  4. 读取OCR（CMD58，确认初始化完成）");
    LOG_INFO("MAIN", "  5. 读取CSD（CMD9，获取容量信息）");
    
    /* 使用TF_SPI_Init来初始化SD卡 */
    /* 这个函数会执行完整的初始化流程，可以验证硬件连接是否正常 */
    status = TF_SPI_Init();
    
    if (status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Init: OK");
        LOG_INFO("MAIN", "TF_SPI_Init()成功！硬件连接正常");
        
        /* 获取设备信息 */
        dev_info = TF_SPI_GetInfo();
        if (dev_info != NULL)
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "Cap: %d MB", dev_info->capacity_mb);
            OLED_ShowString(3, 1, buf);
            
            LOG_INFO("MAIN", "SD卡信息：");
            LOG_INFO("MAIN", "  容量: %d MB", dev_info->capacity_mb);
            LOG_INFO("MAIN", "  块大小: %d 字节", dev_info->block_size);
            LOG_INFO("MAIN", "  块数量: %d", dev_info->block_count);
            LOG_INFO("MAIN", "  卡类型: %s", 
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDSC ? "SDSC" :
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDHC ? "SDHC" :
                     dev_info->card_type == TF_SPI_CARD_TYPE_SDXC ? "SDXC" : "Unknown");
            LOG_INFO("MAIN", "  SDHC/SDXC: %s", dev_info->is_sdhc ? "是" : "否");
            
            /* 显示卡类型 */
            if (dev_info->card_type == TF_SPI_CARD_TYPE_SDHC)
            {
                OLED_ShowString(4, 1, "Type: SDHC");
            }
            else if (dev_info->card_type == TF_SPI_CARD_TYPE_SDXC)
            {
                OLED_ShowString(4, 1, "Type: SDXC");
            }
            else if (dev_info->card_type == TF_SPI_CARD_TYPE_SDSC)
            {
                OLED_ShowString(4, 1, "Type: SDSC");
            }
        }
        else
        {
            LOG_WARN("MAIN", "无法获取设备信息");
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Init: Failed");
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "Error: %d", status);
        OLED_ShowString(3, 1, err_buf);
        
        LOG_ERROR("MAIN", "TF_SPI_Init()失败，状态: %d", status);
        LOG_ERROR("MAIN", "可能原因：");
        LOG_ERROR("MAIN", "  1. SD卡未插入或未上电");
        LOG_ERROR("MAIN", "  2. SPI引脚连接问题（特别是MISO/PB14）");
        LOG_ERROR("MAIN", "  3. CS引脚（PA11）控制问题");
        LOG_ERROR("MAIN", "  4. SD卡损坏或不兼容");
        LOG_ERROR("MAIN", "  5. SPI时钟频率问题（初始化时应≤400kHz）");
    }
    
    Delay_ms(3000);
}

/**
 * @brief 设备识别测试
 */
static void TestDeviceIdentification(void)
{
    TF_SPI_Status_t status;
    uint8_t csd[16];
    uint8_t cid[16];
    uint8_t csd_structure;
    uint32_t capacity_mb, block_size, block_count;
    uint8_t manufacturer_id;
    char oem[3];
    char product_name[6];
    uint32_t serial_number;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Device ID Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 设备识别测试 ===");
    
    /* 读取CSD寄存器 */
    status = TF_SPI_ReadCSD(csd);
    if (status == TF_SPI_OK)
    {
        csd_structure = (csd[0] >> 6) & 0x03;
        LOG_INFO("MAIN", "CSD结构版本: %d", csd_structure);
        
        if (csd_structure == 0)
        {
            /* CSD版本1.0（SDSC） */
            if (tf_example_parse_csd_v1(csd, &capacity_mb, &block_size, &block_count))
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Cap: %dMB", capacity_mb);
                OLED_ShowString(2, 1, buf);
                LOG_INFO("MAIN", "容量: %d MB", capacity_mb);
                LOG_INFO("MAIN", "块大小: %d 字节", block_size);
                LOG_INFO("MAIN", "块数量: %d", block_count);
            }
        }
        else if (csd_structure == 1)
        {
            /* CSD版本2.0（SDHC/SDXC） */
            if (tf_example_parse_csd_v2(csd, &capacity_mb, &block_size, &block_count))
            {
                char buf[17];
                snprintf(buf, sizeof(buf), "Cap: %dMB", capacity_mb);
                OLED_ShowString(2, 1, buf);
                LOG_INFO("MAIN", "容量: %d MB", capacity_mb);
                LOG_INFO("MAIN", "块大小: %d 字节", block_size);
                LOG_INFO("MAIN", "块数量: %d", block_count);
            }
        }
    }
    else
    {
        OLED_ShowString(2, 1, "CSD: Failed");
        LOG_ERROR("MAIN", "读取CSD失败，状态: %d", status);
    }
    
    /* 读取CID寄存器 */
    status = TF_SPI_ReadCID(cid);
    if (status == TF_SPI_OK)
    {
        if (tf_example_parse_cid(cid, &manufacturer_id, oem, product_name, &serial_number))
        {
            char buf[17];
            snprintf(buf, sizeof(buf), "MID: 0x%02X", manufacturer_id);
            OLED_ShowString(3, 1, buf);
            LOG_INFO("MAIN", "厂商ID: 0x%02X", manufacturer_id);
            LOG_INFO("MAIN", "OEM: %s", oem);
            LOG_INFO("MAIN", "产品名: %s", product_name);
            LOG_INFO("MAIN", "序列号: 0x%08X", serial_number);
        }
    }
    else
    {
        OLED_ShowString(3, 1, "CID: Failed");
        LOG_ERROR("MAIN", "读取CID失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 状态查询测试
 */
static void TestStatusQuery(void)
{
    TF_SPI_Status_t status;
    uint8_t card_status;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Status Query");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 状态查询测试 ===");
    LOG_INFO("MAIN", "发送CMD13查询卡状态");
    
    status = TF_SPI_SendStatus(&card_status);
    if (status == TF_SPI_OK)
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Status: 0x%02X", card_status);
        OLED_ShowString(2, 1, buf);
        LOG_INFO("MAIN", "卡状态: 0x%02X", card_status);
        
        /* 解析状态位 */
        if (card_status & 0x01)
        {
            LOG_INFO("MAIN", "状态位: IDLE_STATE (bit 0)");
        }
        if (card_status & 0x02)
        {
            LOG_INFO("MAIN", "状态位: ERASE_RESET (bit 1)");
        }
        if (card_status & 0x04)
        {
            LOG_INFO("MAIN", "状态位: ILLEGAL_CMD (bit 2)");
        }
        if (card_status & 0x08)
        {
            LOG_INFO("MAIN", "状态位: CRC_ERROR (bit 3)");
        }
        if (card_status & 0x10)
        {
            LOG_INFO("MAIN", "状态位: ERASE_SEQ_ERROR (bit 4)");
        }
        if (card_status & 0x20)
        {
            LOG_INFO("MAIN", "状态位: ADDRESS_ERROR (bit 5)");
        }
        if (card_status & 0x40)
        {
            LOG_INFO("MAIN", "状态位: PARAMETER_ERROR (bit 6)");
        }
        
        if (card_status == 0x00)
        {
            OLED_ShowString(3, 1, "Card Ready");
            LOG_INFO("MAIN", "卡状态: 就绪 (0x00)");
        }
    }
    else
    {
        OLED_ShowString(2, 1, "Status: Failed");
        LOG_ERROR("MAIN", "状态查询失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 单块写入测试
 * @note 由于需要直接操作SPI进行数据写入，这里使用TF_SPI模块的高级API
 */
static void TestSingleBlockWrite(void)
{
    TF_SPI_Status_t status;
    uint8_t write_buf[SD_BLOCK_SIZE];
    uint32_t i;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Single Write");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 单块写入测试 ===");
    LOG_INFO("MAIN", "写入块地址: 0x%04X", TEST_BLOCK_ADDR);
    
    /* 准备测试数据（递增序列） */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    /* 注意：这里使用TF_SPI模块的高级API进行写入 */
    /* 如果要展示底层SD协议，需要直接操作SPI，但为了代码简洁，这里使用高级API */
    status = TF_SPI_WriteBlock(TEST_BLOCK_ADDR, write_buf);
    
    if (status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Write: OK");
        LOG_INFO("MAIN", "单块写入成功");
    }
    else
    {
        OLED_ShowString(2, 1, "Write: Failed");
        LOG_ERROR("MAIN", "单块写入失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 单块读取测试
 */
static void TestSingleBlockRead(void)
{
    TF_SPI_Status_t status;
    uint8_t read_buf[SD_BLOCK_SIZE];
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Single Read");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 单块读取测试 ===");
    LOG_INFO("MAIN", "读取块地址: 0x%04X", TEST_BLOCK_ADDR);
    
    status = TF_SPI_ReadBlock(TEST_BLOCK_ADDR, read_buf);
    
    if (status == TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Read: OK");
        LOG_INFO("MAIN", "单块读取成功");
        LOG_INFO("MAIN", "前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                 read_buf[4], read_buf[5], read_buf[6], read_buf[7],
                 read_buf[8], read_buf[9], read_buf[10], read_buf[11],
                 read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
    }
    else
    {
        OLED_ShowString(2, 1, "Read: Failed");
        LOG_ERROR("MAIN", "单块读取失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 多块写入测试
 */
static void TestMultiBlockWrite(void)
{
    TF_SPI_Status_t status;
    static uint8_t write_buf[TEST_BLOCK_COUNT * SD_BLOCK_SIZE];  /* 使用static避免栈溢出 */
    uint32_t i;
    uint32_t start_tick, elapsed_ms;
    uint32_t checksum;
    uint32_t total_bytes = TEST_BLOCK_COUNT * SD_BLOCK_SIZE;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Multi Write");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 多块写入测试 ===");
    LOG_INFO("MAIN", "写入块地址: 0x%04X, 块数量: %d (%d KB)", 
             TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, total_bytes / 1024);
    
    /* 准备测试数据（每个块使用不同的模式） */
    for (i = 0; i < TEST_BLOCK_COUNT * SD_BLOCK_SIZE; i++)
    {
        uint32_t block_idx = i / SD_BLOCK_SIZE;
        write_buf[i] = (uint8_t)(0xAA + (block_idx & 0x0F));  /* 每个块使用不同的值 */
    }
    
    checksum = CalculateChecksum(write_buf, total_bytes);
    LOG_INFO("MAIN", "写入数据校验和: 0x%08X", checksum);
    
    /* 测量写入时间 */
    start_tick = Delay_GetTick();
    status = TF_SPI_WriteBlocks(TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, write_buf);
    elapsed_ms = Delay_GetElapsed(Delay_GetTick(), start_tick);
    
    if (status == TF_SPI_OK)
    {
        char buf[17];
        uint32_t speed_kbps = (total_bytes * 1000) / (elapsed_ms > 0 ? elapsed_ms : 1);
        snprintf(buf, sizeof(buf), "Write: OK (%d)", TEST_BLOCK_COUNT);
        OLED_ShowString(2, 1, buf);
        LOG_INFO("MAIN", "多块写入成功，块数量: %d", TEST_BLOCK_COUNT);
        LOG_INFO("MAIN", "写入时间: %lu ms, 速度: %lu KB/s", elapsed_ms, speed_kbps);
        LOG_INFO("MAIN", "第一块前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 write_buf[0], write_buf[1], write_buf[2], write_buf[3],
                 write_buf[4], write_buf[5], write_buf[6], write_buf[7],
                 write_buf[8], write_buf[9], write_buf[10], write_buf[11],
                 write_buf[12], write_buf[13], write_buf[14], write_buf[15]);
    }
    else
    {
        OLED_ShowString(2, 1, "Write: Failed");
        LOG_ERROR("MAIN", "多块写入失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 多块读取测试
 */
static void TestMultiBlockRead(void)
{
    TF_SPI_Status_t status;
    static uint8_t read_buf[TEST_BLOCK_COUNT * SD_BLOCK_SIZE];  /* 使用static避免栈溢出 */
    uint32_t start_tick, elapsed_ms;
    uint32_t checksum;
    uint32_t total_bytes = TEST_BLOCK_COUNT * SD_BLOCK_SIZE;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Multi Read");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 多块读取测试 ===");
    LOG_INFO("MAIN", "读取块地址: 0x%04X, 块数量: %d (%d KB)", 
             TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, total_bytes / 1024);
    
    /* 测量读取时间 */
    start_tick = Delay_GetTick();
    status = TF_SPI_ReadBlocks(TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, read_buf);
    elapsed_ms = Delay_GetElapsed(Delay_GetTick(), start_tick);
    
    if (status == TF_SPI_OK)
    {
        char buf[17];
        uint32_t speed_kbps = (total_bytes * 1000) / (elapsed_ms > 0 ? elapsed_ms : 1);
        checksum = CalculateChecksum(read_buf, total_bytes);
        snprintf(buf, sizeof(buf), "Read: OK (%d)", TEST_BLOCK_COUNT);
        OLED_ShowString(2, 1, buf);
        LOG_INFO("MAIN", "多块读取成功，块数量: %d", TEST_BLOCK_COUNT);
        LOG_INFO("MAIN", "读取时间: %lu ms, 速度: %lu KB/s", elapsed_ms, speed_kbps);
        LOG_INFO("MAIN", "读取数据校验和: 0x%08X", checksum);
        LOG_INFO("MAIN", "第一块前16字节: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                 read_buf[4], read_buf[5], read_buf[6], read_buf[7],
                 read_buf[8], read_buf[9], read_buf[10], read_buf[11],
                 read_buf[12], read_buf[13], read_buf[14], read_buf[15]);
    }
    else
    {
        OLED_ShowString(2, 1, "Read: Failed");
        LOG_ERROR("MAIN", "多块读取失败，状态: %d", status);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 数据验证测试
 */
static void TestDataVerification(void)
{
    TF_SPI_Status_t status;
    uint8_t write_buf[SD_BLOCK_SIZE];
    uint8_t read_buf[SD_BLOCK_SIZE];
    uint32_t i;
    uint32_t error_count = 0;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Verify Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 数据验证测试 ===");
    LOG_INFO("MAIN", "写入块地址: 0x%04X", TEST_BLOCK_ADDR);
    
    /* 准备测试数据（递增序列） */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }
    
    uint32_t write_checksum = CalculateChecksum(write_buf, SD_BLOCK_SIZE);
    LOG_INFO("MAIN", "写入数据校验和: 0x%08X", write_checksum);
    
    /* 写入数据 */
    status = TF_SPI_WriteBlock(TEST_BLOCK_ADDR, write_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Write Failed");
        LOG_ERROR("MAIN", "写入失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    LOG_INFO("MAIN", "写入成功，开始读取验证");
    
    /* 读取数据 */
    status = TF_SPI_ReadBlock(TEST_BLOCK_ADDR, read_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Read Failed");
        LOG_ERROR("MAIN", "读取失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    uint32_t read_checksum = CalculateChecksum(read_buf, SD_BLOCK_SIZE);
    LOG_INFO("MAIN", "读取数据校验和: 0x%08X", read_checksum);
    
    /* 对比数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (write_buf[i] != read_buf[i])
        {
            error_count++;
            if (error_count <= 5)  /* 只记录前5个错误 */
            {
                LOG_ERROR("MAIN", "数据不匹配，位置 %d: 写入=0x%02X, 读取=0x%02X", 
                         i, write_buf[i], read_buf[i]);
            }
        }
    }
    
    if (error_count == 0)
    {
        OLED_ShowString(2, 1, "Verify: OK");
        LOG_INFO("MAIN", "数据验证成功，512字节全部匹配");
        LOG_INFO("MAIN", "校验和匹配: 0x%08X", write_checksum);
    }
    else
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Error: %d", error_count);
        OLED_ShowString(2, 1, buf);
        LOG_ERROR("MAIN", "数据验证失败，错误字节数: %d/%d", error_count, SD_BLOCK_SIZE);
        if (error_count > 5)
        {
            LOG_ERROR("MAIN", "（仅显示前5个错误，实际错误数: %d）", error_count);
        }
    }
    
    Delay_ms(2000);
}

/**
 * @brief 多块数据验证测试
 */
static void TestMultiBlockVerification(void)
{
    TF_SPI_Status_t status;
    static uint8_t write_buf[TEST_BLOCK_COUNT * SD_BLOCK_SIZE];  /* 使用static避免栈溢出 */
    static uint8_t read_buf[TEST_BLOCK_COUNT * SD_BLOCK_SIZE];  /* 使用static避免栈溢出 */
    uint32_t i;
    uint32_t error_count = 0;
    uint32_t write_checksum, read_checksum;
    uint32_t total_bytes = TEST_BLOCK_COUNT * SD_BLOCK_SIZE;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Multi Verify");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 多块数据验证测试 ===");
    LOG_INFO("MAIN", "测试块地址: 0x%04X, 块数量: %d (%d KB)", 
             TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, total_bytes / 1024);
    
    /* 准备测试数据（每个块使用不同的模式） */
    for (i = 0; i < total_bytes; i++)
    {
        uint32_t block_idx = i / SD_BLOCK_SIZE;
        uint32_t offset = i % SD_BLOCK_SIZE;
        write_buf[i] = (uint8_t)((block_idx * 0x10 + offset) & 0xFF);
    }
    
    write_checksum = CalculateChecksum(write_buf, total_bytes);
    LOG_INFO("MAIN", "写入数据校验和: 0x%08X", write_checksum);
    
    /* 写入数据 */
    status = TF_SPI_WriteBlocks(TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, write_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Write Failed");
        LOG_ERROR("MAIN", "多块写入失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    LOG_INFO("MAIN", "多块写入成功，开始读取验证");
    
    /* 读取数据 */
    status = TF_SPI_ReadBlocks(TEST_BLOCK_ADDR, TEST_BLOCK_COUNT, read_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Read Failed");
        LOG_ERROR("MAIN", "多块读取失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    read_checksum = CalculateChecksum(read_buf, total_bytes);
    LOG_INFO("MAIN", "读取数据校验和: 0x%08X", read_checksum);
    
    /* 对比数据 */
    for (i = 0; i < total_bytes; i++)
    {
        if (write_buf[i] != read_buf[i])
        {
            error_count++;
            if (error_count <= 5)  /* 只记录前5个错误 */
            {
                uint32_t block_idx = i / SD_BLOCK_SIZE;
                uint32_t offset = i % SD_BLOCK_SIZE;
                LOG_ERROR("MAIN", "数据不匹配，块 %d, 偏移 %d: 写入=0x%02X, 读取=0x%02X", 
                         block_idx, offset, write_buf[i], read_buf[i]);
            }
        }
    }
    
    if (error_count == 0)
    {
        OLED_ShowString(2, 1, "Verify: OK");
        LOG_INFO("MAIN", "多块数据验证成功，%d 字节全部匹配", total_bytes);
        LOG_INFO("MAIN", "校验和匹配: 0x%08X", write_checksum);
    }
    else
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Error: %d", error_count);
        OLED_ShowString(2, 1, buf);
        LOG_ERROR("MAIN", "多块数据验证失败，错误字节数: %d/%d", error_count, total_bytes);
        if (error_count > 5)
        {
            LOG_ERROR("MAIN", "（仅显示前5个错误，实际错误数: %d）", error_count);
        }
    }
    
    Delay_ms(2000);
}

/**
 * @brief 边界测试（测试最后一个块）
 */
static void TestBoundaryTest(void)
{
    TF_SPI_Status_t status;
    const tf_spi_dev_t *dev_info;
    uint8_t write_buf[SD_BLOCK_SIZE];
    uint8_t read_buf[SD_BLOCK_SIZE];
    uint32_t last_block;
    uint32_t i;
    uint32_t error_count = 0;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "Boundary Test");
    Delay_ms(500);
    
    LOG_INFO("MAIN", "=== 边界测试 ===");
    
    dev_info = TF_SPI_GetInfo();
    if (dev_info == NULL)
    {
        OLED_ShowString(2, 1, "Not Init");
        LOG_ERROR("MAIN", "TF_SPI未初始化");
        Delay_ms(2000);
        return;
    }
    
    last_block = dev_info->block_count - 1;
    LOG_INFO("MAIN", "卡容量: %lu MB, 块数量: %lu", dev_info->capacity_mb, dev_info->block_count);
    LOG_INFO("MAIN", "测试最后一个块: 块地址 %lu (0x%08lX)", last_block, last_block);
    
    /* 准备测试数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        write_buf[i] = (uint8_t)(0xFF - (i & 0xFF));  /* 递减模式 */
    }
    
    /* 写入最后一个块 */
    status = TF_SPI_WriteBlock(last_block, write_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Write Failed");
        LOG_ERROR("MAIN", "写入最后一个块失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    LOG_INFO("MAIN", "写入最后一个块成功");
    
    /* 读取最后一个块 */
    status = TF_SPI_ReadBlock(last_block, read_buf);
    if (status != TF_SPI_OK)
    {
        OLED_ShowString(2, 1, "Read Failed");
        LOG_ERROR("MAIN", "读取最后一个块失败，状态: %d", status);
        Delay_ms(2000);
        return;
    }
    
    /* 对比数据 */
    for (i = 0; i < SD_BLOCK_SIZE; i++)
    {
        if (write_buf[i] != read_buf[i])
        {
            error_count++;
            if (error_count == 1)
            {
                LOG_ERROR("MAIN", "数据不匹配，位置 %d: 写入=0x%02X, 读取=0x%02X", 
                         i, write_buf[i], read_buf[i]);
            }
        }
    }
    
    if (error_count == 0)
    {
        OLED_ShowString(2, 1, "Boundary: OK");
        LOG_INFO("MAIN", "边界测试成功，最后一个块读写正常");
    }
    else
    {
        char buf[17];
        snprintf(buf, sizeof(buf), "Error: %d", error_count);
        OLED_ShowString(2, 1, buf);
        LOG_ERROR("MAIN", "边界测试失败，错误字节数: %d/%d", error_count, SD_BLOCK_SIZE);
    }
    
    Delay_ms(2000);
}

/**
 * @brief 主函数
 */
int main(void)
{
    SPI_Status_t spi_status;
    SoftI2C_Status_t i2c_status;
    UART_Status_t uart_status;
    OLED_Status_t oled_status;
    Log_Status_t log_status;
    int debug_status;
    log_config_t log_config;
    
    /* ========== 步骤1：系统初始化 ========== */
    System_Init();
    
    /* ========== 步骤2：UART初始化 ========== */
    uart_status = UART_Init(UART_INSTANCE_1);
    if (uart_status != UART_OK)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤3：Debug模块初始化 ========== */
    debug_status = Debug_Init(DEBUG_MODE_UART, 115200);
    if (debug_status != 0)
    {
        while(1) { Delay_ms(1000); }
    }
    Delay_ms(100);
    
    /* ========== 步骤4：Log模块初始化 ========== */
    memset(&log_config, 0, sizeof(log_config));
    log_config.level = LOG_LEVEL_INFO;
    log_config.enable_timestamp = 0;
    log_config.enable_module = 1;
    log_config.enable_color = 0;
    log_status = Log_Init(&log_config);
    if (log_status != LOG_OK)
    {
        ErrorHandler_Handle(log_status, "LOG");
    }
    
    /* ========== 步骤5：输出初始化信息 ========== */
    LOG_INFO("MAIN", "=== TF卡（MicroSD卡）SPI读写示例初始化 ===");
    LOG_INFO("MAIN", "UART1 已初始化: PA9(TX), PA10(RX), 115200");
    LOG_INFO("MAIN", "Debug 模块已初始化: UART 模式");
    LOG_INFO("MAIN", "Log 模块已初始化");
    
    /* ========== 步骤6：LED初始化 ========== */
    if (LED_Init() != LED_OK)
    {
        LOG_ERROR("MAIN", "LED 初始化失败");
        while(1) { Delay_ms(1000); }
    }
    
    /* ========== 步骤7：软件I2C初始化（OLED需要） ========== */
    i2c_status = I2C_SW_Init(SOFT_I2C_INSTANCE_1);
    if (i2c_status != SOFT_I2C_OK)
    {
        LOG_ERROR("MAIN", "软件I2C 初始化失败: %d", i2c_status);
        ErrorHandler_Handle(i2c_status, "SOFT_I2C");
    }
    else
    {
        LOG_INFO("MAIN", "软件I2C 已初始化: PB8(SCL), PB9(SDA)");
    }
    
    /* ========== 步骤8：OLED初始化 ========== */
    oled_status = OLED_Init();
    if (oled_status != OLED_OK)
    {
        LOG_ERROR("MAIN", "OLED 初始化失败: %d", oled_status);
        ErrorHandler_Handle(oled_status, "OLED");
    }
    else
    {
        OLED_Clear();
        OLED_ShowString(1, 1, "TF Card Demo");
        OLED_ShowString(2, 1, "Initializing...");
        LOG_INFO("MAIN", "OLED 已初始化");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤9：SPI初始化 ========== */
    OLED_ShowString(3, 1, "Init SPI2...");
    
    /* 手动配置PA11为GPIO输出（软件NSS模式，SPI驱动不会自动配置） */
    GPIO_EnableClock(GPIOA);
    GPIO_Config(GPIOA, GPIO_Pin_11, GPIO_MODE_OUTPUT_PP, GPIO_SPEED_50MHz);
    GPIO_WritePin(GPIOA, GPIO_Pin_11, Bit_SET);  /* NSS默认拉高（不选中） */
    
    spi_status = SPI_HW_Init(SPI_INSTANCE_2);
    if (spi_status != SPI_OK)
    {
        char err_buf[17];
        snprintf(err_buf, sizeof(err_buf), "SPI Fail:%d", spi_status);
        OLED_ShowString(4, 1, err_buf);
        LOG_ERROR("MAIN", "SPI2 初始化失败: %d", spi_status);
        ErrorHandler_Handle(spi_status, "SPI");
        while(1) { Delay_ms(1000); }
    }
    else
    {
        OLED_ShowString(4, 1, "SPI2: OK");
        LOG_INFO("MAIN", "SPI2 已初始化: PB13(SCK), PB14(MISO), PB15(MOSI), PA11(CS)");
    }
    
    Delay_ms(500);
    
    /* ========== 步骤10：执行测试函数 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "TF Card Tests");
    OLED_ShowString(2, 1, "Starting...");
    Delay_ms(1000);
    
    LOG_INFO("MAIN", "=== 开始执行测试函数 ===");
    
    /* 执行所有测试函数 */
    TestPowerOnReset();
    TestVersionIdentification();
    TestInitialization();
    TestDeviceIdentification();
    TestStatusQuery();
    TestSingleBlockWrite();
    TestSingleBlockRead();
    TestMultiBlockWrite();
    TestMultiBlockRead();
    TestDataVerification();
    TestMultiBlockVerification();
    TestBoundaryTest();
    
    LOG_INFO("MAIN", "=== 所有测试完成 ===");
    
    /* ========== 步骤11：主循环 ========== */
    OLED_Clear();
    OLED_ShowString(1, 1, "All Tests Done");
    OLED_ShowString(2, 1, "LED Blinking");
    
    while (1)
    {
        LED_Toggle(LED_1);
        Delay_ms(500);
    }
}

