/**
 * @file mqtt_client.c
 * @brief MQTT 客户端中间件实现
 */

#include "config.h"

#if CONFIG_MODULE_MQTT_CLIENT_ENABLED

#include "mqtt_client.h"
#include "mqtt.h"
#include "w5500.h"
#include "delay.h"
#include "log.h"
#include <string.h>
#include <stdio.h>

#define MQTT_CLIENT_TCP_WAIT_MS     5000U
/* 0 = Clean Session 关闭（持久会话，Broker 记住订阅） */
#define MQTT_CLIENT_CONNECT_FLAGS   0U

typedef struct {
    MQTT_Client_Config_t cfg;
    struct mqtt_client client;
    uint8_t sendbuf[MQTT_CLIENT_SEND_BUF_SIZE];
    uint8_t recvbuf[MQTT_CLIENT_RECV_BUF_SIZE];
    MQTT_Client_MessageCallback_t message_cb;
    void *user_data;
    uint8_t initialized;
    uint8_t session_ready;
    uint8_t publish_ready;
    uint8_t pending_subscribe;
    uint8_t subscribe_issued;
} MQTT_Client_Context_t;

static MQTT_Client_Context_t g_mqtt;

static MQTT_Client_Status_t mqtt_client_map_error(enum MQTTErrors err)
{
    if (err == MQTT_OK)
    {
        return MQTT_CLIENT_OK;
    }
    return MQTT_CLIENT_ERROR_PROTOCOL;
}

static void mqtt_client_clear_session_flags(void)
{
    g_mqtt.session_ready = 0U;
    g_mqtt.publish_ready = 0U;
    g_mqtt.subscribe_issued = 0U;
}

static void mqtt_client_mark_subscribe_pending(void)
{
    if (g_mqtt.cfg.sub_topic[0] != '\0')
    {
        g_mqtt.pending_subscribe = 1U;
    }
    else
    {
        g_mqtt.pending_subscribe = 0U;
    }
}

static W5500_Status_t mqtt_client_wait_tcp_connected(uint32_t timeout_ms)
{
    uint32_t start;
    W5500_SocketStatus_t status;

    start = Delay_GetTick();
    while (Delay_GetElapsed(Delay_GetTick(), start) < timeout_ms)
    {
        (void)W5500_SyncSocketState(W5500_SOCKET_0);
        if (W5500_GetSocketStatus(W5500_SOCKET_0, &status) == W5500_OK)
        {
            if ((status.flags & (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN)) ==
                (W5500_SOCK_FLAG_INIT | W5500_SOCK_FLAG_CONN))
            {
                return W5500_OK;
            }
        }
        Delay_ms(10U);
    }
    return W5500_ERROR_TIMEOUT;
}

static W5500_Status_t mqtt_client_open_tcp(void)
{
    W5500_SocketConfig_t sock_cfg;
    W5500_Status_t st;

    (void)W5500_SocketClose(W5500_SOCKET_0);

    memset(&sock_cfg, 0, sizeof(sock_cfg));
    sock_cfg.local_port = g_mqtt.cfg.local_port;
    memcpy(sock_cfg.dest_ip, g_mqtt.cfg.broker_ip, 4U);
    sock_cfg.dest_port = g_mqtt.cfg.broker_port;

    st = W5500_SocketInit(W5500_SOCKET_0, &sock_cfg);
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_SocketConnect(W5500_SOCKET_0);
    if (st != W5500_OK)
    {
        return st;
    }

    return mqtt_client_wait_tcp_connected(MQTT_CLIENT_TCP_WAIT_MS);
}

static void mqtt_client_on_publish(void **state, struct mqtt_response_publish *publish)
{
    (void)state;

    if (publish == NULL)
    {
        return;
    }

    LOG_INFO("MQTT", "broker PUBLISH qos=%u len=%u",
             (unsigned int)publish->qos_level,
             (unsigned int)publish->application_message_size);

    if (g_mqtt.message_cb == NULL)
    {
        return;
    }

    g_mqtt.message_cb((const char *)publish->topic_name,
                      (uint16_t)publish->topic_name_size,
                      (const uint8_t *)publish->application_message,
                      (uint16_t)publish->application_message_size,
                      g_mqtt.user_data);
}

