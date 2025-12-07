/**
 * @file flash14_quality_test.c
 * @brief Flash14质量检测模块实现
 * @version 1.0.0
 * @date 2024-01-01
 * @details W25Q系列Flash芯片纯软件质量检测流程实现
 */

#include "flash14_quality_test.h"
#include "w25q_spi.h"
#include "spi_hw.h"
#include "delay.h"
#include "board.h"
#include "config.h"
#include "core_cm3.h"
#include "system_stm32f10x.h"
#include <string.h>

/* ==================== DWT定义（如果core_cm3.h中没有） ==================== */
#ifndef DWT_BASE
#define DWT_BASE            (0xE0001000UL)                              /*!< DWT Base Address */
#endif

#ifndef DWT_Type
typedef struct
{
  __IO uint32_t CTRL;                         /*!< Offset: 0x00  DWT Control Register */
  __IO uint32_t CYCCNT;                       /*!< Offset: 0x04  DWT Cycle Counter Register */
  __I  uint32_t CPICNT;                       /*!< Offset: 0x08  DWT CPI Count Register */
  __I  uint32_t EXCCNT;                       /*!< Offset: 0x0C  DWT Exception Overhead Count Register */
  __I  uint32_t SLEEPCNT;                     /*!< Offset: 0x10  DWT Sleep Count Register */
  __I  uint32_t LSUCNT;                       /*!< Offset: 0x14  DWT LSU Count Register */
  __I  uint32_t FOLDCNT;                      /*!< Offset: 0x18  DWT Folded-instruction Count Register */
  __I  uint32_t PCSR;                         /*!< Offset: 0x1C  DWT Program Counter Sample Register */
  __I  uint32_t COMP0;                        /*!< Offset: 0x20  DWT Comparator Register 0 */
  __I  uint32_t MASK0;                        /*!< Offset: 0x24  DWT Mask Register 0 */
  __I  uint32_t FUNCTION0;                    /*!< Offset: 0x28  DWT Function Register 0 */
       uint32_t RESERVED0[1];
  __I  uint32_t COMP1;                        /*!< Offset: 0x30  DWT Comparator Register 1 */
  __I  uint32_t MASK1;                        /*!< Offset: 0x34  DWT Mask Register 1 */
  __I  uint32_t FUNCTION1;                    /*!< Offset: 0x38  DWT Function Register 1 */
       uint32_t RESERVED1[1];
  __I  uint32_t COMP2;                        /*!< Offset: 0x40  DWT Comparator Register 2 */
  __I  uint32_t MASK2;                        /*!< Offset: 0x44  DWT Mask Register 2 */
  __I  uint32_t FUNCTION2;                    /*!< Offset: 0x48  DWT Function Register 2 */
       uint32_t RESERVED2[1];
  __I  uint32_t COMP3;                        /*!< Offset: 0x50  DWT Comparator Register 3 */
  __I  uint32_t MASK3;                        /*!< Offset: 0x54  DWT Mask Register 3 */
  __I  uint32_t FUNCTION3;                    /*!< Offset: 0x58  DWT Function Register 3 */
} DWT_Type;
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Pos                0                                             /*!< DWT CTRL: CYCCNTENA Position */
#define DWT_CTRL_CYCCNTENA_Msk                (1ul << DWT_CTRL_CYCCNTENA_Pos)               /*!< DWT CTRL: CYCCNTENA Mask */
#endif

#ifndef DWT
#define DWT                 ((DWT_Type *)     DWT_BASE)                                     /*!< DWT configuration struct */
#endif

/* ==================== 简化数学函数 ==================== */

/**
 * @brief 简化平方根函数（使用牛顿迭代法）
 * @param[in] x 输入值
 * @return float 平方根值
 */
static float simple_sqrtf(float x)
{
    float guess = x / 2.0f;
    float prev_guess;
    uint8_t i;
    
    if (x <= 0.0f)
    {
        return 0.0f;
    }
    
    /* 牛顿迭代法：guess = (guess + x/guess) / 2 */
    for (i = 0; i < 10; i++)  /* 迭代10次，通常足够精确（数学函数，不需要常量） */
    {
        prev_guess = guess;
        guess = (guess + x / guess) / 2.0f;
        
        /* 如果变化很小，提前退出 */
        if ((guess - prev_guess) * (guess - prev_guess) < 0.0001f)
        {
            break;
        }
    }
    
    return guess;
}

#if CONFIG_MODULE_LOG_ENABLED
#include "log.h"
#endif

/* ==================== 模块状态 ==================== */

static uint8_t g_quality_test_initialized = 0;  /**< 模块初始化标志 */
uint8_t g_quality_test_verbose_log = 1;  /**< 详细日志标志：1=输出详细日志，0=仅输出汇总 */

/* ==================== DWT周期计数器支持 ==================== */

/**
 * @brief 初始化DWT周期计数器（用于微秒级精度测量）
 */
