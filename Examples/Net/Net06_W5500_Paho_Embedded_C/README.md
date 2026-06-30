# Net06 - W5500 Paho MQTT Client EXTI

小精灵 **STM32F103ZE** + **W5500**，使用 **Eclipse Paho Embedded C**（`MQTTClient-C`）连接 Broker。板子 **`192.168.101.201`**，PC Broker **`192.168.101.101:1883`**（Mosquitto + MQTTX）。

> 业务与 [Net05](../Net05_W5500_MQTT_C/README.md) 相同（周期 tick + 订阅收命令 + 拔线恢复），仅 MQTT 协议栈不同（Paho vs MQTT-C）。

---

## 案例目的

- 演示 **Paho Embedded C** over W5500 TCP
- 中间件 `Middlewares/protocols/paho_mqtt/`（`paho_platform` + `paho_mqtt_client`）
- EXTI + `W5500_PhyMonitor` 拔插网线恢复
- **持久会话 + QoS1** 离线小命令（单条 ≤128 字节）
- 与 PC **MQTTX** 联调

---

## 功能特性

| 项目 | 说明 |
|------|------|
| 本机 IP | `192.168.101.201` |
| Broker | `192.168.101.101:1883` |
| Client ID | `stm32_net06` |
| MQTTX **订阅**（看 tick） | `Net06/stm32_out` |
| MQTTX **发布**（发命令） | `Net06/stm32_in` |
| 周期发布 | 每 **5s** `tick=...`（串口打印 `TX tick=...`） |
| Keep Alive | 60s |
| Clean Session | **0**（持久会话） |
| QoS | **1** |
| 链路恢复 | `W5500_PhyMonitor` |
| 串口 | USART1 **115200** |
| 单条消息上限 | **128 字节** |

### 主题速查

| 你想做什么 | MQTTX 操作 | 主题 |
|------------|-----------|------|
| 看板子 tick | **订阅** | `Net06/stm32_out` |
| 给板子发命令 | **发布**（QoS 1） | `Net06/stm32_in` |

---

## 消息长度与离线条数

| 宏 / 项 | 值 | 说明 |
|---------|-----|------|
| `PAHO_MQTT_CLIENT_PAYLOAD_MAX` | 128 | 单条载荷上限（字节） |
| `PAHO_MQTT_PENDING_QUEUE_DEPTH` | 32 | 离线/突发最多缓存条数 |
| MQTTX 发布 | ≤128 字节 | 超长会被截断并打 `payload too large` |

本案例面向 **F103 + 有限 RAM**，不支持海量数据；长 JSON / 大配置请放 PC 或拆成多条短命令。

---

## 网络参数（`main_example.c`）

```c
#define NET_IP_ADDR               { 192, 168, 101, 201 }
#define NET_BROKER_ADDR           { 192, 168, 101, 101 }
#define NET_MQTT_CLIENT_ID        "stm32_net06"
#define NET_MQTT_TOPIC_MQTTX_SUB  "Net06/stm32_out"
#define NET_MQTT_TOPIC_MQTTX_PUB  "Net06/stm32_in"
#define NET_MQTT_QOS              1U
#define NET_MQTT_CLEAN_SESSION    0U
#define NET_PUB_INTERVAL_MS       5000U
```

---

## MQTTX 联调

1. PC 启动 Mosquitto（`1883`）
2. MQTTX 连接 Broker，**Client ID 勿用 `stm32_net06`**
3. MQTTX 建议 **关闭 Clean Session**（与板子持久会话一致）
4. **订阅** `Net06/stm32_out` → 每 5s 收到 `tick=...`
5. **发布**到 `Net06/stm32_in`（QoS **1**）→ 板子串口：

```text
[INFO ][MQTT] RX topic=Net06/stm32_in payload=hello
```

6. 板子每 5s 串口还会打印自己发出的 tick：

```text
[INFO ][MQTT] TX tick=14022
```

---

## 串口日志参考

### 首次上电 / 复位（Broker 已有会话）

```text
[INFO ][MAIN] === Net06 W5500 Paho MQTT Client EXTI ===
[INFO ][NET] PHY link: UP
[INFO ][MQTT] TCP connected 192.168.101.101:1883
[INFO ][MQTT] CONNACK ok id=stm32_net06 CleanSession=0 SessionPresent=1
[INFO ][MQTT] session restored listen Net06/stm32_in QoS 1
[INFO ][MQTT] drain offline...
[INFO ][MQTT] TX tick=8659
[INFO ][MQTT] RX topic=Net06/stm32_in payload=1
```

### 首次连接（无旧会话）

```text
[INFO ][MQTT] CONNACK ok id=stm32_net06 CleanSession=0 SessionPresent=0
[INFO ][MQTT] SUBACK ok listen Net06/stm32_in QoS 1
```

