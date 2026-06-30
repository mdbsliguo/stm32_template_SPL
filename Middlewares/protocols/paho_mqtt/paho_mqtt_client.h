/**
 * @file paho_mqtt_client.h
 * @brief Paho MQTT Embedded C ¿Í»§¶Ë·â×°£¨W5500 TCP£©
 */

#ifndef PAHO_MQTT_CLIENT_H
#define PAHO_MQTT_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#if CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED

#include "error_code.h"
#include <stdint.h>

#define PAHO_MQTT_CLIENT_DRAIN_DEFAULT   8U
#define PAHO_MQTT_CLIENT_EXTRA_HANDLERS_MAX  4U
#define PAHO_MQTT_DISPATCH_PER_FLUSH           4U
#define PAHO_MQTT_CLIENT_DRAIN_PUBLISH   4U
#define PAHO_MQTT_CLIENT_OFFLINE_DRAIN_MS       3000U
#define PAHO_MQTT_CLIENT_OFFLINE_DRAIN_IDLE_MS  150U
#define PAHO_MQTT_CLIENT_OFFLINE_DRAIN_YIELD_MS 30U
#define PAHO_MQTT_CLIENT_OFFLINE_RECOVER_MS     500U
#define PAHO_MQTT_PENDING_QUEUE_DEPTH           32U
#define PAHO_MQTT_CLIENT_SEND_BUF_SIZE   1024U
#define PAHO_MQTT_CLIENT_RECV_BUF_SIZE   1024U
#define PAHO_MQTT_CLIENT_TOPIC_MAX       64U
#define PAHO_MQTT_CLIENT_CLIENT_ID_MAX   32U
#define PAHO_MQTT_CLIENT_CMD_TIMEOUT_MS  5000U
#define PAHO_MQTT_CLIENT_YIELD_MS          50U
/* ?? MQTT ?????????F103 ??????????????? */
#define PAHO_MQTT_CLIENT_PAYLOAD_MAX       128U
/* 0 = Clean Session ????????? Net05 ??? */
#define PAHO_MQTT_CLIENT_DEFAULT_CLEAN_SESSION  0U

typedef enum {
    PAHO_MQTT_CLIENT_OK = ERROR_OK,
    PAHO_MQTT_CLIENT_ERROR_NOT_IMPLEMENTED = ERROR_BASE_PAHO_MQTT_CLIENT - 99,
    PAHO_MQTT_CLIENT_ERROR_NULL_PTR = ERROR_BASE_PAHO_MQTT_CLIENT - 1,
    PAHO_MQTT_CLIENT_ERROR_INVALID_PARAM = ERROR_BASE_PAHO_MQTT_CLIENT - 2,
    PAHO_MQTT_CLIENT_ERROR_NOT_INITIALIZED = ERROR_BASE_PAHO_MQTT_CLIENT - 3,
    PAHO_MQTT_CLIENT_ERROR_NETWORK = ERROR_BASE_PAHO_MQTT_CLIENT - 4,
    PAHO_MQTT_CLIENT_ERROR_PROTOCOL = ERROR_BASE_PAHO_MQTT_CLIENT - 5,
    PAHO_MQTT_CLIENT_ERROR_BUSY = ERROR_BASE_PAHO_MQTT_CLIENT - 6,
} PAHO_MQTT_Client_Status_t;

typedef struct {
    uint8_t broker_ip[4];
    uint16_t broker_port;
    uint16_t local_port;
    char client_id[PAHO_MQTT_CLIENT_CLIENT_ID_MAX];
    char sub_topic[PAHO_MQTT_CLIENT_TOPIC_MAX];
    char pub_topic[PAHO_MQTT_CLIENT_TOPIC_MAX];
    uint16_t keep_alive_sec;
    uint8_t qos;           /* ????? QoS?0 ? 1? */
    uint8_t clean_session; /* 0=?????1=Clean Session */
} PAHO_MQTT_Client_Config_t;

typedef void (*PAHO_MQTT_Client_MessageCallback_t)(const char *topic,
                                                   uint16_t topic_len,
                                                   const uint8_t *payload,
                                                   uint16_t payload_len,
                                                   void *user_data);

PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Init(const PAHO_MQTT_Client_Config_t *config,
                                                 PAHO_MQTT_Client_MessageCallback_t message_cb,
                                                 void *user_data);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Process(void);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Drain(uint8_t max_rounds);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_Publish(const char *topic,
                                                   const void *payload,
                                                   uint16_t payload_len,
                                                   uint8_t qos);
uint8_t PAHO_MQTT_Client_IsSessionReady(void);
uint8_t PAHO_MQTT_Client_CanPublish(void);
const char *PAHO_MQTT_Client_GetLastErrorStr(void);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_ForceReconnect(void);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_FlushPending(void);
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_RegisterHandler(const char *topic);
uint8_t PAHO_MQTT_Client_HasPending(void);
/* ??????? Paho ??? Handler + MQTTSubscribeWithResults */
PAHO_MQTT_Client_Status_t PAHO_MQTT_Client_SubscribeTopic(const char *topic, uint8_t qos);
uint8_t PAHO_MQTT_Client_IsSessionRestored(void);

#endif /* CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED */

#ifdef __cplusplus
}
#endif

#endif /* PAHO_MQTT_CLIENT_H */
