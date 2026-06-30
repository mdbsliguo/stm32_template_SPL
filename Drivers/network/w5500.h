/**
 * @file w5500.h
 * @brief W5500以太网控制器驱动模块
 * @version 1.0.0
 * @date 2026-06-30
 * @details 基于SPI的W5500驱动，支持静态IP、TCP/UDP Socket0、轮询中断处理
 */

#ifndef W5500_H
#define W5500_H

#ifdef __cplusplus
extern "C" {
#endif

#include "error_code.h"
#include "config.h"
#include "board.h"
#include <stdint.h>

#ifdef CONFIG_MODULE_W5500_ENABLED
#if CONFIG_MODULE_W5500_ENABLED

#ifdef CONFIG_MODULE_SPI_ENABLED
#if CONFIG_MODULE_SPI_ENABLED
#include "spi_hw.h"
#endif
#endif

/** @brief 支持的Socket数量（当前仅Socket0完整实现） */
#define W5500_SOCKET_MAX       8U
#define W5500_SOCKET_0         0U
#define W5500_MAX_PACKET_SIZE  1460U

/**
 * @brief W5500错误码
 */
typedef enum {
    W5500_OK = ERROR_OK,
    W5500_ERROR_NOT_INIT = ERROR_BASE_W5500 - 1,
    W5500_ERROR_SPI_FAILED = ERROR_BASE_W5500 - 2,
    W5500_ERROR_TIMEOUT = ERROR_BASE_W5500 - 3,
    W5500_ERROR_INVALID_PARAM = ERROR_BASE_W5500 - 4,
    W5500_ERROR_INVALID_SOCKET = ERROR_BASE_W5500 - 5,
    W5500_ERROR_NOT_IMPLEMENTED = ERROR_BASE_W5500 - 99,
    W5500_ERROR_INIT_FAILED = ERROR_BASE_W5500 - 6,
    W5500_ERROR_LINK_DOWN = ERROR_BASE_W5500 - 7,
    W5500_ERROR_GATEWAY = ERROR_BASE_W5500 - 8,
    W5500_ERROR_SOCKET = ERROR_BASE_W5500 - 9,
    W5500_ERROR_DISABLED = ERROR_BASE_W5500 - 10,
} W5500_Status_t;

typedef uint8_t W5500_Socket_t;

/**
 * @brief Socket运行模式
 */
typedef enum {
    W5500_MODE_TCP_SERVER = 0,
    W5500_MODE_TCP_CLIENT = 1,
    W5500_MODE_UDP = 2,
} W5500_SocketMode_t;

/**
 * @brief 网络参数（静态IP）
 */
typedef struct {
    uint8_t gateway[4];
    uint8_t subnet[4];
    uint8_t mac[6];
    uint8_t ip[4];
} W5500_NetConfig_t;

/**
 * @brief Socket参数
 */
typedef struct {
    uint16_t local_port;
    uint8_t dest_ip[4];
    uint16_t dest_port;
} W5500_SocketConfig_t;

/** @brief Socket连接状态标志 */
#define W5500_SOCK_FLAG_INIT     0x01U
#define W5500_SOCK_FLAG_CONN     0x02U

/** @brief Socket事件标志 */
#define W5500_SOCK_EVT_RECEIVE   0x01U
#define W5500_SOCK_EVT_TX_OK     0x02U

/**
 * @brief Socket运行状态
 */
typedef struct {
    uint8_t flags;
    uint8_t events;
} W5500_SocketStatus_t;

W5500_Status_t W5500_Init(void);
W5500_Status_t W5500_Deinit(void);
W5500_Status_t W5500_HardwareReset(void);
W5500_Status_t W5500_PhySoftwareReset(void);
W5500_Status_t W5500_WaitLinkUp(uint32_t timeout_ms);
W5500_Status_t W5500_SetNetConfig(const W5500_NetConfig_t *config);
W5500_Status_t W5500_GetNetConfig(W5500_NetConfig_t *config);
W5500_Status_t W5500_DetectGateway(void);
W5500_Status_t W5500_GetLinkStatus(uint8_t *linked);
W5500_Status_t W5500_ReadVersion(uint8_t *version);
W5500_Status_t W5500_EnableChipInterrupt(void);
W5500_Status_t W5500_ConfigureIntPin(void);

W5500_Status_t W5500_SocketInit(W5500_Socket_t socket, const W5500_SocketConfig_t *config);
W5500_Status_t W5500_SocketConnect(W5500_Socket_t socket);
W5500_Status_t W5500_SocketListen(W5500_Socket_t socket);
W5500_Status_t W5500_SocketOpenUdp(W5500_Socket_t socket);
W5500_Status_t W5500_SocketSetPeer(W5500_Socket_t socket, const uint8_t ip[4], uint16_t port);
W5500_Status_t W5500_SocketClose(W5500_Socket_t socket);
W5500_Status_t W5500_SocketRead(W5500_Socket_t socket, uint8_t *buffer, uint16_t buffer_size, uint16_t *read_len);
W5500_Status_t W5500_SocketWrite(W5500_Socket_t socket, const uint8_t *data, uint16_t size);

W5500_Status_t W5500_InterruptProcess(void);
W5500_Status_t W5500_ProcessEvents(uint8_t exti_hint);
W5500_Status_t W5500_SyncSocketState(W5500_Socket_t socket);
uint8_t W5500_IsInterruptActive(void);
W5500_Status_t W5500_GetSocketStatus(W5500_Socket_t socket, W5500_SocketStatus_t *status);
W5500_Status_t W5500_ClearSocketEvents(W5500_Socket_t socket, uint8_t event_mask);

/* PHY 链路监控（拔插网线恢复，Net01~05 共用） */
#define W5500_PHY_MON_CHECK_MS        200U
#define W5500_PHY_MON_CHECK_DOWN_MS   50U
#define W5500_PHY_MON_RECOVER_MS      1000U
#define W5500_PHY_MON_WAIT_LOG_MS     5000U
#define W5500_PHY_MON_WAIT_LINK_MS    3000U

typedef enum {
    W5500_PHY_WATCH_NONE = 0,
    W5500_PHY_WATCH_TCP_CONN,       /* TCP 已连接态需 CONN 标志 */
    W5500_PHY_WATCH_SOCKET_INIT,    /* UDP/Listen 需 INIT 标志 */
} W5500_PhyWatchMode_t;

typedef void (*W5500_PhyEventHandler_t)(void *ctx);

typedef struct {
    const W5500_NetConfig_t *net_cfg;
    uint8_t phy_linked;
    uint8_t socket_watch;
    uint8_t socket_active;
    uint32_t last_check_tick;
    uint32_t last_recover_tick;
    uint32_t last_wait_log_tick;
    W5500_PhyEventHandler_t on_link_down;
    W5500_PhyEventHandler_t on_link_up;
    W5500_PhyEventHandler_t on_socket_lost;
    void *ctx;
} W5500_PhyMonitor_t;

void W5500_PhyMonitor_Init(W5500_PhyMonitor_t *mon,
                           const W5500_NetConfig_t *net_cfg,
                           uint8_t phy_linked,
                           W5500_PhyEventHandler_t on_link_down,
                           W5500_PhyEventHandler_t on_link_up,
                           W5500_PhyEventHandler_t on_socket_lost,
                           void *ctx);
void W5500_PhyMonitor_SetSocketWatch(W5500_PhyMonitor_t *mon,
                                     W5500_PhyWatchMode_t mode,
                                     uint8_t active);
void W5500_PhyMonitor_Process(W5500_PhyMonitor_t *mon);
void W5500_PhyMonitor_RecoverNetwork(const W5500_NetConfig_t *net_cfg);
uint8_t W5500_PhyMonitor_IsLinked(const W5500_PhyMonitor_t *mon);

#endif /* CONFIG_MODULE_W5500_ENABLED */
#endif /* CONFIG_MODULE_W5500_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* W5500_H */
