/**
 * @file main_example.c
 * @brief Net06 - W5500 Paho MQTT Client EXTI 示例（小精灵 F103ZE）
 * @example Examples/Net/Net06_W5500_Paho_Embedded_C/main_example.c
 * @details Paho Embedded C 连接 Broker（默认 192.168.101.101:1883），周期 Publish，Subscribe 收命令
 */

#include "stm32f10x.h"
#include "system_init.h"
#include "board_early_init.h"
#include "board.h"
#include "uart.h"
#include "debug.h"
#include "log.h"
#include "error_handler.h"
#include "error_code.h"
#include "delay.h"
#include "config.h"
#include "led.h"
#include "w5500.h"
#include "exti.h"
#include "paho_mqtt_client.h"
#if CONFIG_MODULE_OLED_ENABLED
#include "oled_ssd1306.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ==================== 网络 / MQTT 参数 ==================== */

#define NET_IP_ADDR             { 192, 168, 101, 201 }
#define NET_GATEWAY             { 192, 168, 101, 1 }
#define NET_SUBNET              { 255, 255, 255, 0 }
#define NET_BROKER_ADDR         { 192, 168, 101, 101 }
#define NET_BROKER_PORT         1883U
#define NET_LOCAL_PORT          50000U

#define NET_PUB_INTERVAL_MS     5000U
#define NET_GATEWAY_DETECT_EN     0U
#define NET_MQTT_CLIENT_ID        "stm32_net06"
/* MQTTX 点「订阅」：看板子每 5s 发的 tick（板子 Publish 到此主题） */
#define NET_MQTT_TOPIC_MQTTX_SUB  "Net06/stm32_out"
/* MQTTX 点「发布」：给板子发命令（板子 Subscribe 此主题） */
#define NET_MQTT_TOPIC_MQTTX_PUB  "Net06/stm32_in"
#define NET_MQTT_SUB_TOPIC        NET_MQTT_TOPIC_MQTTX_PUB
#define NET_MQTT_PUB_TOPIC        NET_MQTT_TOPIC_MQTTX_SUB
#define NET_MQTT_KEEP_ALIVE_SEC   60U
#define NET_MQTT_QOS              1U
#define NET_MQTT_CLEAN_SESSION    0U   /* 0=持久会话（与 Net05 一致） */
#define NET_MQTT_IDLE_MS          1U
/* 与 PAHO_MQTT_CLIENT_PAYLOAD_MAX 一致，MQTTX 单条不要超过此长度 */
#define NET_MQTT_PAYLOAD_MAX        PAHO_MQTT_CLIENT_PAYLOAD_MAX

#define STM32_UID_BASE          0x1FFFF7E8UL

/* ==================== 应用上下文 ==================== */

typedef struct
{
    W5500_NetConfig_t net;
    uint8_t broker_ip[4];
    uint16_t broker_port;
    uint8_t phy_linked;
    uint8_t gw_ok;
    uint8_t oled_ready;
    uint8_t mqtt_ready;
} Net_AppCtx_t;

static Net_AppCtx_t g_app;
static uint32_t g_last_pub_tick;
static volatile uint8_t g_w5500_irq_pending;
static W5500_PhyMonitor_t g_phy_mon;

/* ==================== 基础工具 ==================== */

static void Net_GenerateMacFromUid(uint8_t mac[6])
{
    const uint8_t *uid = (const uint8_t *)STM32_UID_BASE;

    if (mac == NULL)
    {
        return;
    }

    mac[0] = 0x02U;
    mac[1] = (uint8_t)(uid[0] ^ uid[4]);
    mac[2] = (uint8_t)(uid[1] ^ uid[5]);
    mac[3] = uid[2];
    mac[4] = uid[3];
    mac[5] = (uint8_t)(uid[4] ^ uid[7]);
}

static void Net_FatalBlink(void)
{
    while (1)
    {
        LED1_On();
        Delay_ms(100);
        LED1_Off();
        Delay_ms(100);
    }
}