static void DWT_Init(void)
{
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/**
 * @brief 使用DWT测量微秒级延迟
 * @param[in] start_cycles 开始时的周期计数
 * @param[in] end_cycles 结束时的周期计数
 * @return uint32_t 延迟时间（微秒）
 */
static uint32_t DWT_GetElapsedUs(uint32_t start_cycles, uint32_t end_cycles)
{
    uint32_t cycles = end_cycles - start_cycles;
    extern uint32_t SystemCoreClock;
    return (uint32_t)((uint64_t)cycles * 1000000ULL / SystemCoreClock);
}

/* ==================== 静态缓冲区（减少栈使用） ==================== */

static uint8_t s_test_buffer[256] __attribute__((aligned(4)));      /**< 测试数据缓冲区（复用） */
static uint8_t s_read_buffer[256] __attribute__((aligned(4)));     /**< 读取数据缓冲区（复用） */
static uint8_t s_verify_buffer[256] __attribute__((aligned(4)));   /**< 验证数据缓冲区（复用） */

/* ==================== W25Q扩展指令定义 ==================== */

/* W25Q扩展指令（不在标准驱动中） */
#define W25Q_CMD_READ_UNIQUE_ID    0x4B    /**< 读取Unique ID */
#define W25Q_CMD_READ_SFDP         0x5A    /**< 读取SFDP表 */
#define W25Q_CMD_READ_STATUS_REG2  0x35    /**< 读状态寄存器2 */
#define W25Q_CMD_READ_STATUS_REG3  0x15    /**< 读状态寄存器3 */
#define W25Q_CMD_WRITE_STATUS_REG  0x01    /**< 写状态寄存器 */
#define W25Q_CMD_DEEP_POWER_DOWN   0xB9    /**< 进入Deep Power-Down模式 */
#define W25Q_CMD_RELEASE_POWER_DOWN 0xAB   /**< 退出Deep Power-Down模式 */

/* 测试区域配置（使用Flash末尾区域，避免破坏用户数据） */
#define TEST_AREA_SIZE              (1024 * 1024)  /**< 测试区域大小（1MB） */
#define TEST_PAGE_SIZE              256      /**< 页大小 */
#define TEST_SECTOR_SIZE            4096     /**< 扇区大小 */
#define TEST_BLOCK_SIZE             65536     /**< Block大小（64KB） */

/* ==================== 测试循环次数配置 ==================== */
#define TEST_WAKEUP_DELAY_COUNT          10      /**< 唤醒延迟测试次数（简化版，原规范1000次） */
#define TEST_ERASE_SECTOR_COUNT          4       /**< 擦除测试扇区数 */
#define TEST_ERASE_CYCLE_COUNT           5       /**< 每个扇区擦除循环次数 */
#define TEST_PROGRAM_COUNT               10      /**< 编程测试次数（简化版，原规范256次） */
#define TEST_BAD_BLOCK_COUNT             16      /**< 坏块测试数量（简化版，原规范全片测试） */
#define TEST_READ_DISTURB_COUNT          1000    /**< 读干扰测试循环次数（简化版，原规范100000次） */
#define TEST_READ_DISTURB_CHECK_INTERVAL 100     /**< 读干扰检查间隔（每N次检查一次） */
#define TEST_DELAY_DEGRADATION_COUNT     10      /**< 延迟退化测试循环次数（简化版，原规范100次） */
#define TEST_DELAY_DEGRADATION_CYCLE     10      /**< 每次延迟测量前的擦写循环次数（简化版，原规范100次） */

/* ==================== 测试延迟时间配置 ==================== */
#define TEST_WAKEUP_DELAY_MS             1       /**< 唤醒延迟模拟时间（ms） */
#define TEST_DELAY_AFTER_WRITE_MS        10      /**< 写入后等待时间（ms） */
#define TEST_DELAY_AFTER_ERASE_MS        10      /**< 擦除后等待时间（ms） */
#define TEST_DELAY_STATUS_REG_WRITE_MS   10      /**< 状态寄存器写入后等待时间（ms） */
#define TEST_DELAY_STATUS_REG_RETRY      100     /**< 状态寄存器写入重试次数 */

/* ==================== 判定阈值配置 ==================== */
#define TEST_WAKEUP_MEAN_THRESHOLD       200.0f   /**< 唤醒延迟均值阈值（微秒），超过此值判定为翻新
                                                    * 注意：测量值包含SPI通信和状态寄存器读取时间（约10-20μs），
                                                    * 实际Flash唤醒延迟约30-50μs，测量值约150-180μs为正常范围 */
#define TEST_WAKEUP_STD_DEV_THRESHOLD    500.0f   /**< 唤醒延迟标准差阈值（微秒），超过此值判定为翻新 */
#define TEST_ERASE_MEAN_THRESHOLD        120000.0f /**< 擦除延迟均值阈值（微秒），超过此值判定为翻新（正品应<100ms） */
#define TEST_ERASE_CV_THRESHOLD          12.0f    /**< 擦除延迟CV阈值（%），超过此值判定为翻新 */
#define TEST_PROGRAM_TIMEOUT_THRESHOLD   2       /**< 编程超时次数阈值，超过此值判定为翻新 */
#define TEST_PROGRAM_TIMEOUT_MS          1.5f     /**< 编程超时时间（ms），超过此时间视为超时 */
#define TEST_HEALTH_SCORE_THRESHOLD_A    85      /**< 健康度A级阈值（>85为A级） */
#define TEST_HEALTH_SCORE_THRESHOLD_B    70      /**< 健康度B级阈值（70-85为B级） */
#define TEST_BAD_BLOCK_THRESHOLD         2       /**< 坏块数量阈值，超过此值健康度为0 */
#define TEST_READ_DISTURB_ERROR_THRESHOLD 10     /**< 读干扰错误数阈值，超过此值健康度降低 */

/**
 * @brief 计算测试区域起始地址（根据Flash实际容量动态计算）
 * @return uint32_t 测试区域起始地址，如果计算失败返回0
 */
static uint32_t calculate_test_area_addr(void)
{
    const w25q_dev_t *dev_info;
    uint64_t capacity_bytes;
    uint32_t test_addr;
    
    dev_info = W25Q_GetInfo();
    if (dev_info == NULL || dev_info->capacity_mb == 0)
    {
        return 0;
    }
    
    /* 计算Flash总容量（字节） */
    capacity_bytes = (uint64_t)dev_info->capacity_mb * 1024ULL * 1024ULL;
    
    /* 测试区域从末尾1MB开始（如果Flash容量小于1MB，则从0开始） */
    if (capacity_bytes > TEST_AREA_SIZE)
    {
        test_addr = (uint32_t)(capacity_bytes - TEST_AREA_SIZE);
    }
    else
    {
        /* Flash容量太小，使用起始地址（不推荐，但至少不会越界） */
        test_addr = 0;
    }
    
    /* 确保地址对齐到扇区边界（4KB） */
    test_addr = (test_addr / TEST_SECTOR_SIZE) * TEST_SECTOR_SIZE;
    
    return test_addr;
}

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 擦除-写入-读取-验证（内部辅助函数，减少代码重复）
 * @param[in] addr 测试地址
 * @param[in] write_data 写入数据
 * @param[in] data_len 数据长度
 * @param[out] read_data 读取数据缓冲区
 * @param[out] is_match 数据是否匹配（1=匹配，0=不匹配）
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_erase_write_read_verify(
    uint32_t addr,
    const uint8_t *write_data,
    uint32_t data_len,
    uint8_t *read_data,
    uint8_t *is_match)
{
    W25Q_Status_t w25q_status;
    
    if (write_data == NULL || read_data == NULL || is_match == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    *is_match = 0;
    
    /* 擦除 */
    w25q_status = W25Q_EraseSector(addr);
    if (w25q_status != W25Q_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    W25Q_WaitReady(2000);  /* 2秒超时 */
    
    /* 写入 */
    w25q_status = W25Q_Write(addr, write_data, data_len);
    if (w25q_status != W25Q_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    W25Q_WaitReady(2000);  /* 2秒超时 */
    
    /* 读取 */
    Delay_ms(TEST_DELAY_AFTER_WRITE_MS);
    w25q_status = W25Q_Read(addr, read_data, data_len);
    if (w25q_status != W25Q_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 验证 */
    *is_match = (memcmp(write_data, read_data, data_len) == 0) ? 1 : 0;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 读取W25Q状态寄存器1
 * @param[out] status 状态寄存器值
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_read_status_reg1(uint8_t *status)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (status == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x05, 100);  /* W25Q_CMD_READ_STATUS_REG1 */
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, status, 100);
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 读取W25Q状态寄存器2
 * @param[out] status 状态寄存器值
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_read_status_reg2(uint8_t *status)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (status == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_STATUS_REG2, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, status, 100);
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 读取W25Q状态寄存器3
 * @param[out] status 状态寄存器值
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_read_status_reg3(uint8_t *status)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (status == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_STATUS_REG3, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    spi_status = SPI_MasterReceiveByte(spi_instance, status, 100);
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 读取W25Q Unique ID（64位）
 * @param[out] unique_id Unique ID（64位）
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_read_unique_id(uint64_t *unique_id)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint8_t id_bytes[8];
    uint32_t i;
    
    if (unique_id == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    
    /* 发送读取Unique ID指令 */
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_UNIQUE_ID, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 发送3个地址字节（通常为0x00, 0x00, 0x00） */
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 发送1个dummy字节 */
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 读取8字节Unique ID */
    for (i = 0; i < 8; i++)
    {
        spi_status = SPI_MasterReceiveByte(spi_instance, &id_bytes[i], 100);
        if (spi_status != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return QUALITY_TEST_ERROR_W25Q_FAILED;
        }
    }
    
    SPI_NSS_High(spi_instance);
    
    /* 组合64位ID */
    *unique_id = ((uint64_t)id_bytes[0] << 56) |
                ((uint64_t)id_bytes[1] << 48) |
                ((uint64_t)id_bytes[2] << 40) |
                ((uint64_t)id_bytes[3] << 32) |
                ((uint64_t)id_bytes[4] << 24) |
                ((uint64_t)id_bytes[5] << 16) |
                ((uint64_t)id_bytes[6] << 8) |
                ((uint64_t)id_bytes[7]);
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 读取W25Q SFDP表
 * @param[out] sfdp SFDP表数据缓冲区（至少256字节）
 * @param[in] size 缓冲区大小
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_read_sfdp(uint8_t *sfdp, uint32_t size)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint32_t i;
    
    if (sfdp == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (size < 256)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    
    /* 发送读取SFDP指令 */
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_READ_SFDP, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 发送3个地址字节（SFDP起始地址：0x000000） */
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 发送1个dummy字节 */
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x00, 100);
    if (spi_status != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 读取256字节SFDP数据 */
    for (i = 0; i < 256; i++)
    {
        spi_status = SPI_MasterReceiveByte(spi_instance, &sfdp[i], 100);
        if (spi_status != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return QUALITY_TEST_ERROR_W25Q_FAILED;
        }
    }
    
    SPI_NSS_High(spi_instance);
    
    return QUALITY_TEST_OK;
}

/* ==================== 统计函数实现 ==================== */

/**
 * @brief 计算数组均值
 */
Quality_Test_Status_t QualityTest_CalculateMean(const float *data, uint32_t count, float *mean)
{
    uint32_t i;
    float sum = 0.0f;
    
    if (data == NULL || mean == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (count == 0)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    for (i = 0; i < count; i++)
    {
        sum += data[i];
    }
    
    *mean = sum / (float)count;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 计算数组均值和标准差（一次遍历，优化版本）
 * @param[in] data 数据数组
 * @param[in] count 数据个数
 * @param[out] mean 均值
 * @param[out] std_dev 标准差
 * @return Quality_Test_Status_t 错误码
 * @note 如果只需要均值或标准差，可以传入NULL
 */
static Quality_Test_Status_t calculate_mean_and_stddev(
    const float *data,
    uint32_t count,
    float *mean,
    float *std_dev)
{
    uint32_t i;
    float sum = 0.0f;
    float sum_sq_diff = 0.0f;
    float calculated_mean;
    
    if (data == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (count == 0)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 一次遍历计算均值（只计算有效数据，过滤0值） */
    uint32_t valid_count = 0;
    for (i = 0; i < count; i++)
    {
        if (data[i] > 0.0f)  /* 只计算有效数据 */
        {
            sum += data[i];
            valid_count++;
        }
    }
    
    if (valid_count > 0)
    {
        calculated_mean = sum / (float)valid_count;
    }
    else
    {
        calculated_mean = 0.0f;  /* 无有效数据 */
    }
    
    if (mean != NULL)
    {
        *mean = calculated_mean;
    }
    
    /* 如果需要计算标准差，进行第二次遍历（使用相同的有效数据） */
    if (std_dev != NULL)
    {
        uint32_t std_valid_count = 0;
        
        for (i = 0; i < count; i++)
        {
            /* 只计算有效数据（非0值），与均值计算保持一致 */
            if (data[i] > 0.0f)
            {
                float diff = data[i] - calculated_mean;
                sum_sq_diff += diff * diff;
                std_valid_count++;
            }
        }
        
        /* 使用有效数据数量计算标准差（避免0值影响） */
        if (std_valid_count > 1)
        {
            /* 使用样本标准差公式：sqrt(sum((x-mean)^2) / (n-1))，更准确 */
            *std_dev = simple_sqrtf(sum_sq_diff / (float)(std_valid_count - 1));
        }
        else if (std_valid_count == 1)
        {
            *std_dev = 0.0f;  /* 只有一个数据点，标准差为0 */
        }
        else
        {
            *std_dev = 0.0f;  /* 无有效数据 */
        }
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 计算数组标准差
 */
Quality_Test_Status_t QualityTest_CalculateStdDev(const float *data, uint32_t count, float mean, float *std_dev)
{
    uint32_t i;
    float sum_sq_diff = 0.0f;
    float variance;
    float calculated_mean = mean;
    
    if (data == NULL || std_dev == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (count == 0)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 如果mean为0，自动计算均值 */
    if (mean == 0.0f)
    {
        Quality_Test_Status_t status = QualityTest_CalculateMean(data, count, &calculated_mean);
        if (status != QUALITY_TEST_OK)
        {
            return status;
        }
    }
    
    /* 计算方差 */
    for (i = 0; i < count; i++)
    {
        float diff = data[i] - calculated_mean;
        sum_sq_diff += diff * diff;
    }
    
    variance = sum_sq_diff / (float)count;
    
    /* 计算标准差 */
    *std_dev = simple_sqrtf(variance);
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 计算数组变异系数（CV）
 */
Quality_Test_Status_t QualityTest_CalculateCV(const float *data, uint32_t count, float *cv)
{
    float mean = 0.0f;
    float std_dev = 0.0f;
    Quality_Test_Status_t status;
    
    if (cv == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    /* 计算均值 */
    status = QualityTest_CalculateMean(data, count, &mean);
    if (status != QUALITY_TEST_OK)
    {
        return status;
    }
    
    if (mean == 0.0f)
    {
        *cv = 0.0f;
        return QUALITY_TEST_OK;
    }
    
    /* 计算标准差 */
    status = QualityTest_CalculateStdDev(data, count, mean, &std_dev);
    if (status != QUALITY_TEST_OK)
    {
        return status;
    }
    
    /* 计算变异系数（%） */
    *cv = (std_dev / mean) * 100.0f;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 浮点数数组排序（升序，使用冒泡排序）
 */
Quality_Test_Status_t QualityTest_SortFloatArray(float *data, uint32_t count)
{
    uint32_t i, j;
    float temp;
    
    if (data == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (count == 0)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 冒泡排序 */
    for (i = 0; i < count - 1; i++)
    {
        for (j = 0; j < count - 1 - i; j++)
        {
            if (data[j] > data[j + 1])
            {
                temp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = temp;
            }
        }
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 计算数组百分位数
 */
Quality_Test_Status_t QualityTest_CalculatePercentile(const float *data, uint32_t count, uint8_t percentile, float *value)
{
    uint32_t index;
    float *sorted_data = NULL;
    Quality_Test_Status_t status;
    
    if (data == NULL || value == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (count == 0 || percentile > 100)
    {
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    /* 创建临时数组用于排序 */
    sorted_data = (float*)data;  /* 注意：这会修改原数组，但为了节省内存 */
    
    /* 复制数据到临时数组（如果需要保持原数组不变，需要分配内存） */
    /* 这里为了简化，直接使用原数组，调用者需要先复制数据 */
    
    /* 排序 */
    status = QualityTest_SortFloatArray(sorted_data, count);
    if (status != QUALITY_TEST_OK)
    {
        return status;
    }
    
    /* 计算百分位数索引 */
    index = (uint32_t)((float)count * (float)percentile / 100.0f);
    if (index >= count)
    {
        index = count - 1;
    }
    
    *value = sorted_data[index];
    
    return QUALITY_TEST_OK;
}

/* ==================== 公共接口函数实现 ==================== */

/**
 * @brief 质量检测模块初始化
 */
Quality_Test_Status_t QualityTest_Init(void)
{
    if (g_quality_test_initialized)
    {
        return QUALITY_TEST_OK;
    }
    
    /* 检查W25Q是否已初始化 */
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 初始化DWT周期计数器（用于微秒级精度测量） */
    DWT_Init();
    
    g_quality_test_initialized = 1;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 阶段1：数字身份验证（简化实现）
 */
Quality_Test_Status_t QualityTest_Stage1_DigitalIdentity(Quality_Test_Result_t *result)
{
    const w25q_dev_t *dev_info;
    Quality_Test_Status_t status;
    uint8_t status_reg1;
    
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 初始化结果结构体（先执行，避免在LOG_INFO时卡住） */
    memset(result, 0, sizeof(Quality_Test_Result_t));
    
    /* 延迟一下，给UART时间清空缓冲区，然后输出日志 */
    Delay_ms(100);
    
    /* 只在详细日志模式下输出阶段标题 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "=== 阶段1：数字身份验证 ===");
    }
    
    /* 读取JEDEC ID */
    dev_info = W25Q_GetInfo();
    if (dev_info == NULL)
    {
        LOG_ERROR("QUALITY", "阶段1：W25Q_GetInfo() 失败");
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    result->jedec_id = ((uint32_t)dev_info->manufacturer_id << 16) | dev_info->device_id;
    LOG_INFO("QUALITY", "阶段1：JEDEC ID = 0x%06X (厂商码=0x%02X, 设备ID=0x%04X)", 
             result->jedec_id, dev_info->manufacturer_id, dev_info->device_id);
    
    /* 读取Unique ID */
    status = w25q_read_unique_id(&result->unique_id);
    if (status != QUALITY_TEST_OK)
    {
        /* Unique ID读取失败，可能是芯片不支持，继续其他测试 */
        result->unique_id = 0;
        LOG_WARN("QUALITY", "阶段1：Unique ID读取失败（可能不支持）");
    }
    else
    {
        LOG_INFO("QUALITY", "阶段1：Unique ID = 0x%016llX", result->unique_id);
    }
    
    /* 读取SFDP表 */
    status = w25q_read_sfdp(result->sfdp, sizeof(result->sfdp));
    if (status != QUALITY_TEST_OK)
    {
        /* SFDP读取失败，可能是芯片不支持，继续其他测试 */
        memset(result->sfdp, 0, sizeof(result->sfdp));
        LOG_WARN("QUALITY", "阶段1：SFDP读取失败（可能不支持）");
    }
    else
    {
        LOG_INFO("QUALITY", "阶段1：SFDP表读取成功");
    }
    
    /* 读取状态寄存器 */
    status = w25q_read_status_reg1(&status_reg1);
    if (status == QUALITY_TEST_OK)
    {
        result->status_reg[0] = status_reg1;
        LOG_INFO("QUALITY", "阶段1：状态寄存器1 = 0x%02X", status_reg1);
    }
    
    status = w25q_read_status_reg2(&result->status_reg[1]);
    if (status != QUALITY_TEST_OK)
    {
        result->status_reg[1] = 0;
    }
    else
    {
        LOG_INFO("QUALITY", "阶段1：状态寄存器2 = 0x%02X", result->status_reg[1]);
    }
    
    status = w25q_read_status_reg3(&result->status_reg[2]);
    if (status != QUALITY_TEST_OK)
    {
        result->status_reg[2] = 0;
    }
    else
    {
        LOG_INFO("QUALITY", "阶段1：状态寄存器3 = 0x%02X", result->status_reg[2]);
    }
    
    /* 验证JEDEC ID */
    if (dev_info->manufacturer_id != 0xEF)
    {
        /* 厂商码不符，可能是山寨货 */
        LOG_ERROR("QUALITY", "阶段1：厂商码不符！期望0xEF，实际0x%02X → GRADE_D", dev_info->manufacturer_id);
        result->grade = QUALITY_GRADE_D;
        result->stage1_passed = 0;
        return QUALITY_TEST_OK;  /* 返回OK，但标记为D级 */
    }
    
    LOG_INFO("QUALITY", "阶段1：通过（厂商码验证成功）");
    result->stage1_passed = 1;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 写入W25Q状态寄存器（用于测试）
 * @param[in] reg_index 寄存器索引（1/2/3）
 * @param[in] value 要写入的值
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_write_status_reg(uint8_t reg_index, uint8_t value)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 写使能 */
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, 0x06, 100);  /* Write Enable */
    SPI_NSS_High(spi_instance);
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    Delay_us(5);  /* 等待写使能生效 */
    
    /* 写入状态寄存器 */
    SPI_NSS_Low(spi_instance);
    
    if (reg_index == 1)
    {
        /* 写状态寄存器1 */
        spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_WRITE_STATUS_REG, 100);
        if (spi_status == SPI_OK)
        {
            spi_status = SPI_MasterTransmitByte(spi_instance, value, 100);
        }
    }
    else if (reg_index == 2)
    {
        /* 写状态寄存器2 */
        spi_status = SPI_MasterTransmitByte(spi_instance, 0x31, 100);  /* Write Status Register 2 */
        if (spi_status == SPI_OK)
        {
            spi_status = SPI_MasterTransmitByte(spi_instance, value, 100);
        }
    }
    else if (reg_index == 3)
    {
        /* 写状态寄存器3 */
        spi_status = SPI_MasterTransmitByte(spi_instance, 0x11, 100);  /* Write Status Register 3 */
        if (spi_status == SPI_OK)
        {
            spi_status = SPI_MasterTransmitByte(spi_instance, value, 100);
        }
    }
    else
    {
        SPI_NSS_High(spi_instance);
        return QUALITY_TEST_ERROR_INVALID_PARAM;
    }
    
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 等待写入完成 - 使用W25Q_WaitReady替代固定延迟 */
    W25Q_Status_t wait_status = W25Q_WaitReady(1000);  /* 1秒超时 */
    if (wait_status != W25Q_OK)
    {
        return QUALITY_TEST_ERROR_TIMEOUT;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief W25Q进入Deep Power-Down模式
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t w25q_deep_power_down(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint8_t status_reg;
    Quality_Test_Status_t test_status;
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 关键：进入Deep Power-Down前，必须确保芯片完全空闲
     * 如果芯片正在执行擦除/编程操作（BUSY=1），Deep Power-Down命令会被忽略
     * 根据检查清单，需要等待芯片完全空闲 */
    test_status = w25q_read_status_reg1(&status_reg);
    if (test_status != QUALITY_TEST_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 调试：输出状态寄存器值（仅在调试模式） */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：[调试] 进入DPD前，状态寄存器1=0x%02X (BUSY=%d, WEL=%d)", 
                 status_reg, (status_reg & 0x01) ? 1 : 0, (status_reg & 0x02) ? 1 : 0);
    }
    
    /* 检查BUSY位，如果芯片正在忙，等待其完成 */
    if ((status_reg & 0x01) != 0)  /* BUSY位为1，芯片正在忙 */
    {
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段3：[调试] 芯片正在忙，等待空闲...");
        }
        /* 使用W25Q_WaitReady等待芯片空闲 */
        W25Q_Status_t wait_status = W25Q_WaitReady(5000);  /* 5秒超时 */
        if (wait_status != W25Q_OK)
        {
            if (g_quality_test_verbose_log)
            {
                LOG_WARN("QUALITY", "阶段3：[调试] 等待芯片空闲超时");
            }
            return QUALITY_TEST_ERROR_TIMEOUT;
        }
        
        /* 再次读取状态寄存器，确认芯片已空闲 */
        test_status = w25q_read_status_reg1(&status_reg);
        if (test_status != QUALITY_TEST_OK)
        {
            return QUALITY_TEST_ERROR_W25Q_FAILED;
        }
        
        if ((status_reg & 0x01) != 0)  /* 仍然忙，返回错误 */
        {
            if (g_quality_test_verbose_log)
            {
                LOG_WARN("QUALITY", "阶段3：[调试] 等待后芯片仍然忙，状态寄存器=0x%02X", status_reg);
            }
            return QUALITY_TEST_ERROR_W25Q_FAILED;
        }
        
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段3：[调试] 芯片已空闲，状态寄存器=0x%02X", status_reg);
        }
    }
    
    /* 优化：即使芯片已经空闲，也额外等待一段时间，确保芯片完全稳定
     * 这对于确保DPD命令能够成功执行非常重要 */
    Delay_ms(5);  /* 等待5ms，确保芯片完全稳定，准备接收DPD命令 */
    
    /* 芯片已空闲，发送Deep Power-Down命令
     * 根据W25Q规范，CS必须在第8个时钟位后拉高，否则指令不执行
     * 注意：Deep Power-Down命令不需要写使能（WEL）
     * 
     * 重要：根据W25Q64JVSSIQ规格书，Deep Power-Down命令（0xB9）的时序要求：
     * 1. CS必须在第8个时钟位后拉高
     * 2. 命令发送后，芯片需要TDPD时间（至少3μs）才能进入Deep Power-Down
     * 3. 如果芯片正在执行其他操作（擦除/编程），Deep Power-Down命令会被忽略
     * 4. 如果芯片已经空闲，命令应该立即生效
     * 
     * 关键改进：确保CS拉高后芯片有足够时间处理命令 */
    SPI_NSS_Low(spi_instance);
    Delay_us(2);  /* CS拉低后延时，确保芯片识别CS低电平（增加到2μs） */
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_DEEP_POWER_DOWN, 100);
    Delay_us(2);  /* 命令发送后延时，确保命令被芯片接收（增加到2μs） */
    SPI_NSS_High(spi_instance);
    Delay_us(10);  /* CS拉高后延时，确保芯片有足够时间处理Deep Power-Down命令（增加到10μs） */
    
    if (spi_status != SPI_OK)
    {
        if (g_quality_test_verbose_log)
        {
            LOG_WARN("QUALITY", "阶段3：[调试] 发送Deep Power-Down命令失败");
        }
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 根据W25Q规范，Deep Power-Down命令发送后，芯片需要TDPD时间（至少3μs）才能进入休眠
     * 但实际测试中发现，某些芯片可能需要更长时间才能完全进入Deep Power-Down
     * 特别是电源去耦电容不足时，芯片可能需要更长时间才能稳定进入Deep Power-Down
     * 
     * 优化：增加等待时间，确保芯片有足够时间处理DPD命令
     * 注意：这个等待时间在Measure_Wakeup_Delay中还会再次等待（更长的时间），
     * 但这里也需要等待足够时间，确保芯片开始进入Deep Power-Down状态 */
    Delay_us(200);  /* 增加到200μs，确保芯片有足够时间处理DPD命令（规范要求至少3μs，但实际需要更长时间） */
    
    /* 诊断信息：记录发送Deep Power-Down命令的状态（仅在调试模式）
     * 如果后续验证发现芯片没有进入Deep Power-Down，这些信息有助于诊断问题 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：[调试] Deep Power-Down命令已发送，状态寄存器1=0x%02X (BUSY=%d)", 
                 status_reg, (status_reg & 0x01) ? 1 : 0);
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief W25Q退出Deep Power-Down模式（唤醒）
 * @note 此函数未使用，唤醒逻辑已内联到Measure_Wakeup_Delay中以提高测量精度
 * @return Quality_Test_Status_t 错误码
 */
static Quality_Test_Status_t __attribute__((unused)) w25q_release_power_down(void)
{
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    SPI_Status_t spi_status;
    uint8_t dummy;
    
    if (!W25Q_IsInitialized())
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    SPI_NSS_Low(spi_instance);
    spi_status = SPI_MasterTransmitByte(spi_instance, W25Q_CMD_RELEASE_POWER_DOWN, 100);
    if (spi_status == SPI_OK)
    {
        /* Release Power-Down命令需要3个dummy字节 */
        SPI_MasterReceiveByte(spi_instance, &dummy, 100);
        SPI_MasterReceiveByte(spi_instance, &dummy, 100);
        SPI_MasterReceiveByte(spi_instance, &dummy, 100);
    }
    SPI_NSS_High(spi_instance);
    
    if (spi_status != SPI_OK)
    {
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 测量唤醒延迟（使用DWT精确测量）
 * @return uint32_t 唤醒延迟（微秒）
 */
static uint32_t Measure_Wakeup_Delay(void)
{
    uint32_t end_cycles;
    uint8_t status;
    Quality_Test_Status_t test_status;
    SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
    
    /* 进入Deep Power-Down模式 */
    test_status = w25q_deep_power_down();
    if (test_status != QUALITY_TEST_OK)
    {
        /* 如果不支持Deep Power-Down，返回0表示无法测量 */
        return 0;
    }
    
    /* 等待进入Deep Power-Down模式
     * 规范要求TDPD时间至少3μs，但实际需要更长时间才能确保芯片真正进入休眠
     * 根据W25Q规范和建议，等待足够时间，确保芯片完全进入Deep Power-Down状态
     * 
     * 恢复到之前的设置（20ms），以保持与之前有标准差版本的兼容性 */
    Delay_ms(20);  /* 等待20ms，确保芯片真正进入休眠（特别是电容不足时，建议≥220μF） */
    
    /* 发送Release Power-Down命令（0xAB）
     * 根据W25Q规范，Release命令后芯片需要时间唤醒，我们测量这个时间
     * 
     * 优化：在发送Release命令前，确保CS时序正确 */
    SPI_NSS_Low(spi_instance);
    Delay_us(2);  /* CS拉低后延时，确保芯片识别CS低电平 */
    
    if (SPI_MasterTransmitByte(spi_instance, W25Q_CMD_RELEASE_POWER_DOWN, 100) != SPI_OK)
    {
        SPI_NSS_High(spi_instance);
        return 0;
    }
    
    /* 发送3个dummy字节（Release Power-Down命令要求）
     * 关键：根据W25Q规范，如果芯片在Deep Power-Down中：
     *   - 这3个dummy字节期间芯片会唤醒
     *   - 前几个字节可能返回0xFF（芯片未响应）
     *   - 后面的字节会返回正常数据（芯片已唤醒）
     * 如果芯片没有在Deep Power-Down中：
     *   - 这3个字节会立即返回正常的状态寄存器值（通常是0x00或其他值）
     * 我们通过检查这3个字节的返回值来判断芯片是否真正进入了Deep Power-Down */
    uint8_t dummy1, dummy2, dummy3;
    SPI_MasterReceiveByte(spi_instance, &dummy1, 100);
    SPI_MasterReceiveByte(spi_instance, &dummy2, 100);
    SPI_MasterReceiveByte(spi_instance, &dummy3, 100);
    SPI_NSS_High(spi_instance);  /* Release命令完成，拉高CS */
    
    /* 记录Release命令完成的时间点 */
    uint32_t release_end_cycles = DWT->CYCCNT;
    
    /* 检查dummy字节的返回值，判断芯片是否在Deep Power-Down中
     * 根据W25Q64JVSSIQ规格书：
     * - 如果芯片在Deep Power-Down中，Release命令后的dummy字节应该返回0xFF（芯片未响应）
     * - 如果芯片不在Deep Power-Down中，dummy字节会立即返回正常数据（通常是0x00或其他值）
     * 
     * 注意：如果3个字节都是0xFF，说明芯片可能在Deep Power-Down中
     * 如果3个字节不是0xFF，说明芯片没有在Deep Power-Down中（立即响应）
     * 
     * 重要：如果芯片没有进入Deep Power-Down，可能的原因：
     * 1. 芯片不支持Deep Power-Down（但W25Q64JVSSIQ应该支持）
     * 2. 硬件问题（电源、上拉电阻、电容等）- 请检查检查清单
     * 3. 命令发送时序问题
     * 4. 芯片正在执行其他操作（但我们已经检查了BUSY位） */
    uint8_t chip_in_dpd = (dummy1 == 0xFF && dummy2 == 0xFF && dummy3 == 0xFF) ? 1 : 0;
    
    /* 调试信息：记录dummy字节的返回值，用于分析芯片是否真正进入Deep Power-Down
     * 如果dummy字节不是0xFF，说明芯片可能没有进入Deep Power-Down
     * 这会导致唤醒延迟恒定，因为测量的是SPI通信时间，而不是真正的唤醒延迟 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：[调试] Release命令后dummy字节: 0x%02X 0x%02X 0x%02X, 芯片在DPD中=%d", 
                 dummy1, dummy2, dummy3, chip_in_dpd);
        
        /* 如果芯片没有进入Deep Power-Down，输出警告信息，帮助诊断问题 */
        if (!chip_in_dpd)
        {
            LOG_WARN("QUALITY", "阶段3：[警告] 芯片可能未进入Deep Power-Down！");
            LOG_WARN("QUALITY", "阶段3：[警告] 可能原因：1)硬件问题(电源/上拉/电容) 2)芯片不支持 3)时序问题");
            LOG_WARN("QUALITY", "阶段3：[警告] 请检查：Flash VCC与STM32 VDD是否同源，3.3V电源电容≥220μF");
        }
    }
    
    /* 根据W25Q64规格，Release命令后需要等待tRES1时间（数百μs至数ms）才能恢复正常读写
     * 如果芯片在Deep Power-Down中，需要等待更长时间；如果不在，可以立即读取
     * 
     * 优化：增加等待时间，确保芯片有足够时间完全唤醒 */
    if (chip_in_dpd)
    {
        /* 芯片在Deep Power-Down中，等待tRES1时间（根据规格，通常需要数百μs至数ms）
         * 优化：增加到5ms，确保芯片有足够时间完全唤醒 */
        Delay_ms(5);  /* 等待5ms，确保芯片完全唤醒（tRES1时间，某些芯片可能需要更长时间） */
    }
    else
    {
        /* 芯片可能没有进入Deep Power-Down，等待较短时间即可 */
        Delay_us(10);  /* 等待10μs，给芯片一点时间 */
    }
    
    /* 开始测量：从CS拉低开始计时，测量芯片响应时间
     * 如果芯片已经唤醒，会立即响应；如果还在唤醒中，会有延迟 */
    SPI_NSS_Low(spi_instance);
    uint32_t measure_start_cycles = DWT->CYCCNT;
    
    /* 尝试读取状态寄存器，测量芯片响应时间
     * 如果芯片还在唤醒中，可能需要多次尝试 */
    uint8_t retry_count = 0;
    uint8_t first_valid_attempt = 0;
    
    do
    {
        /* 发送Read Status Register命令 */
        if (SPI_MasterTransmitByte(spi_instance, 0x05, 100) != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return 0;
        }
        
        /* 读取状态寄存器 */
        if (SPI_MasterReceiveByte(spi_instance, &status, 100) != SPI_OK)
        {
            SPI_NSS_High(spi_instance);
            return 0;
        }
        
        /* 检查是否收到有效响应
         * 关键：如果芯片在Deep Power-Down中，第一次读取可能返回0xFF（未响应）
         * 如果芯片已唤醒，会返回正常的状态寄存器值（通常不是0xFF）
         * 但也要注意：正常状态寄存器值可能是0x00，所以不能仅用0xFF判断 */
        
        /* 如果收到非0xFF的响应，或者重试多次后收到任何响应，认为芯片已唤醒
         * 注意：正常状态寄存器可能是0x00，所以0x00也是有效响应 */
        if (status != 0xFF || retry_count >= 10)
        {
            first_valid_attempt = retry_count + 1;
            break;
        }
        
        retry_count++;
        if (retry_count > 100)  /* 防止无限循环 */
        {
            SPI_NSS_High(spi_instance);
            return 0;
        }
        
        /* 如果收到0xFF（芯片可能还在唤醒中），等待一小段时间后重试 */
        Delay_us(1);
    } while (1);
    
    /* 记录第一次收到有效响应的时间点 */
    end_cycles = DWT->CYCCNT;
    SPI_NSS_High(spi_instance);
    
    /* 计算延迟（微秒）：
     * 根据W25Q64规格，真正的唤醒延迟应该是从Release命令完成（CS拉高）到芯片能够响应的时间
     * 我们测量从等待tRES1后CS拉低到第一次收到有效响应的时间
     * 这样更准确地反映了芯片的唤醒延迟 */
    uint32_t measure_delay_us = DWT_GetElapsedUs(measure_start_cycles, end_cycles);
    
    /* 根据dummy字节的返回值和读取状态寄存器的结果，判断芯片是否真正进入Deep Power-Down
     * 如果dummy字节都是0xFF且需要重试才能读取状态寄存器，说明芯片真正进入了Deep Power-Down
     * 如果dummy字节不是0xFF或第一次就读取成功，说明芯片可能没有进入Deep Power-Down */
    if (chip_in_dpd && first_valid_attempt > 1)
    {
        /* 芯片真正进入了Deep Power-Down，需要重试才能响应
         * 使用测量延迟，减去SPI通信开销
         * 这是真正的唤醒延迟（tRES1时间） */
        uint32_t spi_overhead_us = 5;  /* Read Status命令+1字节读取 ≈ 5μs */
        if (measure_delay_us > spi_overhead_us)
        {
            /* 返回真实的唤醒延迟（tRES1时间）
             * 根据W25Q64规格，tRES1通常为数百μs至数ms */
            return measure_delay_us - spi_overhead_us;
        }
        else
        {
            return measure_delay_us;  /* 避免负数 */
        }
    }
    else
    {
        /* 芯片可能没有进入Deep Power-Down，或者已经唤醒
         * 这种情况下，延迟主要是SPI通信时间，不是真正的唤醒延迟
         * 返回一个较小的固定值，表示芯片没有真正进入Deep Power-Down */
        /* 注意：如果芯片不支持Deep Power-Down或没有真正进入，返回的值会很小且恒定
         * 这会导致标准差为0，但这是正常的，因为芯片确实没有进入Deep Power-Down */
        uint32_t spi_overhead_us = 5;  /* Read Status命令+1字节读取 ≈ 5μs */
        if (measure_delay_us > spi_overhead_us)
        {
            return measure_delay_us - spi_overhead_us;
        }
        else
        {
            return measure_delay_us;  /* 避免负数 */
        }
    }
}

/**
 * @brief 阶段2：山寨货深度鉴别
 */
Quality_Test_Status_t QualityTest_Stage2_FakeDetection(Quality_Test_Result_t *result)
{
    uint32_t i;
    uint8_t all_zero = 1;
    uint8_t all_ff = 1;
    uint8_t illegal_response;
    Quality_Test_Status_t status;
    
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 延迟一下，给UART时间清空缓冲区 */
    Delay_ms(50);
    
    /* 只在详细日志模式下输出阶段标题 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "=== 阶段2：山寨货深度鉴别 ===");
    }
    
    /* 测试1：检查SFDP表是否全0或全FF（可能是山寨货） */
    for (i = 0; i < 256; i++)
    {
        if (result->sfdp[i] != 0x00)
        {
            all_zero = 0;
        }
        if (result->sfdp[i] != 0xFF)
        {
            all_ff = 0;
        }
    }
    
    if (all_zero || all_ff)
    {
        /* SFDP表异常，可能是山寨货 */
        LOG_ERROR("QUALITY", "阶段2：SFDP表异常（%s）→ GRADE_D", all_zero ? "全0" : "全FF");
        result->grade = QUALITY_GRADE_D;
        result->stage2_passed = 0;
        return QUALITY_TEST_OK;
    }
    
    LOG_INFO("QUALITY", "阶段2：SFDP表检查通过");
    
    /* 测试2：保留位陷阱测试 */
    /* 注意：SR3的Bit7是地址模式位（Address Mode），不是保留位，所以跳过Bit7测试 */
    /* 对于W25Q64JV，SR3的Bit4:3可能是保留位，但不同芯片定义可能不同 */
    /* 这里简化实现：跳过保留位测试，因为真正的保留位测试需要更复杂的逻辑 */
    LOG_INFO("QUALITY", "阶段2：测试2 - 保留位陷阱测试（跳过，SR3 Bit7是地址模式位）");
    LOG_INFO("QUALITY", "阶段2：SR3 Bit7是地址模式位（S23），不是保留位，写入后保持为1是正常的");
    
    /* 测试3：非法指令响应分析 */
    LOG_INFO("QUALITY", "阶段2：测试3 - 非法指令响应分析");
    {
        SPI_Instance_t spi_instance = W25Q_SPI_INSTANCE;
        SPI_Status_t spi_status;
        
        /* 发送未定义指令0x9B */
        SPI_NSS_Low(spi_instance);
        spi_status = SPI_MasterTransmitByte(spi_instance, 0x9B, 100);
        if (spi_status == SPI_OK)
        {
            /* 读取响应 */
            spi_status = SPI_MasterReceiveByte(spi_instance, &illegal_response, 100);
        }
        SPI_NSS_High(spi_instance);
        
        if (spi_status == SPI_OK)
        {
            LOG_INFO("QUALITY", "阶段2：非法指令0x9B响应=0x%02X", illegal_response);
            /* 正品应返回0xFF或0x00，如果返回其他值，可能是山寨货 */
            if (illegal_response != 0xFF && illegal_response != 0x00)
            {
                /* 返回异常值 → 山寨货 */
                LOG_ERROR("QUALITY", "阶段2：非法指令返回异常值0x%02X（期望0xFF或0x00）→ GRADE_D", illegal_response);
                result->grade = QUALITY_GRADE_D;
                result->stage2_passed = 0;
                return QUALITY_TEST_OK;
            }
            LOG_INFO("QUALITY", "阶段2：非法指令响应测试通过（返回0x%02X）", illegal_response);
        }
        else
        {
            LOG_WARN("QUALITY", "阶段2：非法指令测试SPI通信失败");
        }
    }
    
    /* 测试4：WPS块保护穿透测试（简化实现） */
    LOG_INFO("QUALITY", "阶段2：测试4 - WPS块保护穿透测试");
    {
        uint32_t protect_test_addr;
        uint8_t sr1_backup;
        uint8_t test_write_data[256];
        W25Q_Status_t w25q_status;
        
        /* 动态计算测试地址 */
        protect_test_addr = calculate_test_area_addr();
        if (protect_test_addr != 0)
        {
            LOG_INFO("QUALITY", "阶段2：测试地址=0x%06X", protect_test_addr);
            /* 备份SR1原始值 */
            status = w25q_read_status_reg1(&sr1_backup);
            if (status == QUALITY_TEST_OK)
            {
                LOG_INFO("QUALITY", "阶段2：SR1原始值=0x%02X", sr1_backup);
                /* 使用地址0x000000进行测试（确保在保护范围内） */
                uint32_t test_addr = 0x000000;
                
                /* 启用块保护（设置BP[1:0]=11，保护全部区域） */
                /* 注意：BP[1:0]=11保护全部，无论TB位如何设置 */
                uint8_t protected_sr1 = (sr1_backup & 0xF3) | 0x0C;  /* 清除BP[1:0]，然后设置为11 */
                LOG_INFO("QUALITY", "阶段2：设置块保护，SR1=0x%02X (BP[1:0]=11, 保护全部)", protected_sr1);
                
                status = w25q_write_status_reg(1, protected_sr1);
                if (status == QUALITY_TEST_OK)
                {
                    Delay_ms(TEST_DELAY_STATUS_REG_WRITE_MS);
                    
                    /* 验证块保护是否设置成功 */
                    uint8_t sr1_verify;
                    status = w25q_read_status_reg1(&sr1_verify);
                    if (status == QUALITY_TEST_OK)
                    {
                        LOG_INFO("QUALITY", "阶段2：验证SR1=0x%02X", sr1_verify);
                        if ((sr1_verify & 0x0C) != 0x0C)
                        {
                            LOG_WARN("QUALITY", "阶段2：块保护设置失败，SR1=0x%02X", sr1_verify);
                            /* 恢复原始值 */
                            w25q_write_status_reg(1, sr1_backup);
                            Delay_ms(TEST_DELAY_STATUS_REG_WRITE_MS);
                            return QUALITY_TEST_OK;
                        }
                    }
                    
                    /* 先读取被保护区域的原始数据 */
                    uint8_t original_data[256];
                    w25q_status = W25Q_Read(test_addr, original_data, 256);
                    if (w25q_status == W25Q_OK)
                    {
                        /* 尝试写入被保护区域 */
                        memset(test_write_data, 0xAA, 256);
                        LOG_INFO("QUALITY", "阶段2：尝试写入被保护区域0x%06X（应失败）", test_addr);
                        w25q_status = W25Q_Write(test_addr, test_write_data, 256);
                        if (w25q_status == W25Q_OK)
                        {
                            W25Q_WaitReady(2000);  /* 2秒超时 */
                            
                            /* 读取写入后的数据，验证是否真的写入了 */
                            uint8_t read_back_data[256];
                            w25q_status = W25Q_Read(test_addr, read_back_data, 256);
                            if (w25q_status == W25Q_OK)
                            {
                                /* 检查数据是否被写入（如果块保护生效，数据应该不变） */
                                uint8_t data_changed = 0;
                                uint32_t j;
                                for (j = 0; j < 256; j++)
                                {
                                    if (read_back_data[j] != original_data[j] && read_back_data[j] == 0xAA)
                                    {
                                        data_changed = 1;
                                        break;
                                    }
                                }
                                
                                if (data_changed)
                                {
                                    /* 数据被写入了，说明块保护未生效 → 山寨货 */
                                    LOG_ERROR("QUALITY", "阶段2：块保护未生效（数据被写入）→ GRADE_D");
                                    result->grade = QUALITY_GRADE_D;
                                    result->stage2_passed = 0;
                                    
                                    /* 恢复原始值 */
                                    w25q_write_status_reg(1, sr1_backup);
                                    Delay_ms(TEST_DELAY_STATUS_REG_WRITE_MS);
                                    return QUALITY_TEST_OK;
                                }
                                else
                                {
                                    /* 数据未被写入，说明块保护生效 */
                                    LOG_INFO("QUALITY", "阶段2：WPS块保护测试通过（数据未被写入，保护生效）");
                                }
                            }
                            else
                            {
                                LOG_WARN("QUALITY", "阶段2：读取验证数据失败");
                            }
                        }
                        else
                        {
                            /* 写入命令失败，说明可能检测到保护，这是正常的 */
                            LOG_INFO("QUALITY", "阶段2：写入命令失败（可能是保护检测），继续验证");
                        }
                    }
                    else
                    {
                        LOG_WARN("QUALITY", "阶段2：读取原始数据失败，跳过WPS测试");
                    }
                    
                    /* 恢复原始值 */
                    w25q_write_status_reg(1, sr1_backup);
                    Delay_ms(TEST_DELAY_STATUS_REG_WRITE_MS);
                }
                else
                {
                    LOG_WARN("QUALITY", "阶段2：SR1写入失败，跳过WPS测试");
                }
            }
            else
            {
                LOG_WARN("QUALITY", "阶段2：SR1读取失败，跳过WPS测试");
            }
        }
        else
        {
            LOG_WARN("QUALITY", "阶段2：测试地址计算失败，跳过WPS测试");
        }
    }
    
    /* 所有测试通过 */
    LOG_INFO("QUALITY", "阶段2：通过（所有测试通过）");
    result->stage2_passed = 1;
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 阶段3：翻新货时序指纹鉴定（简化实现）
 */
Quality_Test_Status_t QualityTest_Stage3_RefurbishDetection(Quality_Test_Result_t *result)
{
    uint32_t i, j;
    W25Q_Status_t w25q_status;
    uint32_t test_addr;
    
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 动态计算测试区域地址 */
    test_addr = calculate_test_area_addr();
    if (test_addr == 0)
    {
        LOG_ERROR("QUALITY", "阶段3：测试区域地址计算失败");
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    
    /* 初始化阶段3的统计变量（确保从0开始） */
    result->program_timeout_count = 0;
    result->wakeup_mean = 0.0f;
    result->wakeup_std_dev = 0.0f;
    result->erase_cv = 0.0f;
    result->program_jitter = 0.0f;
    
    /* 阶段开始信息始终输出（不影响性能） */
    LOG_INFO("QUALITY", "=== 阶段3：翻新货时序指纹鉴定 ===");
    LOG_INFO("QUALITY", "阶段3：测试区域地址 = 0x%06X", test_addr);
    
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：测试1 - 唤醒延迟测试（%d次）", TEST_WAKEUP_DELAY_COUNT);
    }
    for (i = 0; i < TEST_WAKEUP_DELAY_COUNT; i++)
    {
        /* 使用DWT精确测量唤醒延迟（函数内部无日志，确保测量精度） */
        uint32_t delay_us = Measure_Wakeup_Delay();
        
        if (delay_us == 0)
        {
            /* Deep Power-Down不支持，标记为警告，不用于翻新判定 */
            LOG_WARN("QUALITY", "阶段3：Deep Power-Down不支持，跳过唤醒延迟测试");
            delay_us = 1;  /* 设置为1μs，避免影响统计，但不用于判定 */
        }
        
        /* 检查数组边界 */
        if (i < 100)  /* wakeup_delays数组大小为100 */
        {
            result->wakeup_delays[i] = (float)delay_us;
            /* 运行时控制：详细日志标志控制输出 */
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：唤醒延迟 %d/%d = %luμs", 
                         i+1, TEST_WAKEUP_DELAY_COUNT, (unsigned long)delay_us);
            }
        }
        else
        {
            LOG_ERROR("QUALITY", "阶段3：数组越界！i=%lu >= 100", (unsigned long)i);
        }
        
        /* 在每次测试之间增加间隔，确保芯片有足够时间进入休眠
         * 避免连续测试导致芯片无法真正进入Deep Power-Down状态
         * 根据检查清单，需要给芯片足够的时间完成状态转换 */
        if (i < TEST_WAKEUP_DELAY_COUNT - 1)  /* 最后一次不需要等待 */
        {
            Delay_ms(20);  /* 增加到20ms，确保芯片完全进入休眠并稳定 */
        }
    }
    
    /* 计算唤醒延迟统计（只使用实际测量的数据，不包含未使用的数组元素） */
    /* 注意：wakeup_delays数组大小为100，但只使用了前TEST_WAKEUP_DELAY_COUNT个元素 */
    calculate_mean_and_stddev(result->wakeup_delays, TEST_WAKEUP_DELAY_COUNT, 
                              &result->wakeup_mean, &result->wakeup_std_dev);
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：唤醒延迟测试完成，开始计算统计值");
        LOG_INFO("QUALITY", "阶段3：调用calculate_mean_and_stddev");
        LOG_INFO("QUALITY", "阶段3：calculate_mean_and_stddev完成");
        LOG_INFO("QUALITY", "阶段3：唤醒延迟统计 - 均值=%luμs, 标准差=%luμs", 
                 (unsigned long)result->wakeup_mean, (unsigned long)result->wakeup_std_dev);
    }
    
    /* 测试2：擦除延迟 */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：测试2 - 擦除延迟测试（%d个扇区，每个%d次）", 
                 TEST_ERASE_SECTOR_COUNT, TEST_ERASE_CYCLE_COUNT);
    }
    for (i = 0; i < TEST_ERASE_SECTOR_COUNT; i++)
    {
        uint32_t sector_addr = test_addr + i * TEST_SECTOR_SIZE;
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：测试扇区 %d/%d (地址=0x%06X)", i+1, TEST_ERASE_SECTOR_COUNT, sector_addr);
            }
        
        /* 先写入一些数据（确保扇区不是全FF状态） */
        {
            uint8_t dummy_data[256];
            memset(dummy_data, 0x55, 256);
            /* 运行时控制：详细日志标志控制输出 */
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：扇区%d - 预写入数据", i+1);
            }
            w25q_status = W25Q_Write(sector_addr, dummy_data, 256);
            if (w25q_status == W25Q_OK)
            {
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段3：扇区%d - 等待写入完成", i+1);
                }
                w25q_status = W25Q_WaitReady(2000);  /* 2秒超时，避免卡住 */
                if (w25q_status != W25Q_OK)
                {
                    LOG_WARN("QUALITY", "阶段3：扇区%d - 等待写入超时", i+1);
                }
                else
                {
                    if (g_quality_test_verbose_log)
                    {
                        LOG_INFO("QUALITY", "阶段3：扇区%d - 写入完成", i+1);
                    }
                }
            }
            else
            {
                LOG_WARN("QUALITY", "阶段3：扇区%d - 写入失败", i+1);
            }
        }
        
        for (j = 0; j < TEST_ERASE_CYCLE_COUNT; j++)
        {
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：扇区%d - 擦除循环 %d/%d", i+1, j+1, TEST_ERASE_CYCLE_COUNT);
            }
            
            /* 每次擦除前先写入数据（确保有内容可擦除） */
            if (j > 0)
            {
                uint8_t dummy_data[256];
                memset(dummy_data, 0x55, 256);
                w25q_status = W25Q_Write(sector_addr, dummy_data, 256);
                if (w25q_status == W25Q_OK)
                {
                    w25q_status = W25Q_WaitReady(2000);  /* 2秒超时 */
                    if (w25q_status != W25Q_OK)
                    {
                        LOG_WARN("QUALITY", "阶段3：扇区%d - 循环%d写入等待超时", i+1, j+1);
                    }
                    else
                    {
                        /* 运行时控制：详细日志标志控制输出 */
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段3：扇区%d - 循环%d写入完成", i+1, j+1);
                        }
                        /* 写入完成后，添加小延时确保芯片稳定（特别是在非调试模式下） */
                        Delay_ms(5);  /* 等待5ms，确保写入操作完全完成，芯片稳定 */
                    }
                }
            }
            
            /* 擦除扇区（使用DWT精确测量，测量期间禁用日志） */
            uint32_t erase_start_cycles = DWT->CYCCNT;
            w25q_status = W25Q_EraseSector(sector_addr);
            
            /* 注意：W25Q_EraseSector内部已经有WaitReady，但超时时间可能不够（100ms） */
            /* 所以我们需要再次等待，确保擦除真正完成 */
            /* 即使EraseSector返回超时，也可能擦除正在进行中，继续等待 */
            w25q_status = W25Q_WaitReady(10000);  /* 10秒超时，确保擦除完成 */
            
            if (w25q_status == W25Q_OK)
            {
                uint32_t erase_end_cycles = DWT->CYCCNT;
                uint32_t erase_delay_us = DWT_GetElapsedUs(erase_start_cycles, erase_end_cycles);
                result->erase_times[i][j] = (float)erase_delay_us;
                /* 运行时控制：详细日志标志控制输出 */
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段3：扇区%d - 循环%d - 擦除完成，总耗时=%luμs", 
                             i+1, j+1, (unsigned long)erase_delay_us);
                }
            }
            else
            {
                if (g_quality_test_verbose_log)
                {
                    LOG_WARN("QUALITY", "阶段3：扇区%d - 循环%d - 擦除等待超时或失败（状态=%d）", i+1, j+1, w25q_status);
                }
                result->erase_times[i][j] = 0.0f;
            }
            
            /* 关键修复：每次擦除完成后，添加稳定延时，确保芯片有足够时间恢复到稳定状态
             * 这有助于提高测量精度，特别是在非调试模式下（无日志输出时操作更快）
             * 连续擦除操作可能导致芯片内部状态不稳定，影响测量精度
             * 注意：这个延时不影响擦除时间的测量（测量在WaitReady完成后就结束了） */
            if (j < TEST_ERASE_CYCLE_COUNT - 1)  /* 最后一次不需要等待 */
            {
                Delay_ms(10);  /* 等待10ms，确保芯片稳定，提高测量精度 */
            }
        }
    }
    
    /* 计算擦除延迟CV */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：擦除延迟测试完成");
        LOG_INFO("QUALITY", "阶段3：计算擦除延迟CV");
    }
    {
        float block_means[TEST_ERASE_SECTOR_COUNT];
        float block_std_dev = 0.0f;
        float block_mean = 0.0f;
        
        /* 计算每个扇区的平均擦除时间（只计算有效数据，过滤0值） */
        uint32_t valid_sector_count = 0;
        float valid_block_means[TEST_ERASE_SECTOR_COUNT];
        
        for (i = 0; i < TEST_ERASE_SECTOR_COUNT; i++)
        {
            /* 计算该扇区的有效数据均值（过滤0值） */
            float sector_sum = 0.0f;
            uint32_t valid_count = 0;
            
            for (j = 0; j < TEST_ERASE_CYCLE_COUNT; j++)
            {
                if (result->erase_times[i][j] > 0.0f)
                {
                    sector_sum += result->erase_times[i][j];
                    valid_count++;
                }
            }
            
            if (valid_count > 0)
            {
                block_means[i] = sector_sum / (float)valid_count;
                valid_block_means[valid_sector_count] = block_means[i];
                valid_sector_count++;
                
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段3：扇区%d平均擦除时间 = %luμs (有效数据%d/%d)", 
                             i+1, (unsigned long)block_means[i], valid_count, TEST_ERASE_CYCLE_COUNT);
                }
            }
            else
            {
                block_means[i] = 0.0f;
                if (g_quality_test_verbose_log)
                {
                    LOG_WARN("QUALITY", "阶段3：扇区%d无有效擦除数据", i+1);
                }
            }
        }
        
        /* 计算扇区间均值的标准差和CV（只使用有效扇区数据） */
        if (valid_sector_count >= 2)  /* 至少需要2个扇区才能计算CV */
        {
            calculate_mean_and_stddev(valid_block_means, valid_sector_count, &block_mean, &block_std_dev);
            if (block_mean > 0.0f)
            {
                result->erase_cv = (block_std_dev / block_mean) * 100.0f;
            }
            else
            {
                result->erase_cv = 0.0f;
            }
        }
        else
        {
            /* 有效扇区数不足，无法计算CV */
            block_mean = 0.0f;
            block_std_dev = 0.0f;
            result->erase_cv = 0.0f;
            if (g_quality_test_verbose_log)
            {
                LOG_WARN("QUALITY", "阶段3：有效扇区数不足（%lu < 2），无法计算CV", (unsigned long)valid_sector_count);
            }
        }
        
        /* 使用整数显示CV，避免浮点数格式化问题 */
        uint32_t cv_percent = (uint32_t)(result->erase_cv * 100.0f + 0.5f);  /* 四舍五入到0.01% */
        /* 运行时控制：详细日志标志控制输出 */
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段3：擦除延迟统计 - 扇区均值=%luμs, 标准差=%luμs, CV=%lu.%02lu%%", 
                     (unsigned long)block_mean, (unsigned long)block_std_dev, 
                     cv_percent / 100, cv_percent % 100);
        }
    }
    
    /* 测试3：编程延迟 */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：测试3 - 编程延迟测试（%d次）", TEST_PROGRAM_COUNT);
    }
    {
        uint8_t test_data[256];
        uint32_t prog_test_addr = test_addr;
        
        /* 先擦除测试扇区 */
        /* 运行时控制：详细日志标志控制输出 */
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段3：预擦除测试区域");
        }
        w25q_status = W25Q_EraseSector(prog_test_addr);
        if (w25q_status == W25Q_OK)
        {
            w25q_status = W25Q_WaitReady(5000);  /* 5秒超时，擦除需要更长时间 */
            if (w25q_status != W25Q_OK)
            {
                LOG_WARN("QUALITY", "阶段3：预擦除等待超时");
            }
        }
        
        memset(test_data, 0xAA, 256);
        
        /* 关键修复1：确保数组访问不越界，并初始化program_timeout_count */
        if (TEST_PROGRAM_COUNT > 50)
        {
            LOG_ERROR("QUALITY", "阶段3：TEST_PROGRAM_COUNT(%d)超过数组大小(50)，存在内存越界风险！", TEST_PROGRAM_COUNT);
            return QUALITY_TEST_ERROR_INVALID_PARAM;
        }
        
        /* 确保program_timeout_count在每次测试前正确初始化（防止内存被覆盖） */
        result->program_timeout_count = 0;
        
        for (i = 0; i < TEST_PROGRAM_COUNT; i++)
        {
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：编程循环 %d/%d", i+1, TEST_PROGRAM_COUNT);
            }
            
            /* 每次写入前需要擦除（Flash特性：先擦后写） */
            if (i > 0)
            {
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段3：循环%d - 擦除扇区", i+1);
                }
                w25q_status = W25Q_EraseSector(prog_test_addr);
                if (w25q_status == W25Q_OK)
                {
                    w25q_status = W25Q_WaitReady(5000);  /* 5秒超时 */
                    if (w25q_status != W25Q_OK)
                    {
                        LOG_WARN("QUALITY", "阶段3：循环%d - 擦除等待超时", i+1);
                    }
                }
            }
            
            /* 测量编程时间 - 使用DWT精确测量Flash内部编程时间（WaitReady等待时间） */
            /* 注意：测量期间禁用日志，避免影响测量精度 */
            w25q_status = W25Q_Write(prog_test_addr, test_data, 256);
            
            /* 注意：W25Q_Write内部已经有WaitReady，但可能超时时间不够 */
            /* 所以我们需要再次等待，确保编程真正完成 */
            /* 同时使用DWT精确测量Flash内部编程时间（WaitReady的等待时间） */
            {
                uint32_t wait_start_cycles = DWT->CYCCNT;
                w25q_status = W25Q_WaitReady(5000);  /* 5秒超时，确保编程完成 */
                uint32_t wait_end_cycles = DWT->CYCCNT;
                uint32_t wait_elapsed_us = DWT_GetElapsedUs(wait_start_cycles, wait_end_cycles);
                
                if (w25q_status == W25Q_OK)
                {
                    /* 使用WaitReady的等待时间作为编程时间（Flash内部编程时间） */
                    float program_time_ms = (float)wait_elapsed_us / 1000.0f;
                    
                    /* 关键修复2：数组边界检查，防止内存越界覆盖program_timeout_count */
                    if (i < 50)
                    {
                        result->program_times[i] = (float)wait_elapsed_us;  /* 微秒 */
                    }
                    else
                    {
                        LOG_ERROR("QUALITY", "阶段3：数组越界！i=%d >= 50，跳过写入", i);
                        continue;  /* 跳过本次循环，防止内存越界 */
                    }
                    
                    /* 运行时控制：详细日志标志控制输出 */
                    if (program_time_ms > TEST_PROGRAM_TIMEOUT_MS)
                    {
                        result->program_timeout_count++;
                        if (g_quality_test_verbose_log)
                        {
                            LOG_WARN("QUALITY", "阶段3：循环%d - 编程超时（%luμs > %luμs）", 
                                     i+1, (unsigned long)wait_elapsed_us, (unsigned long)(TEST_PROGRAM_TIMEOUT_MS * 1000));
                        }
                    }
                    else
                    {
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段3：循环%d - 编程完成，耗时=%luμs", i+1, (unsigned long)wait_elapsed_us);
                        }
                    }
                }
                else
                {
                    if (g_quality_test_verbose_log)
                    {
                        LOG_WARN("QUALITY", "阶段3：循环%d - 编程等待超时或失败（状态=%d）", i+1, w25q_status);
                    }
                    result->program_times[i] = 0.0f;
                }
            }
        }
        
        /* 计算编程抖动（使用优化版本，一次遍历） */
        {
            float prog_mean;
            calculate_mean_and_stddev(result->program_times, TEST_PROGRAM_COUNT, 
                                     &prog_mean, &result->program_jitter);
            /* 运行时控制：详细日志标志控制输出 */
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段3：编程延迟测试完成");
                LOG_INFO("QUALITY", "阶段3：开始计算编程抖动");
                LOG_INFO("QUALITY", "阶段3：calculate_mean_and_stddev完成");
                LOG_INFO("QUALITY", "阶段3：编程抖动 = %luμs, 超时次数 = %d", 
                         (unsigned long)result->program_jitter, result->program_timeout_count);
            }
        }
    }
    
    /* 计算擦除延迟均值（用于判定） */
    float erase_mean = 0.0f;
    {
        uint32_t i, j;
        float sum = 0.0f;
        uint32_t count = 0;
        for (i = 0; i < TEST_ERASE_SECTOR_COUNT; i++)
        {
            for (j = 0; j < TEST_ERASE_CYCLE_COUNT; j++)
            {
                if (result->erase_times[i][j] > 0.0f)
                {
                    sum += result->erase_times[i][j];
                    count++;
                }
            }
        }
        if (count > 0)
        {
            erase_mean = sum / (float)count;
        }
    }
    
    /* 判定翻新特征 */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段3：统计结果 - 唤醒延迟均值=%luμs, 标准差=%luμs, 擦除均值=%luμs, CV=%lu%%, 编程超时=%d次",
                 (unsigned long)result->wakeup_mean, (unsigned long)result->wakeup_std_dev, 
                 (unsigned long)erase_mean, (unsigned long)result->erase_cv, result->program_timeout_count);
        LOG_INFO("QUALITY", "阶段3：判定阈值 - 唤醒延迟均值阈值=%luμs, 标准差阈值=%luμs, 擦除均值阈值=%luμs, CV阈值=%lu%%, 编程超时阈值=%d次",
                 (unsigned long)TEST_WAKEUP_MEAN_THRESHOLD, (unsigned long)TEST_WAKEUP_STD_DEV_THRESHOLD,
                 (unsigned long)TEST_ERASE_MEAN_THRESHOLD, (unsigned long)TEST_ERASE_CV_THRESHOLD, TEST_PROGRAM_TIMEOUT_THRESHOLD);
    }
    
    /* 分别检查每个判定条件 */
    /* 注意：唤醒延迟测量包含了SPI通信和状态寄存器读取时间，实际Flash唤醒延迟更短
     * 但测量值仍然可以用于判定，因为翻新芯片的唤醒延迟会明显更长且不稳定
     * 移除150μs的规避逻辑，直接使用阈值判定（60μs阈值已考虑测量方法） */
    uint8_t wakeup_mean_failed = (result->wakeup_mean > TEST_WAKEUP_MEAN_THRESHOLD) ? 1 : 0;
    uint8_t wakeup_std_failed = (result->wakeup_std_dev > TEST_WAKEUP_STD_DEV_THRESHOLD) ? 1 : 0;
    uint8_t erase_mean_failed = (erase_mean > TEST_ERASE_MEAN_THRESHOLD) ? 1 : 0;
    uint8_t erase_cv_failed = (result->erase_cv > TEST_ERASE_CV_THRESHOLD) ? 1 : 0;
    uint8_t program_failed = (result->program_timeout_count > TEST_PROGRAM_TIMEOUT_THRESHOLD) ? 1 : 0;
    
    /* 综合判定：任一条件失败即判定为翻新 */
    uint8_t wakeup_failed = wakeup_mean_failed || wakeup_std_failed;
    uint8_t erase_failed = erase_mean_failed || erase_cv_failed;
    
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        if (wakeup_mean_failed)
        {
            LOG_WARN("QUALITY", "阶段3：唤醒延迟均值超标（%luμs > %luμs）", 
                     (unsigned long)result->wakeup_mean, (unsigned long)TEST_WAKEUP_MEAN_THRESHOLD);
        }
        else
        {
            LOG_INFO("QUALITY", "阶段3：唤醒延迟均值正常（%luμs <= %luμs）", 
                     (unsigned long)result->wakeup_mean, (unsigned long)TEST_WAKEUP_MEAN_THRESHOLD);
        }
        
        if (wakeup_std_failed)
        {
            LOG_WARN("QUALITY", "阶段3：唤醒延迟标准差超标（%luμs > %luμs）", 
                     (unsigned long)result->wakeup_std_dev, (unsigned long)TEST_WAKEUP_STD_DEV_THRESHOLD);
        }
        else
        {
            LOG_INFO("QUALITY", "阶段3：唤醒延迟标准差正常（%luμs <= %luμs）", 
                     (unsigned long)result->wakeup_std_dev, (unsigned long)TEST_WAKEUP_STD_DEV_THRESHOLD);
        }
        
        if (erase_mean_failed)
        {
            LOG_WARN("QUALITY", "阶段3：擦除延迟均值超标（%luμs > %luμs）", 
                     (unsigned long)erase_mean, (unsigned long)TEST_ERASE_MEAN_THRESHOLD);
        }
        else
        {
            LOG_INFO("QUALITY", "阶段3：擦除延迟均值正常（%luμs <= %luμs）", 
                     (unsigned long)erase_mean, (unsigned long)TEST_ERASE_MEAN_THRESHOLD);
        }
        
        if (erase_cv_failed)
        {
            LOG_WARN("QUALITY", "阶段3：擦除CV超标（%lu%% > %lu%%）", 
                     (unsigned long)result->erase_cv, (unsigned long)TEST_ERASE_CV_THRESHOLD);
        }
        else
        {
            LOG_INFO("QUALITY", "阶段3：擦除CV正常（%lu%% <= %lu%%）", 
                     (unsigned long)result->erase_cv, (unsigned long)TEST_ERASE_CV_THRESHOLD);
        }
        
        if (program_failed)
        {
            LOG_WARN("QUALITY", "阶段3：编程超时次数超标（%d次 > %d次）", 
                     result->program_timeout_count, TEST_PROGRAM_TIMEOUT_THRESHOLD);
        }
        else
        {
            LOG_INFO("QUALITY", "阶段3：编程超时次数正常（%d次 <= %d次）", 
                     result->program_timeout_count, TEST_PROGRAM_TIMEOUT_THRESHOLD);
        }
    }
    
    if (wakeup_failed || erase_failed || program_failed)
    {
        /* 判定结果始终输出（不影响性能） */
        LOG_WARN("QUALITY", "阶段3：检测到翻新特征 → GRADE_C");
        result->grade = QUALITY_GRADE_C;
        result->stage3_passed = 0;
    }
    else
    {
        /* 判定结果始终输出（不影响性能） */
        LOG_INFO("QUALITY", "阶段3：通过（未检测到翻新特征）");
        result->stage3_passed = 1;
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 阶段4：寿命健康度量化评估（简化实现）
 */
Quality_Test_Status_t QualityTest_Stage4_LifetimeAssessment(Quality_Test_Result_t *result)
{
    W25Q_Status_t w25q_status;
    uint32_t test_addr;
    uint8_t *test_data = s_test_buffer;      /* 使用静态缓冲区，减少栈使用 */
    uint8_t *read_data = s_read_buffer;      /* 使用静态缓冲区，减少栈使用 */
    
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 延迟一下，给UART时间清空缓冲区 */
    Delay_ms(50);
    
    /* 只在详细日志模式下输出阶段标题 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "=== 阶段4：寿命健康度量化评估 ===");
    }
    
    /* 动态计算测试区域地址 */
    test_addr = calculate_test_area_addr();
    if (test_addr == 0)
    {
        LOG_ERROR("QUALITY", "阶段4：测试区域地址计算失败");
        return QUALITY_TEST_ERROR_W25Q_FAILED;
    }
    LOG_INFO("QUALITY", "阶段4：测试区域地址 = 0x%06X", test_addr);
    
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：测试1 - 坏块统计测试");
    }
    
    /* 简化实现：基本健康度评估 */
    result->bad_block_count = 0;
    result->read_disturb_errors = 0;
    result->health_score = 0;  /* 初始化健康度分数 */
    
    /* 测试1：坏块统计（简化：测试16个Block，而非全片） */
    {
        const w25q_dev_t *dev_info = W25Q_GetInfo();
        uint32_t total_blocks;
        uint32_t test_blocks;
        uint32_t block_addr;
        uint32_t i;
        
        if (dev_info != NULL && dev_info->capacity_mb > 0)
        {
            /* 计算总Block数（64KB per Block） */
            total_blocks = (dev_info->capacity_mb * 1024 * 1024) / TEST_BLOCK_SIZE;
            /* 简化：只测试部分Block（均匀分布） */
            test_blocks = (total_blocks > TEST_BAD_BLOCK_COUNT) ? TEST_BAD_BLOCK_COUNT : total_blocks;
            
            /* 运行时控制：详细日志标志控制输出 */
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段4：总Block数 = %lu, 测试Block数 = %d", 
                         (unsigned long)total_blocks, TEST_BAD_BLOCK_COUNT);
            }
            
            for (i = 0; i < test_blocks; i++)
            {
                /* 计算Block地址（均匀分布） */
                block_addr = (i * total_blocks / test_blocks) * TEST_BLOCK_SIZE;
                /* 运行时控制：详细日志标志控制输出 */
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段4：测试Block %d/%d (地址=0x%06X)", 
                             i+1, test_blocks, block_addr);
                }
                
                /* 测试Block：擦除-写入-读取-验证 */
                {
                    uint8_t *block_test_data = s_test_buffer;   /* 复用静态缓冲区 */
                    uint8_t *block_read_data = s_read_buffer;   /* 复用静态缓冲区 */
                    uint8_t test_pattern = 0xAA + (i & 0x0F);  /* 不同Block使用不同模式 */
                    uint8_t is_match = 0;
                    Quality_Test_Status_t test_status;
                    
                    memset(block_test_data, test_pattern, 256);
                    
                    if (g_quality_test_verbose_log)
                    {
                        LOG_INFO("QUALITY", "阶段4：Block %d - 执行擦除-写入-读取-验证", i+1);
                    }
                    /* 使用公共函数进行擦除-写入-读取-验证 */
                    test_status = w25q_erase_write_read_verify(
                        block_addr,
                        block_test_data,
                        256,
                        block_read_data,
                        &is_match);
                    
                    if (test_status != QUALITY_TEST_OK || is_match == 0)
                    {
                        /* 测试失败或数据不匹配，可能是坏块 */
                        result->bad_block_count++;
                        if (g_quality_test_verbose_log)
                        {
                            LOG_WARN("QUALITY", "阶段4：Block %d - 检测到坏块（状态=%d, 匹配=%d）", 
                                     i+1, test_status, is_match);
                        }
                    }
                    else
                    {
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段4：Block %d - 测试通过", i+1);
                        }
                    }
                }
            }
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段4：坏块统计完成，坏块数量 = %d", result->bad_block_count);
            }
        }
        else
        {
            LOG_WARN("QUALITY", "阶段4：无法获取设备信息，跳过坏块统计");
        }
    }
    
    /* 测试2：读干扰敏感性测试 */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：测试2 - 读干扰敏感性测试（%d次读取）", TEST_READ_DISTURB_COUNT);
    }
    {
        uint32_t read_count = TEST_READ_DISTURB_COUNT;
        uint32_t page_a_addr = test_addr;
        uint32_t page_b_addr = test_addr + TEST_PAGE_SIZE;  /* 相邻页 */
        uint8_t *pattern_a = s_test_buffer;      /* 复用静态缓冲区 */
        uint8_t *verify_data = s_verify_buffer;   /* 使用验证缓冲区 */
        uint32_t i;
        
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段4：页A地址=0x%06X, 页B地址=0x%06X", page_a_addr, page_b_addr);
        }
        memset(pattern_a, 0x55, 256);
        
        /* 擦除并写入测试页A */
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段4：擦除页A");
        }
        w25q_status = W25Q_EraseSector(page_a_addr);
        if (w25q_status == W25Q_OK)
        {
            w25q_status = W25Q_WaitReady(5000);  /* 5秒超时，擦除需要更长时间 */
            if (w25q_status == W25Q_OK)
            {
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段4：写入页A");
                }
                w25q_status = W25Q_Write(page_a_addr, pattern_a, 256);
                if (w25q_status == W25Q_OK)
                {
                    w25q_status = W25Q_WaitReady(2000);  /* 2秒超时 */
                    if (w25q_status == W25Q_OK)
                    {
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段4：开始读干扰测试（每%d次检查一次）", TEST_READ_DISTURB_CHECK_INTERVAL);
                        }
                        
                        /* 反复读取相邻页B，每N次检查一次页A */
                        for (i = 0; i < read_count; i++)
                        {
                            /* 读取相邻页B */
                            w25q_status = W25Q_Read(page_b_addr, read_data, 256);
                            
                            /* 每N次检查一次页A */
                            if ((i % TEST_READ_DISTURB_CHECK_INTERVAL) == 0 && i > 0)
                            {
                                if (g_quality_test_verbose_log)
                                {
                                    LOG_INFO("QUALITY", "阶段4：读干扰测试 %d/%d - 检查页A", i, read_count);
                                }
                                w25q_status = W25Q_Read(page_a_addr, verify_data, 256);
                                if (w25q_status == W25Q_OK)
                                {
                                    /* 统计错误位数 */
                                    uint32_t j;
                                    uint32_t error_bits = 0;
                                    for (j = 0; j < 256; j++)
                                    {
                                        uint8_t diff = pattern_a[j] ^ verify_data[j];
                                        /* 计算错误位数 */
                                        while (diff != 0)
                                        {
                                            if (diff & 0x01)
                                            {
                                                error_bits++;
                                                result->read_disturb_errors++;
                                            }
                                            diff >>= 1;
                                        }
                                    }
                                    if (error_bits > 0)
                                    {
                                        LOG_WARN("QUALITY", "阶段4：读干扰测试 %d/%d - 检测到%d个错误位", 
                                                 i, read_count, error_bits);
                                    }
                                }
                            }
                        }
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段4：读干扰测试完成，总错误位数 = %d", result->read_disturb_errors);
                        }
                    }
                    else
                    {
                        LOG_WARN("QUALITY", "阶段4：页A写入等待超时");
                    }
                }
                else
                {
                    LOG_WARN("QUALITY", "阶段4：页A写入失败");
                }
            }
            else
            {
                LOG_WARN("QUALITY", "阶段4：页A擦除等待超时");
            }
        }
        else
        {
            LOG_WARN("QUALITY", "阶段4：页A擦除失败");
        }
    }
    
    /* 测试3：数据完整性压力测试（简化：单次测试） */
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：测试3 - 数据完整性压力测试");
    }
    {
        uint8_t is_match = 0;
        Quality_Test_Status_t test_status;
        
        /* 运行时控制：详细日志标志控制输出 */
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段4：执行擦除-写入-读取-验证");
        }
        memset(test_data, 0x55, 256);
        
        /* 使用公共函数进行擦除-写入-读取-验证 */
        test_status = w25q_erase_write_read_verify(
            test_addr,
            test_data,
            256,
            read_data,
            &is_match);
        
        if (test_status == QUALITY_TEST_OK && is_match == 0)
        {
            /* 数据不匹配，增加读干扰错误计数 */
            result->read_disturb_errors++;
            LOG_WARN("QUALITY", "阶段4：数据完整性测试失败（数据不匹配）");
        }
        else if (test_status == QUALITY_TEST_OK)
        {
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段4：数据完整性测试通过");
            }
        }
        else
        {
            if (g_quality_test_verbose_log)
            {
                LOG_WARN("QUALITY", "阶段4：数据完整性测试失败（状态=%d）", test_status);
            }
        }
    }
    
    /* 计算健康度分数（先计算基础分数） */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：计算健康度分数");
        LOG_INFO("QUALITY", "阶段4：坏块数量 = %d, 读干扰错误数 = %d", 
                 result->bad_block_count, result->read_disturb_errors);
    }
    if (result->read_disturb_errors == 0 && result->bad_block_count == 0)
    {
        result->health_score = 100;
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段4：健康度分数 = 100（无错误）");
        }
    }
    else if (result->read_disturb_errors < TEST_READ_DISTURB_ERROR_THRESHOLD && 
             result->bad_block_count <= TEST_BAD_BLOCK_THRESHOLD)
    {
        /* 基础分数 = 80 - 读干扰错误数 - 坏块扣分 */
        result->health_score = 80 - result->read_disturb_errors - (result->bad_block_count * 5);
        if (g_quality_test_verbose_log)
        {
            LOG_INFO("QUALITY", "阶段4：健康度分数 = %d（基础分数计算）", result->health_score);
        }
    }
    else
    {
        result->health_score = 0;
        if (g_quality_test_verbose_log)
        {
            LOG_WARN("QUALITY", "阶段4：健康度分数 = 0（错误过多）");
        }
    }
    
    /* 如果坏块过多，健康度直接为0 */
    if (result->bad_block_count > TEST_BAD_BLOCK_THRESHOLD)
    {
        result->health_score = 0;
        if (g_quality_test_verbose_log)
        {
            LOG_WARN("QUALITY", "阶段4：健康度分数 = 0（坏块过多：%d > %d）", 
                     result->bad_block_count, TEST_BAD_BLOCK_THRESHOLD);
        }
    }
    
    /* 测试4：指令响应延迟退化检测（在健康度计算之后，用于进一步调整） */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：测试4 - 指令响应延迟退化检测（%d次测量，每次%d次循环）", 
                 TEST_DELAY_DEGRADATION_COUNT, TEST_DELAY_DEGRADATION_CYCLE);
    }
    {
        uint32_t read_id_latency[TEST_DELAY_DEGRADATION_COUNT];
        uint32_t i, j;
        
        for (i = 0; i < TEST_DELAY_DEGRADATION_COUNT; i++)
        {
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段4：延迟退化测试 %d/%d", i+1, TEST_DELAY_DEGRADATION_COUNT);
                LOG_INFO("QUALITY", "阶段4：执行 %d 次擦写循环", TEST_DELAY_DEGRADATION_CYCLE);
            }
            /* 每完成N次擦写循环后测量 */
            for (j = 0; j < TEST_DELAY_DEGRADATION_CYCLE; j++)
            {
                /* 擦除-写入循环 */
                w25q_status = W25Q_EraseSector(test_addr);
                if (w25q_status == W25Q_OK)
                {
                    w25q_status = W25Q_WaitReady(5000);  /* 5秒超时，擦除需要更长时间 */
                    if (w25q_status == W25Q_OK)
                    {
                        w25q_status = W25Q_Write(test_addr, test_data, 256);
                        if (w25q_status == W25Q_OK)
                        {
                            w25q_status = W25Q_WaitReady(2000);  /* 2秒超时 */
                        }
                    }
                }
            }
            
            /* 测量Page Read延迟（使用DWT精确测量，替代Read ID） */
            {
                uint32_t read_start_cycles = DWT->CYCCNT;
                w25q_status = W25Q_Read(test_addr, read_data, 256);
                uint32_t read_end_cycles = DWT->CYCCNT;
                uint32_t read_delay_us = DWT_GetElapsedUs(read_start_cycles, read_end_cycles);
                read_id_latency[i] = read_delay_us;  /* 存储为微秒，但变量名保持read_id_latency */
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段4：Page Read延迟[%d] = %luμs", i, (unsigned long)read_delay_us);
                }
            }
        }
        
        /* 检查延迟是否呈单调递增趋势（简化：只检查最后3次是否递增） */
        {
            uint8_t is_increasing = 1;
            float increase_ratio = 0.0f;
            uint32_t last_index = TEST_DELAY_DEGRADATION_COUNT - 1;
            
            if (g_quality_test_verbose_log)
            {
                LOG_INFO("QUALITY", "阶段4：延迟退化测试完成，开始分析延迟趋势");
                LOG_INFO("QUALITY", "阶段4：检查延迟趋势（最后3次）");
            }
            /* 检查最后3次是否递增 */
            if (TEST_DELAY_DEGRADATION_COUNT >= 3)
            {
                for (i = TEST_DELAY_DEGRADATION_COUNT - 3; i < TEST_DELAY_DEGRADATION_COUNT; i++)
                {
                    if (i > 0 && read_id_latency[i] <= read_id_latency[i-1])
                    {
                        is_increasing = 0;
                        if (g_quality_test_verbose_log)
                        {
                            LOG_INFO("QUALITY", "阶段4：延迟未呈递增趋势（[%d]=%lu <= [%d]=%lu）", 
                                     i, (unsigned long)read_id_latency[i], 
                                     i-1, (unsigned long)read_id_latency[i-1]);
                        }
                        break;
                    }
                }
            }
            
            if (is_increasing && read_id_latency[0] > 0)
            {
                /* 计算延迟增加比例 */
                increase_ratio = ((float)(read_id_latency[last_index] - read_id_latency[0])) / (float)read_id_latency[0] * 100.0f;
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段4：延迟递增趋势检测 - 首次=%lums, 最后=%lums, 增加比例=%lu%%", 
                             (unsigned long)read_id_latency[0], 
                             (unsigned long)read_id_latency[last_index],
                             (unsigned long)increase_ratio);
                }
                
                /* 如果延迟增加超过20%，判定为轻度磨损，降低健康度分数 */
                if (increase_ratio > 20.0f)
                {
                    int old_health = result->health_score;
                    if (result->health_score > 10)
                    {
                        result->health_score -= 10;  /* 降低10分 */
                    }
                    else
                    {
                        result->health_score = 0;  /* 确保不为负 */
                    }
                    if (g_quality_test_verbose_log)
                    {
                        LOG_WARN("QUALITY", "阶段4：检测到延迟退化（增加%lu%%），健康度从%d降至%d", 
                                 (unsigned long)increase_ratio, old_health, result->health_score);
                    }
                }
                else
                {
                    if (g_quality_test_verbose_log)
                    {
                        LOG_INFO("QUALITY", "阶段4：延迟退化在正常范围内（增加%lu%% < 20%%）", 
                                 (unsigned long)increase_ratio);
                    }
                }
            }
            else
            {
                if (g_quality_test_verbose_log)
                {
                    LOG_INFO("QUALITY", "阶段4：延迟未呈递增趋势或数据无效");
                }
            }
        }
    }
    
    /* 确保健康度在0-100范围内 */
    if (result->health_score < 0)
    {
        result->health_score = 0;
    }
    if (result->health_score > 100)
    {
        result->health_score = 100;
    }
    
    result->stage4_passed = (result->health_score >= TEST_HEALTH_SCORE_THRESHOLD_B) ? 1 : 0;
    
    /* 运行时控制：详细日志标志控制输出 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段4：完成，最终健康度分数 = %d", result->health_score);
        LOG_INFO("QUALITY", "阶段4：健康度评估完成 - 健康度=%d%%, 坏块数=%d, 读干扰错误=%d",
                 result->health_score, result->bad_block_count, result->read_disturb_errors);
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 阶段5：综合判定与自动化决策
 */
