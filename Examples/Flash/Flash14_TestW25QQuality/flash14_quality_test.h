/**
 * @file flash14_quality_test.h
 * @brief Flash14质量检测模块接口声明
 * @version 1.0.0
 * @date 2024-01-01
 * @details W25Q系列Flash芯片纯软件质量检测流程，包括自动化到货抽检、山寨货鉴别、翻新货鉴定和寿命健康度评估
 */

#ifndef FLASH14_QUALITY_TEST_H
#define FLASH14_QUALITY_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "w25q_spi.h"
#include <stdint.h>

/* ==================== 质量等级定义 ==================== */

/**
 * @brief 质量等级枚举
 */
typedef enum {
    QUALITY_GRADE_A = 0,  /**< 优质品（健康度>85） */
    QUALITY_GRADE_B = 1,  /**< 轻度磨损（健康度70-85） */
    QUALITY_GRADE_C = 2,  /**< 高风险/翻新（健康度<70或翻新特征） */
    QUALITY_GRADE_D = 3   /**< 山寨货（直接退货） */
} Quality_Grade_t;

/* ==================== 运行时日志控制 ==================== */

/**
 * @brief 运行时日志输出控制标志
 * @note 使用静态变量控制，第一次运行输出详细日志，第二次运行测量期间不输出日志
 */
extern uint8_t g_quality_test_verbose_log;  /**< 详细日志标志：1=输出详细日志，0=仅输出汇总 */

/* ==================== 错误码定义 ==================== */

/**
 * @brief 质量检测模块错误码基值
 * @note 案例层模块，使用临时基值
 */
#define ERROR_BASE_QUALITY_TEST -5000

/**
 * @brief 质量检测状态枚举
 */
typedef enum {
    QUALITY_TEST_OK = ERROR_OK,                                    /**< 操作成功 */
    QUALITY_TEST_ERROR_NOT_INIT = ERROR_BASE_QUALITY_TEST - 1,     /**< 模块未初始化 */
    QUALITY_TEST_ERROR_NULL_PTR = ERROR_BASE_QUALITY_TEST - 2,     /**< 空指针错误 */
    QUALITY_TEST_ERROR_INVALID_PARAM = ERROR_BASE_QUALITY_TEST - 3, /**< 无效参数 */
    QUALITY_TEST_ERROR_W25Q_FAILED = ERROR_BASE_QUALITY_TEST - 4,  /**< W25Q操作失败 */
    QUALITY_TEST_ERROR_TIMEOUT = ERROR_BASE_QUALITY_TEST - 5,      /**< 超时错误 */
    QUALITY_TEST_ERROR_MEMORY = ERROR_BASE_QUALITY_TEST - 6,      /**< 内存不足 */
    QUALITY_TEST_ERROR_STATISTICS = ERROR_BASE_QUALITY_TEST - 7    /**< 统计计算错误 */
} Quality_Test_Status_t;

/* ==================== 测试结果结构体 ==================== */

/**
 * @brief 质量测试结果结构体
 */
typedef struct {
    /* 身份验证数据 */
    uint32_t jedec_id;              /**< JEDEC ID（制造商ID << 16 | 设备ID） */
    uint64_t unique_id;             /**< Unique ID（64位） */
    uint8_t sfdp[256];              /**< SFDP表数据 */
    uint8_t status_reg[3];          /**< 状态寄存器SR1/SR2/SR3 */
    
    /* 时序数据（简化版，实际使用时可能需要动态分配） */
    float wakeup_delays[100];       /**< 唤醒延迟（简化：100次，而非1000次） */
    float erase_times[16][10];      /**< 擦除延迟（16个Block，每个10次） */
    float program_times[50];        /**< 编程延迟（简化：50次，而非256次） */
    
    /* 统计结果 */
    float wakeup_mean;              /**< 唤醒延迟均值（微秒） */
    float wakeup_std_dev;           /**< 唤醒延迟标准差（微秒） */
    float wakeup_tail_latency;      /**< 唤醒延迟尾部延迟（99th percentile，微秒） */
    float erase_cv;                 /**< 擦除延迟变异系数（%） */
    int program_timeout_count;      /**< 编程超时次数（>1.5ms） */
    float program_jitter;           /**< 编程时间抖动（标准差，微秒） */
    
    /* 健康度评估 */
    int health_score;               /**< 健康度分数（0-100） */
    int bad_block_count;            /**< 坏块数量 */
    int read_disturb_errors;        /**< 读干扰错误数 */
    
    /* 最终判定 */
    Quality_Grade_t grade;           /**< 质量等级 */
    
    /* 测试标志 */
    uint8_t stage1_passed;          /**< 阶段1通过标志 */
    uint8_t stage2_passed;          /**< 阶段2通过标志 */
    uint8_t stage3_passed;          /**< 阶段3通过标志 */
    uint8_t stage4_passed;          /**< 阶段4通过标志 */
    uint8_t stage5_passed;          /**< 阶段5通过标志 */
} Quality_Test_Result_t;