static W5500_Status_t Net_BuildNetConfig(W5500_NetConfig_t *cfg)
{
    static const uint8_t ip[] = NET_IP_ADDR;
    static const uint8_t gw[] = NET_GATEWAY;
    static const uint8_t mask[] = NET_SUBNET;
    static const uint8_t broker[] = NET_BROKER_ADDR;

    if (cfg == NULL)
    {
        return W5500_ERROR_INVALID_PARAM;
    }

    memcpy(cfg->ip, ip, 4U);
    memcpy(cfg->gateway, gw, 4U);
    memcpy(cfg->subnet, mask, 4U);
    memcpy(g_app.broker_ip, broker, 4U);
    g_app.broker_port = NET_BROKER_PORT;
    Net_GenerateMacFromUid(cfg->mac);
    return W5500_OK;
}

/* ==================== 调试与显示 ==================== */

static void Net_InitDebug(void)
{
    UART_Status_t uart_st;
    int dbg_st;
    log_config_t log_cfg;

    uart_st = UART_Init(UART_INSTANCE_1);
    if (uart_st != UART_OK)
    {
        Net_FatalBlink();
    }

    dbg_st = Debug_Init(DEBUG_MODE_UART, DEBUG_UART_BAUDRATE);
    if (dbg_st != 0)
    {
        Net_FatalBlink();
    }

    memset(&log_cfg, 0, sizeof(log_cfg));
    log_cfg.level = LOG_LEVEL_INFO;
    log_cfg.enable_timestamp = 0;
    log_cfg.enable_module = 1;
    log_cfg.enable_color = 0;

    if (Log_Init(&log_cfg) != LOG_OK)
    {
        Net_FatalBlink();
    }
}

static void Net_OledShowLinePadded(uint8_t line, const char *text)
{
#if CONFIG_MODULE_OLED_ENABLED
    char buf[17];
    uint8_t i;
    uint8_t len;

    if (!g_app.oled_ready || text == NULL)
    {
        return;
    }

    len = 0U;
    while ((text[len] != '\0') && (len < 16U))
    {
        len++;
    }
    for (i = 0U; i < len; i++)
    {
        buf[i] = text[i];
    }
    for (; i < 16U; i++)
    {
        buf[i] = ' ';
    }
    buf[16] = '\0';
    (void)OLED_ShowString(line, 1, buf);
#else
    (void)line;
    (void)text;
#endif
}

static void Net_InitOled(void)
{
#if CONFIG_MODULE_OLED_ENABLED
    g_app.oled_ready = 0U;
    if (OLED_Init() != OLED_OK)
    {
        LOG_WARN("MAIN", "OLED init fail");
        return;
    }
    g_app.oled_ready = 1U;
    Net_OledShowLinePadded(1, "Net06 Paho");
    Net_OledShowLinePadded(2, "Init...");
    Net_OledShowLinePadded(3, "B:----:----");
    Net_OledShowLinePadded(4, "Boot...");
#endif
}

static void Net_UpdateOled(void)
{
#if CONFIG_MODULE_OLED_ENABLED
    char line[17];

    if (!g_app.oled_ready)
    {
        return;
    }

    Net_OledShowLinePadded(1, "Net06 Paho");
    (void)snprintf(line, sizeof(line), "%u.%u.%u.%u",
                   g_app.net.ip[0], g_app.net.ip[1],
                   g_app.net.ip[2], g_app.net.ip[3]);
    Net_OledShowLinePadded(2, line);
    (void)snprintf(line, sizeof(line), "B:%u.%u.%u.%u",
                   g_app.broker_ip[0], g_app.broker_ip[1],
                   g_app.broker_ip[2], g_app.broker_ip[3]);
    Net_OledShowLinePadded(3, line);
    Net_OledShowLinePadded(4, g_app.mqtt_ready ? "MQTT ON" : "MQTT OFF");
#endif
}