### 拔插网线

```text
[WARN ][NET] PHY link DOWN
[INFO ][NET] PHY link UP, recover net
[INFO ][MQTT] TCP connected ...
[INFO ][MQTT] session restored listen Net06/stm32_in QoS 1
[INFO ][MQTT] drain offline...
[INFO ][MQTT] RX topic=Net06/stm32_in payload=6
```

---

## 拔插网线（离线 QoS1 小命令）

**步骤：**

1. 等串口 `PHY link: UP` 且 MQTT 已连接（有 `CONNACK` / `session restored` 或 `SUBACK`）
2. **拔网线**，等 `PHY link DOWN`
3. MQTTX 向 `Net06/stm32_in` 发 `1` `2` `3` …（每条 **QoS 1**，每条 ≤128 字节）
4. **插回网线**，等 `PHY link UP, recover net`
5. 串口应出现 `drain offline...`，随后逐条 `RX topic=...`

**机制简述：**

- PHY DOWN → `ForceReconnect()` 关 Socket
- PHY UP → 主循环 `Drain()` 重建 TCP + MQTT
- `SessionPresent=1` 时免重复 SUBSCRIBE
- 离线消息在 `drain offline...` 阶段分步 `MQTTYield` 入队，主循环 `FlushPending()` 在浅栈上回调打印

| 项 | 限制 |
|----|------|
| 单条长度 | ≤ **128 字节** |
| 离线条数 | pending 队列 **32 条** |
| 主栈 | 须 **8KB** `startup`（见下） |

> **说明**：若同一 payload（如 `7`）在日志里出现多次，多为 MQTTX 重复发布或 Broker 侧 QoS1 重发积压，可重启 Broker / 换 Client ID 清空会话后再测。

---

## 主栈配置（必做）

默认 `Start/startup_stm32f10x_hd.s` 主栈仅 **1KB**。Net06 MQTT 栈较深，**1KB 会踩内存**，表现为串口乱码或死机。

**Keil 须换成本目录 8KB 启动文件：**

| 步骤 | 操作 |
|------|------|
| 1 | 工程树 **Start** 组 → 移除 `..\..\..\Start\startup_stm32f10x_hd.s` |
| 2 | 添加 `Examples/Net/Net06_W5500_Paho_Embedded_C/startup_stm32f10x_hd.s`（`Stack_Size = 0x2000`） |
| 3 | **Rebuild** 后烧录 |

可在 `Build/Keil/Listings/*.map` 中确认 `Stack_Size` 为 **0x2000**。

---

## Keil 工程

| 项 | 值 |
|----|-----|
| 工程 | [`Examples.uvprojx`](Examples.uvprojx) |
| Target | `Net06_W5500_Paho_Embedded_C` |
| 输出 | `Build/Keil/Objects/Net06_W5500_Paho_Embedded_C.axf` |
| 中间件 | `paho_platform.c`、`paho_mqtt_client.c` + Paho 第三方源文件 |
| 宏 | `MQTTCLIENT_PLATFORM_HEADER=paho_platform.h` |

`.uvprojx` 需手动维护；打开工程后若提示同步文件列表，选 **是**。

---

## 与 Net05 差异

| 对比项 | Net05 MQTT-C | Net06 Paho |
|--------|--------------|------------|
| MQTT 库 | MQTT-C | Paho Embedded C |
| 驱动 API | `mqtt_client.h` | `paho_mqtt_client.h` |
| 主题前缀 | `Net05/` | `Net06/` |
| Client ID | `stm32_net05` | `stm32_net06` |
| 收发包模型 | `mqtt_sync()` 非阻塞 | `MQTTConnect` / `MQTTYield` 分步 |
| 离线收包 | PAL 层处理 | pending 队列 + 分步 offline drain |
| **业务行为** | tick + 命令 + 拔线恢复 | **相同** |

硬件接线、W5500 EXTI、OLED 与 Net05 **完全相同**。

---

## 主循环结构

```text
Net_ProcessOnce()
  ├─ W5500_ProcessEvents / PhyMonitor
  ├─ PAHO_MQTT_Client_Drain(8)     → TCP / CONNECT / SUBSCRIBE / Yield / offline drain
  ├─ PAHO_MQTT_Client_FlushPending() → 投递 pending，回调里打 RX 日志
  └─ Net_ProcessPeriodicPublish()  → 每 5s Publish tick + TX 日志
```

---

## 相关参考

- [Net05 MQTT-C](../Net05_W5500_MQTT_C/README.md)
- [本目录工程说明](Doc/工程说明.md)
- [`Middlewares/protocols/paho_mqtt/README.md`](../../../Middlewares/protocols/paho_mqtt/README.md)
- [`Drivers/network/README.md`](../../../Drivers/network/README.md)

---

**最后更新**：2026-06-30
