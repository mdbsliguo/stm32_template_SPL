# Paho MQTT Embedded C 中间件

基于 [Eclipse Paho Embedded C](https://github.com/eclipse-paho/paho.mqtt.embedded-c)（`MQTTClient-C` + `MQTTPacket`），TCP 传输适配 W5500。

演示案例：`Examples/Net/Net06_W5500_Paho_Embedded_C/`

---

## 目录结构

```text
Middlewares/protocols/paho_mqtt/
  paho_mqtt_client.h/c    # 工程封装 API（对齐 Net05 mqtt_client 用法）
  paho_platform.h/c       # Timer + Network（W5500 Socket0）
  third_party/paho/       # 上游 Paho 源码（EPL/EDL）
```

---

## 模块开关

```c
#define CONFIG_MODULE_PAHO_MQTT_CLIENT_ENABLED  1
#define CONFIG_MODULE_W5500_ENABLED             1
#define CONFIG_MODULE_SPI_ENABLED               1
```

---

## 核心 API

| 函数 | 说明 |
|------|------|
| `PAHO_MQTT_Client_Init()` | 配置 Broker、Client ID、订阅/发布主题 |
| `PAHO_MQTT_Client_Process()` | 建 TCP → CONNECT → SUBSCRIBE → offline drain 分步 / `MQTTYield` |
| `PAHO_MQTT_Client_Drain(n)` | 多轮 `Process()`，末尾 `FlushPending()` |
| `PAHO_MQTT_Client_Publish()` | 发布（载荷 ≤ `PAYLOAD_MAX`） |
| `PAHO_MQTT_Client_ForceReconnect()` | 关 Socket + 置重连；拔网线时调用 |
| `PAHO_MQTT_Client_FlushPending()` | 主循环投递 pending 队列（浅栈回调） |
| `PAHO_MQTT_Client_IsSessionReady()` | 已订阅且链路正常 |
| `PAHO_MQTT_Client_CanPublish()` | 可发布（offline drain 期间为 0） |

可选扩展 API（案例未使用）：

| 函数 | 说明 |
|------|------|
| `PAHO_MQTT_Client_RegisterHandler()` | CONNECT 前注册额外主题 Handler |
| `PAHO_MQTT_Client_SubscribeTopic()` | 运行时订阅 + SUBACK |
| `PAHO_MQTT_Client_IsSessionRestored()` | CONNACK `SessionPresent` |

---

## 收消息与离线 drain

1. `MQTTYield` 收到 PUBLISH → `paho_mqtt_message_arrived` **只入 pending 队列**（立即 `memcpy` 载荷）
2. 主循环调用 `FlushPending()` → 应用回调（可安全打日志）
3. `CleanSession=0` 且 `SessionPresent=1` 或拔线重连后 → `drain offline...`，每轮 `Process` 走一步 `MQTTYield`，不阻塞主循环
4. `CanPublish()` 在 `offline_draining` 期间为 0，避免与收离线包冲突

---

## 消息长度与 RAM

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `PAHO_MQTT_CLIENT_PAYLOAD_MAX` | **128** | 单条消息最大载荷（字节） |
| `PAHO_MQTT_CLIENT_SEND_BUF_SIZE` | 1024 | MQTT 发送缓冲 |
| `PAHO_MQTT_CLIENT_RECV_BUF_SIZE` | 1024 | MQTT 接收缓冲 |
| `PAHO_MQTT_PENDING_QUEUE_DEPTH` | 32 | pending 队列深度 |
| `PAHO_MQTT_CLIENT_OFFLINE_DRAIN_MS` | 3000 | 离线 drain 总超时（ms） |

pending 队列约占 `深度 × (PAYLOAD_MAX + TOPIC_MAX)` 字节 BSS。修改 `PAYLOAD_MAX` 须评估 RAM 与主栈（案例要求 **8KB** 栈）。

错误码基值：`ERROR_BASE_PAHO_MQTT_CLIENT = -4800`。

---

## 与 Net05（MQTT-C）对比

| 项目 | Net05 | Net06 |
|------|-------|-------|
| 库 | MQTT-C | Paho Embedded C |
| API | `mqtt_sync` 非阻塞 | `MQTTConnect` / `MQTTYield` |
| 封装 | `mqtt_client` | `paho_mqtt_client` |
| 业务 | tick + 命令 + 拔线恢复 | **相同** |

---

## Keil 集成

**封装与平台：**

- `paho_platform.c`
- `paho_mqtt_client.c`

**Paho 第三方：**

- `third_party/paho/MQTTClient-C/MQTTClient.c`
- `third_party/paho/MQTTPacket/MQTTPacket.c`
- `third_party/paho/MQTTPacket/MQTTConnectClient.c`
- `third_party/paho/MQTTPacket/MQTTSerializePublish.c`
- `third_party/paho/MQTTPacket/MQTTDeserializePublish.c`
- `third_party/paho/MQTTPacket/MQTTSubscribeClient.c`
- `third_party/paho/MQTTPacket/MQTTUnsubscribeClient.c`

**C/C++ Define：**

```text
MQTTCLIENT_PLATFORM_HEADER=paho_platform.h
```

**Include 路径：**

- `Middlewares/protocols/paho_mqtt`
- `Middlewares/protocols/paho_mqtt/third_party/paho/MQTTPacket`
- `Middlewares/protocols/paho_mqtt/third_party/paho/MQTTClient-C`

第三方 `MQTTClient.h` 将 `SUCCESS` 重命名为 `PAHO_MQTT_RC_SUCCESS`，避免与 SPL `ErrorStatus::SUCCESS` 冲突。

---

**最后更新**：2026-06-30