static void Net_OnMqttMessage(const char *topic,
                              uint16_t topic_len,
                              const uint8_t *payload,
                              uint16_t payload_len,
                              void *user_data)
{
    static char topic_buf[PAHO_MQTT_CLIENT_TOPIC_MAX];
    static char payload_buf[PAHO_MQTT_CLIENT_PAYLOAD_MAX + 1U];
    uint16_t topic_n;
    uint16_t payload_n;

    (void)user_data;

    if ((topic == NULL) || (payload == NULL) || (payload_len == 0U))
    {
        return;
    }

    topic_n = (topic_len < (sizeof(topic_buf) - 1U)) ?
              topic_len : (uint16_t)(sizeof(topic_buf) - 1U);
    memcpy(topic_buf, topic, topic_n);
    topic_buf[topic_n] = '\0';

    payload_n = (payload_len < (sizeof(payload_buf) - 1U)) ?
                payload_len : (uint16_t)(sizeof(payload_buf) - 1U);
    memcpy(payload_buf, payload, payload_n);
    payload_buf[payload_n] = '\0';

    LOG_INFO("MQTT", "RX topic=%s payload=%s", topic_buf, payload_buf);
}

/* ==================== W5500 EXTI ==================== */

static void Net_W5500ExtiCallback(EXTI_Line_t line, void *user_data)
{
    (void)line;
    (void)user_data;
    g_w5500_irq_pending = 1U;
}

static error_code_t Net_InitW5500Exti(void)
{
    EXTI_Status_t st;

    g_w5500_irq_pending = 0U;

    st = EXTI_HW_Init(W5500_EXTI_LINE, EXTI_TRIGGER_RISING_FALLING, EXTI_MODE_INTERRUPT);
    if (st != EXTI_OK)
    {
        LOG_ERROR("NET", "EXTI init fail: %d", (int)st);
        return (error_code_t)st;
    }

    st = EXTI_SetCallback(W5500_EXTI_LINE, Net_W5500ExtiCallback, NULL);
    if (st != EXTI_OK)
    {
        return (error_code_t)st;
    }

    st = EXTI_Enable(W5500_EXTI_LINE);
    if (st != EXTI_OK)
    {
        return (error_code_t)st;
    }

    LOG_INFO("NET", "W5500 EXTI PF9 edge OK");
    return ERROR_OK;
}

static void Net_ServiceW5500(void)
{
    W5500_Status_t st;
    uint8_t hint;

    hint = g_w5500_irq_pending;
    g_w5500_irq_pending = 0U;

    st = W5500_ProcessEvents(hint);
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "ProcessEvents: %d", (int)st);
    }
}

static void Net_OnPhyLinkDown(void *ctx)
{
    (void)ctx;
    (void)PAHO_MQTT_Client_ForceReconnect();
}

static void Net_OnPhyLinkUp(void *ctx)
{
    (void)ctx;
    (void)PAHO_MQTT_Client_ForceReconnect();
}

static void Net_OnSocketLost(void *ctx)
{
    (void)ctx;
    (void)PAHO_MQTT_Client_ForceReconnect();
}