static void mqtt_client_reconnect_cb(struct mqtt_client *client, void **state)
{
    W5500_Status_t st;
    enum MQTTErrors merr;

    (void)state;

    mqtt_client_clear_session_flags();

    st = mqtt_client_open_tcp();
    if (st != W5500_OK)
    {
        LOG_WARN("MQTT", "TCP connect broker fail: %d", (int)st);
        client->error = MQTT_ERROR_SOCKET_ERROR;
        return;
    }

    LOG_INFO("MQTT", "TCP connected %u.%u.%u.%u:%u",
             g_mqtt.cfg.broker_ip[0], g_mqtt.cfg.broker_ip[1],
             g_mqtt.cfg.broker_ip[2], g_mqtt.cfg.broker_ip[3],
             (unsigned int)g_mqtt.cfg.broker_port);

    mqtt_reinit(client, 0,
                g_mqtt.sendbuf, sizeof(g_mqtt.sendbuf),
                g_mqtt.recvbuf, sizeof(g_mqtt.recvbuf));

    merr = mqtt_connect(client,
                        g_mqtt.cfg.client_id,
                        NULL, NULL, 0U,
                        NULL, NULL,
                        MQTT_CLIENT_CONNECT_FLAGS,
                        g_mqtt.cfg.keep_alive_sec);
    if (merr != MQTT_OK)
    {
        LOG_WARN("MQTT", "mqtt_connect fail: %s", mqtt_error_str(merr));
        client->error = merr;
        return;
    }

    mqtt_client_mark_subscribe_pending();
    if (g_mqtt.pending_subscribe != 0U)
    {
        LOG_INFO("MQTT", "CONNECT queued, will subscribe %s", g_mqtt.cfg.sub_topic);
    }

    LOG_INFO("MQTT", "MQTT session start id=%s", g_mqtt.cfg.client_id);
}

static uint8_t mqtt_client_is_connack_done(void)
{
    return (uint8_t)(mqtt_mq_find(&g_mqtt.client.mq, MQTT_CONTROL_CONNECT, NULL) == NULL);
}

static uint8_t mqtt_client_is_suback_done(void)
{
    return (uint8_t)(mqtt_mq_find(&g_mqtt.client.mq, MQTT_CONTROL_SUBSCRIBE, NULL) == NULL);
}

static void mqtt_client_update_session_flags(void)
{
    uint8_t connack_done;

    if (g_mqtt.client.error != MQTT_OK)
    {
        mqtt_client_clear_session_flags();
        return;
    }

    connack_done = mqtt_client_is_connack_done();
    g_mqtt.publish_ready = connack_done;

    if (g_mqtt.cfg.sub_topic[0] == '\0')
    {
        g_mqtt.session_ready = connack_done;
        return;
    }

    g_mqtt.session_ready = (uint8_t)((g_mqtt.subscribe_issued != 0U) &&
                                     (mqtt_client_is_suback_done() != 0U));
}

static void mqtt_client_try_subscribe(void)
{
    enum MQTTErrors merr;

    if ((g_mqtt.pending_subscribe == 0U) ||
        (g_mqtt.cfg.sub_topic[0] == '\0') ||
        (mqtt_client_is_connack_done() == 0U))
    {
        return;
    }

    merr = mqtt_subscribe(&g_mqtt.client, g_mqtt.cfg.sub_topic, (int)g_mqtt.cfg.qos);
    if (merr != MQTT_OK)
    {
        LOG_WARN("MQTT", "subscribe fail: %s", mqtt_error_str(merr));
        g_mqtt.client.error = merr;
        return;
    }

    g_mqtt.pending_subscribe = 0U;
    g_mqtt.subscribe_issued = 1U;
    LOG_INFO("MQTT", "subscribe queued %s (wait SUBACK)", g_mqtt.cfg.sub_topic);
}

static void mqtt_client_post_sync(void)
{
    mqtt_client_update_session_flags();
    mqtt_client_try_subscribe();
    mqtt_client_update_session_flags();
}

static void mqtt_client_log_session_transitions(uint8_t was_publish_ready,
                                                uint8_t was_session_ready)
{
    if ((was_publish_ready == 0U) && (g_mqtt.publish_ready != 0U))
    {
        if (g_mqtt.cfg.pub_topic[0] != '\0')
        {
            LOG_INFO("MQTT", "CONNACK ok, publish %s", g_mqtt.cfg.pub_topic);
        }
        else
        {
            LOG_INFO("MQTT", "CONNACK ok");
        }
    }

    if ((was_session_ready == 0U) && (g_mqtt.session_ready != 0U))
    {
        if (g_mqtt.cfg.sub_topic[0] != '\0')
        {
            LOG_INFO("MQTT", "SUBACK ok, listen %s", g_mqtt.cfg.sub_topic);
        }
        else
        {
            LOG_INFO("MQTT", "CONNACK ok, session up");
        }
    }
}

static void mqtt_client_on_sync_error(void)
{
    if (g_mqtt.client.error != MQTT_OK)
    {
        mqtt_client_clear_session_flags();
        mqtt_client_mark_subscribe_pending();
    }
}