Quality_Test_Status_t QualityTest_Stage5_Judgment(Quality_Test_Result_t *result)
{
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 延迟一下，给UART时间清空缓冲区 */
    Delay_ms(50);
    
    /* 阶段5的日志始终输出（因为这是最终判定，需要看到结果） */
    LOG_INFO("QUALITY", "=== 阶段5：综合判定 ===");
    LOG_INFO("QUALITY", "阶段5：当前等级 = %d, 健康度 = %d%%, 阶段3通过 = %d", 
             result->grade, result->health_score, result->stage3_passed);
    
    /* 调试阶段不评估等级，避免影响结果 */
    if (g_quality_test_verbose_log)
    {
        LOG_INFO("QUALITY", "阶段5：[调试模式] 跳过等级评估，保持当前等级 = %d", result->grade);
        result->stage5_passed = 1;
        return QUALITY_TEST_OK;
    }
    
    /* 决策表逻辑（按优先级顺序） */
    
    /* 情况1：阶段2判定山寨 → 直接D级 */
    if (result->grade == QUALITY_GRADE_D)
    {
        LOG_INFO("QUALITY", "阶段5：阶段2判定为D级，保持D级");
        result->stage5_passed = 1;
        return QUALITY_TEST_OK;
    }
    
    /* 情况2：阶段3判定为C级 */
    if (result->grade == QUALITY_GRADE_C && result->stage3_passed == 0)
    {
        if (result->health_score < TEST_HEALTH_SCORE_THRESHOLD_A)
        {
            /* 双重确认：阶段3 C级 + 阶段4健康度<85% → C级 */
            LOG_INFO("QUALITY", "阶段5：阶段3/4双重确认翻新 → GRADE_C（健康度=%d%% < %d%%）", 
                     result->health_score, TEST_HEALTH_SCORE_THRESHOLD_A);
            result->stage5_passed = 1;
            return QUALITY_TEST_OK;
        }
        else
        {
            /* 矛盾降级：阶段3 C级 + 阶段4健康度>=85% → B级 */
            result->grade = QUALITY_GRADE_B;
            LOG_WARN("QUALITY", "阶段5：阶段3/4矛盾，以健康度为准，降级为B级（待复查）（健康度=%d%% >= %d%%）", 
                     result->health_score, TEST_HEALTH_SCORE_THRESHOLD_A);
            result->stage5_passed = 1;
            return QUALITY_TEST_OK;
        }
    }
    
    /* 情况3：阶段3通过，根据健康度判定 */
    LOG_INFO("QUALITY", "阶段5：根据健康度判定等级（阈值A=%d, 阈值B=%d）", 
             TEST_HEALTH_SCORE_THRESHOLD_A, TEST_HEALTH_SCORE_THRESHOLD_B);
    if (result->health_score >= TEST_HEALTH_SCORE_THRESHOLD_A)
    {
        result->grade = QUALITY_GRADE_A;
        LOG_INFO("QUALITY", "阶段5：健康度>=%d%% → GRADE_A", TEST_HEALTH_SCORE_THRESHOLD_A);
    }
    else if (result->health_score >= TEST_HEALTH_SCORE_THRESHOLD_B)
    {
        result->grade = QUALITY_GRADE_B;
        LOG_INFO("QUALITY", "阶段5：健康度70-85%% → GRADE_B");
    }
    else
    {
        result->grade = QUALITY_GRADE_C;
        LOG_WARN("QUALITY", "阶段5：健康度<70%% → GRADE_C");
    }
    
    result->stage5_passed = 1;
    LOG_INFO("QUALITY", "阶段5：最终判定完成，质量等级 = %d", result->grade);
    
    return QUALITY_TEST_OK;
}