static W5500_Status_t Net_InitW5500(void)
{
    W5500_Status_t st;
    uint8_t version = 0U;
    uint8_t oled_ready;

    oled_ready = g_app.oled_ready;
    memset(&g_app, 0, sizeof(g_app));
    g_app.oled_ready = oled_ready;

    st = Net_BuildNetConfig(&g_app.net);
    if (st != W5500_OK)
    {
        return st;
    }

    LOG_INFO("NET", "MAC %02X:%02X:%02X:%02X:%02X:%02X",
             g_app.net.mac[0], g_app.net.mac[1], g_app.net.mac[2],
             g_app.net.mac[3], g_app.net.mac[4], g_app.net.mac[5]);
    LOG_INFO("NET", "IP  %u.%u.%u.%u",
             g_app.net.ip[0], g_app.net.ip[1], g_app.net.ip[2], g_app.net.ip[3]);
    LOG_INFO("NET", "Broker %u.%u.%u.%u:%u",
             g_app.broker_ip[0], g_app.broker_ip[1],
             g_app.broker_ip[2], g_app.broker_ip[3],
             (unsigned int)g_app.broker_port);

    st = W5500_Init();
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_SetNetConfig(&g_app.net);
    if (st != W5500_OK)
    {
        return st;
    }

    st = W5500_HardwareReset();
    if (st != W5500_OK)
    {
        LOG_WARN("NET", "wait link: %d (check cable)", (int)st);
    }

    st = W5500_ReadVersion(&version);
    if (st == W5500_OK)
    {
        LOG_INFO("NET", "W5500 version 0x%02X", version);
    }

    st = W5500_GetLinkStatus(&g_app.phy_linked);
    if (st == W5500_OK)
    {
        LOG_INFO("NET", "PHY link: %s", g_app.phy_linked ? "UP" : "DOWN");
    }

#if NET_GATEWAY_DETECT_EN
    st = W5500_DetectGateway();
    g_app.gw_ok = (st == W5500_OK) ? 1U : 0U;
#else
    g_app.gw_ok = 0U;
#endif

    st = W5500_EnableChipInterrupt();
    if (st != W5500_OK)
    {
        return st;
    }

    (void)W5500_ProcessEvents(1U);
    if (Net_InitW5500Exti() != ERROR_OK)
    {
        return W5500_ERROR_INIT_FAILED;
    }
    (void)W5500_ConfigureIntPin();
    (void)EXTI_ClearPending(W5500_EXTI_LINE);
    if (W5500_IsInterruptActive() != 0U)
    {
        g_w5500_irq_pending = 1U;
    }

    Net_UpdateOled();

    W5500_PhyMonitor_Init(&g_phy_mon, &g_app.net, g_app.phy_linked,
                          Net_OnPhyLinkDown, Net_OnPhyLinkUp, Net_OnSocketLost, NULL);
    return W5500_OK;
}

static PAHO_MQTT_Client_Status_t Net_InitMqtt(void)
{
    PAHO_MQTT_Client_Config_t mqtt_cfg;
    PAHO_MQTT_Client_Status_t st;

    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    memcpy(mqtt_cfg.broker_ip, g_app.broker_ip, 4U);
    mqtt_cfg.broker_port = g_app.broker_port;
    mqtt_cfg.local_port = NET_LOCAL_PORT;
    mqtt_cfg.keep_alive_sec = NET_MQTT_KEEP_ALIVE_SEC;
    mqtt_cfg.qos = NET_MQTT_QOS;
    mqtt_cfg.clean_session = NET_MQTT_CLEAN_SESSION;
    (void)strncpy(mqtt_cfg.client_id, NET_MQTT_CLIENT_ID, sizeof(mqtt_cfg.client_id) - 1U);
    (void)strncpy(mqtt_cfg.sub_topic, NET_MQTT_SUB_TOPIC, sizeof(mqtt_cfg.sub_topic) - 1U);
    (void)strncpy(mqtt_cfg.pub_topic, NET_MQTT_PUB_TOPIC, sizeof(mqtt_cfg.pub_topic) - 1U);

    st = PAHO_MQTT_Client_Init(&mqtt_cfg, Net_OnMqttMessage, NULL);
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        LOG_ERROR("MQTT", "init fail: %d", (int)st);
        return st;
    }

    LOG_INFO("MQTT", "MQTTX SUBSCRIBE -> %s (see tick)", NET_MQTT_PUB_TOPIC);
    LOG_INFO("MQTT", "MQTTX PUBLISH   -> %s (send cmd)", NET_MQTT_SUB_TOPIC);
    return PAHO_MQTT_CLIENT_OK;
}

