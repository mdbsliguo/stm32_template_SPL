/**
 * @file liter_ring.h
 * @brief 按升明细环形日志（LittleFS 定长记录覆盖写）
 * @details 文件 log/liter_ring.dat：头 + N 条定长记录，满后覆盖最旧
 */

#ifndef LITER_RING_H
#define LITER_RING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include <stdint.h>

#ifdef CONFIG_MODULE_LITER_RING_ENABLED
#if CONFIG_MODULE_LITER_RING_ENABLED

#define LITER_RING_MAGIC            0x31524C50u  /**< 'PLR1' */
#define LITER_RING_VERSION          1u
#define LITER_RING_DEFAULT_REC_SIZE 16u
#define LITER_RING_PATH_MAX         48u
/** Stage4 完成标记（写在文件头 reserved[0]，避免新建 stage4_done.txt） */
#define LITER_RING_STAGE4_DONE_MAGIC  0x53443431u

/**
 * @brief 单条按升记录（16 字节）
 */
typedef struct {
    uint32_t seq;         /**< 全局递增序号 */
    uint32_t liter_x1000; /**< 升数 x1000 */
    uint32_t pulse;       /**< 脉冲计数 */
    uint32_t txn_id;      /**< 交易 ID */
} liter_ring_record_t;

/**
 * @brief 环形日志配置
 */
typedef struct {
    const char *file_path;   /**< 文件路径，如 log/liter_ring.dat */
    uint32_t capacity;       /**< 最大保留条数 */
    uint32_t sync_every;     /**< 每 N 条 FileSync，0=每条都 sync */
} liter_ring_config_t;

/**
 * @brief 环形日志运行状态（只读）
 */
typedef struct {
    uint32_t capacity;
    uint32_t head;
    uint32_t total_writes;
} liter_ring_stats_t;

error_code_t LiterRing_Init(const liter_ring_config_t *config);
error_code_t LiterRing_Deinit(void);
uint8_t LiterRing_IsInitialized(void);

error_code_t LiterRing_Append(const liter_ring_record_t *record);
error_code_t LiterRing_AppendSessionBegin(void);
error_code_t LiterRing_AppendSession(const liter_ring_record_t *record);
error_code_t LiterRing_AppendSessionEnd(void);
error_code_t LiterRing_ReadSlot(uint32_t slot, liter_ring_record_t *record);
error_code_t LiterRing_GetStats(liter_ring_stats_t *stats);
error_code_t LiterRing_ForceSync(void);

error_code_t LiterRing_VerifyWrap(uint32_t expect_total,
                                  uint32_t *oldest_seq,
                                  uint32_t *newest_seq);

error_code_t LiterRing_MarkStage4Done(void);
uint8_t LiterRing_IsStage4Done(void);

#endif /* CONFIG_MODULE_LITER_RING_ENABLED */
#endif /* CONFIG_MODULE_LITER_RING_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* LITER_RING_H */
