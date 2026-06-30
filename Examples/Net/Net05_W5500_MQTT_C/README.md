# Net05 - W5500 MQTT Client EXTI

小精灵 **STM32F103ZE** + **W5500**，**MQTT Client**（[MQTT-C](https://github.com/LiamBindle/MQTT-C)），EXTI 事件驱动 TCP。板子 **`192.168.101.201`**，Broker 默认 **`192.168.101.101:1883`**（PC 上 Mosquitto + MQTTX）。

---

## 案例目的

- 演示 **MQTT over TCP**（非裸 TCP 字符串）
- 中间件 `Middlewares/protocols/mqtt/`（PAL + `mqtt_client` 封装）
- **自动重连**：`mqtt_init_reconnect` + W5500 TCP 重建
- 与 PC **MQTTX** 联调（本文含 **MQTTX 新手教程**）

---

## 功能特性

| 项目 | 说明 |
|------|------|
| 本机 IP | `192.168.101.201` |
| Broker | `192.168.101.101:1883` |
| Client ID | `stm32_net05` |
| 看板子 tick（MQTTX **订阅**） | `Net05/stm32_out` |
| 给板子发命令（MQTTX **发布**） | `Net05/stm32_in` |
| 板子周期发布 | 每 **5s** 向 `stm32_out` 发 `tick=...` |
| Keep Alive | 60s |
| Clean Session | **0**（持久会话，断线重连后 Broker 可保留订阅） |
| QoS | **1**（订阅 `stm32_in`、发布 `stm32_out` tick 均为 QoS 1） |
| 链路恢复 | `W5500_PhyMonitor`：拔插网线自动 MQTT 重连 |
| 串口 | USART1 **115200** |

---

## 五分钟搞懂 MQTT（第一次接触必读）

### 三个角色

```text
  [板子 Net05]  ←──以太网──→  [Broker 经纪人在 PC 上]  ←──WiFi/网线──→  [MQTTX 你的电脑]
   MQTT 客户端                    Mosquitto                         MQTT 调试工具
```

- **Broker（经纪人）**：像邮局，只负责**转发**消息，不解析业务。PC 上跑 **Mosquitto**。
- **Client（客户端）**：板子和 MQTTX 都是客户端，都连到同一个 Broker。
- **主题 Topic**：消息的「信箱名字」，例如 `Net05/stm32_out`。发信时必须写清主题。

### 本案例两个主题（先看这张表）

| 你想做什么 | MQTTX 操作 | 填这个主题 | 含义 |
|------------|-----------|------------|------|
| **看板子**每 5s 的 `tick=...` | **订阅** Subscribe | `Net05/stm32_out` | **out** = 板子往外发 |
| **给板子**发命令、字符串 | **发布** Publish | `Net05/stm32_in` | **in** = 发进板子 |

记忆口诀：

```text
看数据  →  订阅  stm32_out  （板子 out）
发命令  →  发布  stm32_in    （板子 in）
```

### 订阅 vs 发布（操作含义）

| 操作 | 英文 | 含义 | 本案例谁在用 |
|------|------|------|--------------|
| **发布** | Publish | 往某个主题**发**一条消息 | 板子发 tick 到 `stm32_out`；你在 MQTTX **发布**到 `stm32_in` 给板子 |
| **订阅** | Subscribe | **收**某个主题上的消息 | 板子订阅 `stm32_in`；你在 MQTTX **订阅** `stm32_out` 看 tick |

```text
想让别人收到  →  发布（Publish）到该主题
想看到别人发的  →  订阅（Subscribe）该主题
```

> **不能用** TCP/UDP 网络调试助手测 MQTT，必须用 **Broker + MQTTX**。

---

## 第一步：准备 PC 环境

### 1.1 设置 PC 网卡 IP

与板子同网段（网线接交换机或直连）：

| 项 | 值 |
|----|-----|
| IP 地址 | `192.168.101.101` |
| 子网掩码 | `255.255.255.0` |
| 网关 | 可留空或 `192.168.101.1` |

### 1.2 安装并启动 Mosquitto（Broker）

1. 打开 [Mosquitto 官网](https://mosquitto.org/download/) 下载 Windows 安装包并安装。
2. 安装后确认服务在跑（默认监听 **1883** 端口）：
   - 服务管理器里找 **Mosquitto Broker**，状态为「正在运行」；或
   - 命令行：`netstat -an | findstr 1883`，应能看到 `0.0.0.0:1883` 在 LISTENING。
3. 本案例使用**匿名连接**（无用户名密码），与 Mosquitto 默认配置一致。

若 1883 被占用或 Broker 未启动，板子串口会一直连不上。

### 1.3 安装 MQTTX

1. 打开 [MQTTX 官网](https://mqttx.app/zh/downloads) 下载并安装（有 Windows 版）。
2. 打开后界面一般为：左侧连接列表、右侧消息区。

---

## 第二步：MQTTX 新建连接（连 Broker）

1. 点击左上角 **「+」** 或 **「新建连接」**。
2. 按下面表格填写（名称可自取，其余建议与板子一致）：

| 配置项 | 填什么 | 说明 |
|--------|--------|------|
| **名称** | `Net05本地` | 随意，方便识别 |
| **Host** | `192.168.101.101` | PC 的 IP，即 Broker 地址 |
| **Port** | `1883` | MQTT 默认端口 |
| **Client ID** | `mqttx_pc_01` | **不要**与板子相同；板子是 `stm32_net05` |
| **Username / Password** | 留空 | 本案例未启用鉴权 |
| **SSL/TLS** | 关闭 | 局域网明文 1883 |

3. 点击 **「连接」**，左上角应显示 **已连接**（绿色）。

若连接失败：检查 Mosquitto 是否启动、PC 防火墙是否放行 1883、IP 是否为 `.101`。

---

## 第三步：看板子发出来的数据（订阅）

1. 在 MQTTX 连接成功后的界面，找到 **「订阅」** 区域。
2. 点击 **「新建订阅」** 或 **「+」**。
3. **Topic（主题）** 填：

   ```text
   Net05/stm32_out
   ```

4. **QoS** 选 **1**（与板子一致）。
5. 确认订阅。

> 主题名带 **out**：板子**往外**发数据，所以你在 MQTTX **订阅**它。

### 同时做的准备

1. 板子烧录 **Net05**，网线接好，串口 **115200** 打开。
2. 串口应陆续出现类似：

   ```text
   [INFO][NET] IP  192.168.101.201
   [INFO][NET] Broker 192.168.101.101:1883
   [INFO][MQTT] TCP connected 192.168.101.101:1883
   [INFO][MQTT] subscribe queued Net05/stm32_in (wait SUBACK)
   [INFO][MQTT] SUBACK ok, listen Net05/stm32_in
   [INFO][MQTT] CONNACK ok, publish Net05/stm32_out
   [INFO][MQTT] TX tick=5000
   ```

3. MQTTX 订阅 `Net05/stm32_out` 后，约每 **5 秒** 应收到一条：

> `session ready` / `SUBACK ok` 出现后再发命令，可避免重连后第一条丢失。

   ```text
   tick=12345
   ```

收到说明：**板子 → Broker → MQTTX** 整条链路正常。

---

## 第四步：给板子发命令（发布）

要让**板子串口**打印你发的内容，必须在 MQTTX 里 **发布**，不是订阅。

1. 在 MQTTX 找到 **「发布」** / **「Publish」** 区域（通常在订阅列表下方或右侧）。
2. 填写：

| 配置项 | 填什么 |
|--------|--------|
| **Topic（主题）** | `Net05/stm32_in` |
| **Payload（消息内容）** | 任意，例如 `hello` 或 `开灯` |
| **QoS** | `1` |

3. 点击 **「发送」** / 纸飞机图标。

> 主题名带 **in**：消息**发进**板子，所以你在 MQTTX **发布**到它。

### 板子串口应看到

```text
[INFO][MQTT] broker PUBLISH qos=1 len=5
[INFO][MQTT] RX topic (14 bytes)
Net05/stm32_in
[INFO][MQTT] RX payload (5 bytes)
hello
```

### 常见误区

| 错误做法 | 结果 |
|----------|------|
| 想收 tick 却订阅了 `stm32_in` | 收不到 tick（应订阅 **`stm32_out`**） |
| 想控板子却发布到 `stm32_out` | 板子收不到（应发布到 **`stm32_in`**） |
| 仍用旧主题 `device/net05/telemetry` 等 | 与当前固件不匹配，收不到 |
| 未等串口 `session ready` 就发 | 可能板子还没订阅成功，第一条会丢 |

---

## 第五步：串口中文不乱码

MQTT 载荷是**原始字节**。MQTTX 默认发 **UTF-8** 中文。

请把串口助手编码设为 **UTF-8**（SSCOM / MobaXterm 等均有该选项）。  
若助手是 GBK 而消息是 UTF-8，中文会显示乱码；英文 `hello` 不受影响。

---

## 联调检查清单

按顺序打勾：

- [ ] PC IP = `192.168.101.101`，Mosquitto 已启动（1883 监听）
- [ ] 板子 IP = `192.168.101.201`，PHY 链路 UP
- [ ] MQTTX 已连接 `192.168.101.101:1883`，Client ID **不是** `stm32_net05`
- [ ] 串口出现 `CONNACK ok` / `SUBACK ok` 和 `TX tick=...`
- [ ] MQTTX **订阅** `Net05/stm32_out` → 能看到 `tick=...`
- [ ] MQTTX **发布** 到 `Net05/stm32_in` → 板子串口打印 payload

---

## 参数与载荷限制

| 项目 | 说明 |
|------|------|
| 单次消息建议 | 载荷 **&lt; 900 字节**（MQTT 收缓冲 1024 字节，含包头） |
| 测试用语 | 几十字节足够（`hello`、短 JSON 均可） |
| 主题长度 | 最长 64 字符（`Net05/stm32_out` 足够短） |

---

## 主循环（实现参考）

```text
W5500_ProcessEvents -> MQTT_Client_Drain -> 周期 Publish
```

`MQTT_Client_Drain(n)`：连续最多 `n` 次 `mqtt_sync`，用于排空 TCP/MQTT 收包或发出 Publish。

---

## 模块结构

```text
Middlewares/protocols/mqtt/
├── mqtt_client.c/h       # 工程封装（error_code_t、W5500 TCP）
├── mqtt_pal.c/h          # MQTT-C 平台层（SocketRead/Write）
└── third_party/mqtt-c/   # MQTT-C 源码
```

---

## 配置（`main_example.c`）

```c
#define NET_IP_ADDR             { 192, 168, 101, 201 }
#define NET_BROKER_ADDR         { 192, 168, 101, 101 }
#define NET_BROKER_PORT         1883U
#define NET_MQTT_CLIENT_ID      "stm32_net05"
#define NET_MQTT_TOPIC_MQTTX_SUB  "Net05/stm32_out"   /* MQTTX 订阅：看 tick */
#define NET_MQTT_TOPIC_MQTTX_PUB  "Net05/stm32_in"    /* MQTTX 发布：发命令 */
#define NET_MQTT_SUB_TOPIC        NET_MQTT_TOPIC_MQTTX_PUB
#define NET_MQTT_PUB_TOPIC        NET_MQTT_TOPIC_MQTTX_SUB
#define NET_MQTT_QOS              1U
#define NET_PUB_INTERVAL_MS     5000U
```

PC IP 若不是 `.101`，只改 `NET_BROKER_ADDR` 并重新编译。

---

## 常见问题

| 现象 | 处理 |
|------|------|
| MQTTX 连不上 Broker | 查 Mosquitto 服务、防火墙、IP/端口 |
| 能连 Broker，板子无 `TCP connected` | 查板子网线、Broker 地址是否为 PC IP |
| 有 `TX tick`，MQTTX 收不到 | MQTTX 应**订阅** `Net05/stm32_out`（不是 `stm32_in`） |
| MQTTX 发了，板子无 RX | MQTTX 应**发布**到 `Net05/stm32_in`（不是 `stm32_out`） |
| 中文乱码 | 串口助手改 **UTF-8** |
| Client ID 冲突 | MQTTX 与板子不要用同一个 ID |
| 断线后第一条命令丢失 | 等串口 `SUBACK ok` 再发；拔网线后会自动重连 |

### 拔插网线（`W5500_PhyMonitor`）

主循环 `W5500_PhyMonitor_Process()`。MQTT 会话活跃时用 `W5500_PHY_WATCH_TCP_CONN` 监视底层 TCP；链路事件触发 `MQTT_Client_ForceReconnect()` + `Drain()`。

| 阶段 | 串口预期 |
|------|----------|
| 拔线 | `PHY link DOWN`，OLED `MQTT OFF` |
| 等待 | 每 5s `waiting PHY link UP...` |
| 插回 | `PHY link UP, recover net` → `TCP connected` → `CONNACK ok` → `SUBACK ok` → `TX tick=...` |

```text
[WARN ][NET] PHY link DOWN
[INFO ][NET] waiting PHY link UP...
[INFO ][NET] PHY link UP, recover net
[INFO ][MQTT] TCP connected
[INFO ][MQTT] CONNACK ok
[INFO ][MQTT] SUBACK ok
[INFO ][MQTT] TX tick=12345
```

> 插回后请等 `SUBACK ok` 再向 `Net05/stm32_in` 发命令，避免首条命令丢失。

驱动 API 见 [`Drivers/network/README.md`](../../../Drivers/network/README.md)「PHY 链路监控」。

---

## MQTT 会话（Clean Session）

板子连接 Broker 时 **Clean Session = 0**（持久会话）：

- Broker 会记住 `stm32_net05` 对 `Net05/stm32_in` 的订阅
- 短暂断线重连后，可能无需等板子重新 SUBSCRIBE 也能收到部分消息（视 Broker 与 QoS 而定）
- MQTTX 连接建议同样 **关闭 Clean Session**（与板子一致即可，Client ID 仍须不同）

修改位置：`Middlewares/protocols/mqtt/mqtt_client.c` 中 `MQTT_CLIENT_CONNECT_FLAGS`（`0` = 关，`MQTT_CONNECT_CLEAN_SESSION` = 开）。

---

## Keil 工程

| 项 | 值 |
|----|-----|
| Target | `Net05_W5500_MQTT_C` |
| 新增源文件 | `mqtt.c`、`mqtt_pal.c`、`mqtt_client.c` |
| Include | `Middlewares/protocols/mqtt`、`third_party/mqtt-c` |

---

## 相关参考

- [Net03 TCP Client](../Net03_W5500_Client/README.md)
- [Net04 UDP](../Net04_W5500_UDP/README.md)
- [MQTTX 官方文档](https://mqttx.app/zh/docs)

---

**最后更新**：2026-06-30