static void Net_ProcessPeriodicPublish(void)
{
    PAHO_MQTT_Client_Status_t st;
    char payload[48];
    uint32_t now;
    int len;

    if (PAHO_MQTT_Client_CanPublish() == 0U)
    {
        return;
    }

    now = Delay_GetTick();
    if (Delay_GetElapsed(now, g_last_pub_tick) < NET_PUB_INTERVAL_MS)
    {
        return;
    }
    g_last_pub_tick = now;

    len = snprintf(payload, sizeof(payload), "tick=%lu", (unsigned long)now);
    if ((len <= 0) || (len >= (int)sizeof(payload)))
    {
        return;
    }

    st = PAHO_MQTT_Client_Publish(NET_MQTT_PUB_TOPIC, payload, (uint16_t)len, NET_MQTT_QOS);
    if (st != PAHO_MQTT_CLIENT_OK)
    {
        LOG_WARN("MQTT", "publish fail: %d %s", (int)st, PAHO_MQTT_Client_GetLastErrorStr());
        return;
    }

    LOG_INFO("MQTT", "TX %s", payload);
    (void)PAHO_MQTT_Client_Drain(PAHO_MQTT_CLIENT_DRAIN_PUBLISH);
}

static void Net_ProcessOnce(void)
{
    PAHO_MQTT_Client_Status_t mst;
    uint8_t mqtt_on;

    Net_ServiceW5500();
    W5500_PhyMonitor_Process(&g_phy_mon);

    if (W5500_PhyMonitor_IsLinked(&g_phy_mon) == 0U)
    {
        if (g_app.mqtt_ready != 0U)
        {
            g_app.mqtt_ready = 0U;
            Net_UpdateOled();
        }
        return;
    }

    mst = PAHO_MQTT_Client_Drain(PAHO_MQTT_CLIENT_DRAIN_DEFAULT);
    if ((mst != PAHO_MQTT_CLIENT_OK) &&
        (PAHO_MQTT_Client_IsSessionReady() == 0U))
    {
        LOG_DEBUG("MQTT", "sync: %s", PAHO_MQTT_Client_GetLastErrorStr());
    }

    mqtt_on = PAHO_MQTT_Client_IsSessionReady();
    if (mqtt_on != g_app.mqtt_ready)
    {
        g_app.mqtt_ready = mqtt_on;
        Net_UpdateOled();
        if (mqtt_on != 0U)
        {
            g_last_pub_tick = Delay_GetTick();
        }
    }

    (void)PAHO_MQTT_Client_FlushPending();
    Net_ProcessPeriodicPublish();

    W5500_PhyMonitor_SetSocketWatch(&g_phy_mon, W5500_PHY_WATCH_TCP_CONN,
                                    (uint8_t)((PAHO_MQTT_Client_CanPublish() != 0U) ||
                                              (PAHO_MQTT_Client_IsSessionReady() != 0U)));
}

/* ==================== 主函数 ==================== */

int main(void)
{
    W5500_Status_t st;

    if (Board_EarlyInit() != ERROR_OK)
    {
        Net_FatalBlink();
    }

    System_Init();
    Net_InitDebug();
    Net_InitOled();

    LOG_INFO("MAIN", "=== Net06 W5500 Paho MQTT Client EXTI ===");

    st = Net_InitW5500();
    if (st != W5500_OK)
    {
        LOG_ERROR("MAIN", "W5500 init fail: %d", (int)st);
        ErrorHandler_Handle((error_code_t)st, "MAIN");
        Net_FatalBlink();
    }

    if (Net_InitMqtt() != PAHO_MQTT_CLIENT_OK)
    {
        Net_FatalBlink();
    }

    LED1_On();
    g_last_pub_tick = Delay_GetTick();

    while (1)
    {
        Net_ProcessOnce();
        if ((g_w5500_irq_pending == 0U) && (g_app.mqtt_ready != 0U))
        {
            Delay_ms(NET_MQTT_IDLE_MS);
        }
    }
}