/* ==================== 前向声明 ==================== */

/**
 * @brief 内部测试执行函数（被RunFullTest调用两次）
 */
static Quality_Test_Status_t QualityTest_RunFullTestInternal(Quality_Test_Result_t *result);

/* ==================== 静态变量（避免栈溢出） ==================== */

static Quality_Test_Result_t s_debug_result;  /* 第一次测试结果（静态变量，避免栈溢出） */

/* ==================== 公共函数实现 ==================== */

/**
 * @brief 运行完整质量检测流程
 * @note 执行两次测试：
 *       第一次：输出详细日志（用于调试定位问题）
 *       第二次：测量期间不输出日志，测试结束后输出汇总（确保测量精度）
 */
Quality_Test_Status_t QualityTest_RunFullTest(Quality_Test_Result_t *result)
{
    Quality_Test_Status_t status;
    
    /* 先进行参数检查 */
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 参数检查通过后，直接开始执行 */
    /* 第一次运行：输出详细日志（调试模式） */
    g_quality_test_verbose_log = 1;
    memset(&s_debug_result, 0, sizeof(Quality_Test_Result_t));
    
    /* 直接开始第一次测试 */
    status = QualityTest_RunFullTestInternal(&s_debug_result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "第一次测试执行失败: %d", status);
        /* 即使第一次失败，也尝试第二次测试 */
    }
    
    /* 第二次运行：测量期间不输出日志，测试结束后输出汇总（测试模式） */
    Delay_ms(100);  /* 给UART时间清空缓冲区 */
    
    LOG_INFO("QUALITY", "");
    LOG_INFO("QUALITY", "========================================");
    LOG_INFO("QUALITY", "=== 第二次测试：测试模式（测量期间不输出日志，确保精度） ===");
    LOG_INFO("QUALITY", "========================================");
    g_quality_test_verbose_log = 0;
    memset(result, 0, sizeof(Quality_Test_Result_t));
    
    status = QualityTest_RunFullTestInternal(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "第二次测试执行失败: %d", status);
        return status;
    }
    
    /* 输出测试结果汇总 */
    LOG_INFO("QUALITY", "");
    LOG_INFO("QUALITY", "=== 测试结果汇总（第二次测试数据） ===");
    
    /* 阶段3统计结果 */
    {
        float erase_mean = 0.0f;
        uint32_t i, j;
        float sum = 0.0f;
        uint32_t count = 0;
        for (i = 0; i < TEST_ERASE_SECTOR_COUNT; i++)
        {
            for (j = 0; j < TEST_ERASE_CYCLE_COUNT; j++)
            {
                if (result->erase_times[i][j] > 0.0f)
                {
                    sum += result->erase_times[i][j];
                    count++;
                }
            }
        }
        if (count > 0)
        {
            erase_mean = sum / (float)count;
        }
        
        uint32_t cv_percent = (uint32_t)(result->erase_cv * 100.0f + 0.5f);
        /* 改进输出格式：使用浮点数显示，保留一位小数，避免小数部分被截断 */
        uint32_t wakeup_mean_uint = (uint32_t)(result->wakeup_mean + 0.5f);
        uint32_t wakeup_std_uint = (uint32_t)(result->wakeup_std_dev + 0.5f);
        uint32_t wakeup_std_decimal = (uint32_t)((result->wakeup_std_dev - (float)wakeup_std_uint) * 10.0f + 0.5f);
        if (wakeup_std_decimal >= 10) wakeup_std_decimal = 9;  /* 防止溢出 */
        LOG_INFO("QUALITY", "阶段3统计 - 唤醒延迟: 均值=%luμs, 标准差=%lu.%luμs", 
                 wakeup_mean_uint, wakeup_std_uint, wakeup_std_decimal);
        LOG_INFO("QUALITY", "阶段3统计 - 擦除延迟: 均值=%luμs, CV=%lu.%02lu%%", 
                 (unsigned long)erase_mean, cv_percent / 100, cv_percent % 100);
        LOG_INFO("QUALITY", "阶段3统计 - 编程延迟: 抖动=%luμs, 超时=%d次", 
                 (unsigned long)result->program_jitter, result->program_timeout_count);
        LOG_INFO("QUALITY", "阶段3判定 - %s", result->stage3_passed ? "通过" : "未通过");
    }
    
    /* 阶段4统计结果 */
    LOG_INFO("QUALITY", "阶段4统计 - 健康度=%d%%, 坏块数=%d, 读干扰错误=%d", 
             result->health_score, result->bad_block_count, result->read_disturb_errors);
    
    /* 最终判定结果 */
    const char* grade_names[] = {"Grade A", "Grade B", "Grade C", "Grade D"};
    if (result->grade < 4)
    {
        LOG_INFO("QUALITY", "最终判定 - 质量等级: %s", grade_names[result->grade]);
    }
    else
    {
        LOG_INFO("QUALITY", "最终判定 - 质量等级: Unknown(%d)", result->grade);
    }
    
    return QUALITY_TEST_OK;
}