/* ==================== 公共接口函数 ==================== */

/**
 * @brief 质量检测模块初始化
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_Init(void);

/**
 * @brief 运行完整质量检测流程
 * @param[out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 执行所有5个阶段的测试，并自动判定质量等级
 */
Quality_Test_Status_t QualityTest_RunFullTest(Quality_Test_Result_t *result);

/**
 * @brief 阶段1：数字身份验证
 * @param[out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 读取JEDEC ID、Unique ID、SFDP表、状态寄存器
 */
Quality_Test_Status_t QualityTest_Stage1_DigitalIdentity(Quality_Test_Result_t *result);

/**
 * @brief 阶段2：山寨货深度鉴别
 * @param[in,out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 保留位陷阱、非法指令、WPS测试、SFDP校验
 */
Quality_Test_Status_t QualityTest_Stage2_FakeDetection(Quality_Test_Result_t *result);

/**
 * @brief 阶段3：翻新货时序指纹鉴定
 * @param[in,out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 唤醒延迟稳定性、擦除延迟分布、编程抖动、频率压力
 */
Quality_Test_Status_t QualityTest_Stage3_RefurbishDetection(Quality_Test_Result_t *result);

/**
 * @brief 阶段4：寿命健康度量化评估
 * @param[in,out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 读干扰、坏块统计、延迟退化、数据完整性
 */
Quality_Test_Status_t QualityTest_Stage4_LifetimeAssessment(Quality_Test_Result_t *result);

/**
 * @brief 阶段5：综合判定与自动化决策
 * @param[in,out] result 测试结果结构体指针
 * @return Quality_Test_Status_t 错误码
 * @note 根据各阶段测试结果自动判定质量等级
 */
Quality_Test_Status_t QualityTest_Stage5_Judgment(Quality_Test_Result_t *result);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 计算数组均值
 * @param[in] data 数据数组
 * @param[in] count 数据个数
 * @param[out] mean 均值
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_CalculateMean(const float *data, uint32_t count, float *mean);

/**
 * @brief 计算数组标准差
 * @param[in] data 数据数组
 * @param[in] count 数据个数
 * @param[in] mean 均值（如果为0则自动计算）
 * @param[out] std_dev 标准差
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_CalculateStdDev(const float *data, uint32_t count, float mean, float *std_dev);

/**
 * @brief 计算数组变异系数（CV）
 * @param[in] data 数据数组
 * @param[in] count 数据个数
 * @param[out] cv 变异系数（%）
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_CalculateCV(const float *data, uint32_t count, float *cv);

/**
 * @brief 计算数组百分位数
 * @param[in] data 数据数组（需要先排序）
 * @param[in] count 数据个数
 * @param[in] percentile 百分位数（0-100）
 * @param[out] value 百分位数值
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_CalculatePercentile(const float *data, uint32_t count, uint8_t percentile, float *value);

/**
 * @brief 浮点数数组排序（升序）
 * @param[in,out] data 数据数组
 * @param[in] count 数据个数
 * @return Quality_Test_Status_t 错误码
 */
Quality_Test_Status_t QualityTest_SortFloatArray(float *data, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* FLASH14_QUALITY_TEST_H */