MQTT_Client_Status_t MQTT_Client_Init(const MQTT_Client_Config_t *config,
                                      MQTT_Client_MessageCallback_t message_cb,
                                      void *user_data)
{
    if (config == NULL)
    {
        return MQTT_CLIENT_ERROR_NULL_PTR;
    }
    if ((config->client_id[0] == '\0') || (config->broker_port == 0U))
    {
        return MQTT_CLIENT_ERROR_INVALID_PARAM;
    }
    if (config->qos > 1U)
    {
        return MQTT_CLIENT_ERROR_INVALID_PARAM;
    }

    memset(&g_mqtt, 0, sizeof(g_mqtt));
    memcpy(&g_mqtt.cfg, config, sizeof(MQTT_Client_Config_t));
    g_mqtt.message_cb = message_cb;
    g_mqtt.user_data = user_data;

    mqtt_init_reconnect(&g_mqtt.client,
                        mqtt_client_reconnect_cb,
                        NULL,
                        mqtt_client_on_publish);

    g_mqtt.initialized = 1U;
    return MQTT_CLIENT_OK;
}

MQTT_Client_Status_t MQTT_Client_Process(void)
{
    enum MQTTErrors merr;
    uint8_t was_publish_ready;
    uint8_t was_session_ready;

    if (g_mqtt.initialized == 0U)
    {
        return MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }

    was_publish_ready = g_mqtt.publish_ready;
    was_session_ready = g_mqtt.session_ready;

    merr = mqtt_sync(&g_mqtt.client);
    if (merr != MQTT_OK)
    {
        mqtt_client_on_sync_error();
        return mqtt_client_map_error(merr);
    }

    mqtt_client_post_sync();
    mqtt_client_log_session_transitions(was_publish_ready, was_session_ready);

    return MQTT_CLIENT_OK;
}

MQTT_Client_Status_t MQTT_Client_Drain(uint8_t max_rounds)
{
    MQTT_Client_Status_t st;
    uint8_t i;

    if (max_rounds == 0U)
    {
        return MQTT_CLIENT_OK;
    }

    st = MQTT_CLIENT_OK;
    for (i = 0U; i < max_rounds; i++)
    {
        st = MQTT_Client_Process();
        if (st != MQTT_CLIENT_OK)
        {
            break;
        }
    }
    return st;
}

MQTT_Client_Status_t MQTT_Client_Publish(const char *topic,
                                         const void *payload,
                                         uint16_t payload_len,
                                         uint8_t qos)
{
    enum MQTTErrors merr;
    uint8_t pub_flags;

    if (g_mqtt.initialized == 0U)
    {
        return MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }
    if ((topic == NULL) || ((payload == NULL) && (payload_len > 0U)))
    {
        return MQTT_CLIENT_ERROR_NULL_PTR;
    }
    if (g_mqtt.publish_ready == 0U)
    {
        return MQTT_CLIENT_ERROR_BUSY;
    }

    pub_flags = (qos == 0U) ? MQTT_PUBLISH_QOS_0 : MQTT_PUBLISH_QOS_1;

    merr = mqtt_publish(&g_mqtt.client,
                        topic,
                        payload,
                        payload_len,
                        pub_flags);
    if (merr != MQTT_OK)
    {
        return mqtt_client_map_error(merr);
    }
    return MQTT_CLIENT_OK;
}

uint8_t MQTT_Client_IsSessionReady(void)
{
    return (uint8_t)((g_mqtt.initialized != 0U) &&
                     (g_mqtt.session_ready != 0U) &&
                     (g_mqtt.client.error == MQTT_OK));
}

uint8_t MQTT_Client_CanPublish(void)
{
    return (uint8_t)((g_mqtt.initialized != 0U) &&
                     (g_mqtt.publish_ready != 0U) &&
                     (g_mqtt.client.error == MQTT_OK));
}

const char *MQTT_Client_GetLastErrorStr(void)
{
    return mqtt_error_str(g_mqtt.client.error);
}

MQTT_Client_Status_t MQTT_Client_ForceReconnect(void)
{
    if (g_mqtt.initialized == 0U)
    {
        return MQTT_CLIENT_ERROR_NOT_INITIALIZED;
    }

    (void)W5500_SocketClose(W5500_SOCKET_0);
    mqtt_client_clear_session_flags();
    mqtt_client_mark_subscribe_pending();
    g_mqtt.client.error = MQTT_ERROR_CONNECTION_CLOSED;
    return MQTT_CLIENT_OK;
}

#endif /* CONFIG_MODULE_MQTT_CLIENT_ENABLED */