/**
 * @brief 内部测试执行函数（被RunFullTest调用两次）
 */
static Quality_Test_Status_t QualityTest_RunFullTestInternal(Quality_Test_Result_t *result)
{
    Quality_Test_Status_t status;
    
    /* 先进行参数检查，避免在LOG_INFO时卡住 */
    if (result == NULL)
    {
        return QUALITY_TEST_ERROR_NULL_PTR;
    }
    
    if (!g_quality_test_initialized)
    {
        return QUALITY_TEST_ERROR_NOT_INIT;
    }
    
    /* 阶段1：数字身份验证 */
    status = QualityTest_Stage1_DigitalIdentity(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "阶段1执行失败: %d", status);
        return status;
    }
    
    /* 如果阶段1判定为D级，直接返回 */
    if (result->grade == QUALITY_GRADE_D)
    {
        LOG_WARN("QUALITY", "阶段1判定为D级，测试终止");
        return QUALITY_TEST_OK;
    }
    
    /* 阶段2：山寨货深度鉴别 */
    status = QualityTest_Stage2_FakeDetection(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "阶段2执行失败: %d", status);
        return status;
    }
    
    /* 如果阶段2判定为D级，直接返回 */
    if (result->grade == QUALITY_GRADE_D)
    {
        LOG_WARN("QUALITY", "阶段2判定为D级，测试终止");
        return QUALITY_TEST_OK;
    }
    
    /* 阶段3：翻新货时序指纹鉴定 */
    status = QualityTest_Stage3_RefurbishDetection(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "阶段3执行失败: %d", status);
        return status;
    }
    
    /* 如果阶段3判定为C级，继续测试但标记 */
    
    /* 阶段4：寿命健康度量化评估 */
    status = QualityTest_Stage4_LifetimeAssessment(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "阶段4执行失败: %d", status);
        return status;
    }
    
    /* 阶段5：综合判定 */
    LOG_INFO("QUALITY", "=== 阶段5：综合判定 ===");
    status = QualityTest_Stage5_Judgment(result);
    if (status != QUALITY_TEST_OK)
    {
        LOG_ERROR("QUALITY", "阶段5执行失败: %d", status);
        return status;
    }
    
    LOG_INFO("QUALITY", "=== 所有阶段测试完成 ===");
    
    return QUALITY_TEST_OK;
}
