# Net04 - W5500 UDP EXTI

小精灵 **STM32F103ZE** + **W5500**，Socket0 **UDP**，EXTI 处理收包事件。默认与 **PC 网络调试助手** 联调：板子 **`192.168.101.201:50000`**，PC **`192.168.101.101:8080`**。

> 同一套硬件轮流烧录 Net03 / Net04 时，本机 IP 均为 **`.201`**，无需改网线侧配置；`NET_PEER_ADDR` 指向 PC 助手 IP 即可。

---

## 📋 案例目的

- 演示 W5500 **UDP** + `W5500_SocketOpenUdp()`
- 演示 UDP **无连接**收发（对比 Net03 TCP Client）
- 解析 W5500 UDP 收包 **8 字节帧头**（源 IP/端口/长度）
- 用 **PC 网络调试助手** 完成收发与回显验证

---

## 🎯 功能特性

| 项目 | 说明 |
|------|------|
| 工作模式 | Socket0 **UDP**，无连接 |
| 本机 IP | 静态 `192.168.101.201`（与 Net03 一致，单板测试） |
| 本地端口 | `50000` |
| 默认对端（PC） | `192.168.101.101:8080`（`main_example.c` 中 `NET_PEER_*` 可改） |
| 发送 | 每 **500ms** 发 `\r\nW5500 UDP hello\r\n` 到默认对端 |
| 接收 | 轮询 `Sn_RX_RSR`，解析帧头，串口打印载荷 |
| 回显 | 收到 UDP 后 **回发给来源 IP:端口** |
| 链路恢复 | `W5500_PhyMonitor`：拔插网线自动重开 UDP Socket |
| 中断 | PF9 EXTI9 + `W5500_ProcessEvents` |
| 调试 | USART1 **115200** |

---

## 🔁 UDP 与 TCP（相对 Net03）

| | **TCP Net03** | **UDP Net04** |
|--|---------------|---------------|
| 连接 | `SocketConnect` | **无**，`SocketOpenUdp` 即可 |
| 助手模式 | TCP Client 连板子，或板子连助手 Server | **UDP**，配好双方 IP/端口 |
| 可靠性 | 有连接态 | 无连接，发完即走 |

UDP 无正式 Server/Client，只有 **bind 本地端口收** + **sendto 对端发**。Net04 同时做：周期发往 PC + 收包回显。

---

## ⚙️ 网络参数（`main_example.c`）

```c
#define NET_IP_ADDR       { 192, 168, 101, 201 }   /* 板子 */
#define NET_PEER_ADDR     { 192, 168, 101, 101 }   /* PC 助手 IP */
#define NET_LOCAL_PORT    50000U                   /* 板子监听 */
#define NET_PEER_PORT     8080U                    /* PC 助手端口 */
```

PC 实际 IP 若不是 `.101`，只改 `NET_PEER_ADDR`（或助手里改目标，两边一致即可）。

---

## 🚀 PC 网络调试助手设置（推荐）

### 电脑网卡

- IP：`192.168.101.101`
- 掩码：`255.255.255.0`
- 网关：可不填（同网段直连/交换机）

### 助手 UDP 模式

| 项 | 值 |
|----|-----|
| 协议 | **UDP** |
| 本地主机地址 | `192.168.101.101` |
| 本地主机端口 | `8080` |
| 远程主机地址 | `192.168.101.201` |
| 远程主机端口 | `50000` |

### 操作步骤

1. Keil Target **`Net04_W5500_UDP`**，编译烧录
2. 串口 115200，确认 `UDP bind :50000`、`UDP OK`
3. 助手应 **每 500ms 收到** `W5500 UDP hello`
4. 在助手发送区输入文字，点发送 → 目标 `192.168.101.201:50000`
5. 板子串口：`RX n bytes from 192.168.101.101:8080` + 载荷 + `UDP echo`
6. 助手应收到 **回显** 的相同内容

### 拔插网线

7. **拔网线** → `PHY link DOWN`，hello 停止
8. **插回** → `PHY link UP, recover net` → `UDP socket open :50000` → hello 恢复

```text
板子(.201:50000)  ──500ms hello──►  PC(.101:8080)
板子(.201:50000)  ◄──回显/手动──   PC(.101:8080)
```

---

## UDP 收包帧头（W5500）

| 偏移 | 长度 | 含义 |
|------|------|------|
| 0~3 | 4 | 来源 IP |
| 4~5 | 2 | 来源端口（大端） |
| 6~7 | 2 | 载荷长度（大端） |
| 8~ | N | 载荷 |

---

## 📝 串口日志参考

```text
[INFO ][MAIN] === Net04 W5500 UDP EXTI ===
[INFO ][NET] IP  192.168.101.201
[INFO ][NET] Peer 192.168.101.101:8080 local :50000
[INFO ][NET] UDP socket open :50000
[INFO ][NET] TX 22 bytes to 192.168.101.101:8080
[INFO ][NET] RX 5 bytes from 192.168.101.101:8080
hello
[INFO ][NET] UDP echo 5 bytes
```

### 拔插网线

```text
[WARN ][NET] PHY link DOWN
[INFO ][NET] waiting PHY link UP...
[INFO ][NET] PHY link UP, recover net
[INFO ][NET] UDP socket open :50000
[INFO ][NET] TX 22 bytes to 192.168.101.101:8080
```

---

## ❓ 常见问题

| 现象 | 处理 |
|------|------|
| 助手收不到 hello | 查 PC IP/端口是否为 `.101:8080`；防火墙；`NET_PEER_*` 是否与助手一致 |
| 有 TX 无 RX | 助手须向 **`.201:50000`** 发 **UDP**（不是 TCP） |
| 回显收不到 | 助手本地端口保持 `8080` 打开；勿与 TCP 模式混用同一端口 |
| 与 Net03 切换 | 本机均为 `.201`；烧录不同固件即可，助手改 TCP/UDP 模式 |
| 拔网线插回无 hello | 查 `W5500_PhyMonitor_Process`；插回后应有 `UDP socket open` |

### 拔插网线（`W5500_PhyMonitor`）

- 主循环 `W5500_PhyMonitor_Process()`；DOWN 时跳过收发
- `W5500_PhyMonitor_SetSocketWatch(..., W5500_PHY_WATCH_SOCKET_INIT, udp_on)` 监视 UDP Socket
- 链路 DOWN / Socket 丢失 → 关 Socket；UP 后回调 `Net_StartUdpSocket()` 重新 bind

驱动说明见 [`Drivers/network/README.md`](../../../Drivers/network/README.md)。

---

## 🛠️ Keil 工程

| 项 | 值 |
|----|-----|
| Target | `Net04_W5500_UDP` |
| 工程 | [`Examples.uvprojx`](Examples.uvprojx) |

---

## 🔗 相关参考

- [Net03 TCP Client](../Net03_W5500_Client/README.md)
- [`Drivers/network/README.md`](../../../Drivers/network/README.md)

---

**最后更新**：2026-06-30
