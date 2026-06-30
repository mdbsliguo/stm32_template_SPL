/**
 * @file mqtt_client.h
 * @brief MQTT 客户端中间件（MQTT-C + W5500 TCP）
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifdef CONFIG_MODULE_MQTT_CLIENT_ENABLED
#if CONFIG_MODULE_MQTT_CLIENT_ENABLED

#include "error_code.h"
#include <stdint.h>

#define MQTT_CLIENT_DRAIN_DEFAULT   8U
#define MQTT_CLIENT_DRAIN_PUBLISH   4U
#define MQTT_CLIENT_SEND_BUF_SIZE   1024U
#define MQTT_CLIENT_RECV_BUF_SIZE   1024U
#define MQTT_CLIENT_TOPIC_MAX       64U
#define MQTT_CLIENT_CLIENT_ID_MAX   32U

typedef enum {
    MQTT_CLIENT_OK = ERROR_OK,
    MQTT_CLIENT_ERROR_NOT_IMPLEMENTED = ERROR_BASE_MQTT_CLIENT - 99,
    MQTT_CLIENT_ERROR_NULL_PTR = ERROR_BASE_MQTT_CLIENT - 1,
    MQTT_CLIENT_ERROR_INVALID_PARAM = ERROR_BASE_MQTT_CLIENT - 2,
    MQTT_CLIENT_ERROR_NOT_INITIALIZED = ERROR_BASE_MQTT_CLIENT - 3,
    MQTT_CLIENT_ERROR_NETWORK = ERROR_BASE_MQTT_CLIENT - 4,
    MQTT_CLIENT_ERROR_PROTOCOL = ERROR_BASE_MQTT_CLIENT - 5,
    MQTT_CLIENT_ERROR_BUSY = ERROR_BASE_MQTT_CLIENT - 6,
} MQTT_Client_Status_t;

typedef struct {
    uint8_t broker_ip[4];
    uint16_t broker_port;
    uint16_t local_port;
    char client_id[MQTT_CLIENT_CLIENT_ID_MAX];
    char sub_topic[MQTT_CLIENT_TOPIC_MAX];
    char pub_topic[MQTT_CLIENT_TOPIC_MAX];
    uint16_t keep_alive_sec;
    uint8_t qos;    /* 订阅与建议发布 QoS（0 或 1） */
} MQTT_Client_Config_t;

typedef void (*MQTT_Client_MessageCallback_t)(const char *topic,
                                              uint16_t topic_len,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              void *user_data);

MQTT_Client_Status_t MQTT_Client_Init(const MQTT_Client_Config_t *config,
                                      MQTT_Client_MessageCallback_t message_cb,
                                      void *user_data);
MQTT_Client_Status_t MQTT_Client_Process(void);
MQTT_Client_Status_t MQTT_Client_Drain(uint8_t max_rounds);
MQTT_Client_Status_t MQTT_Client_Publish(const char *topic,
                                         const void *payload,
                                         uint16_t payload_len,
                                         uint8_t qos);
uint8_t MQTT_Client_IsSessionReady(void);
uint8_t MQTT_Client_CanPublish(void);
const char *MQTT_Client_GetLastErrorStr(void);
MQTT_Client_Status_t MQTT_Client_ForceReconnect(void);

#endif /* CONFIG_MODULE_MQTT_CLIENT_ENABLED */
#endif /* CONFIG_MODULE_MQTT_CLIENT_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
