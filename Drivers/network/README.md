# W5500 以太网驱动

基于 SPI 的 W5500 以太网控制器驱动，自 `w5500_Project_template` 迁移并符合主工程分层规范。

---

## 职责

- W5500 芯片寄存器读写（SPI VDM 帧格式）
- 硬件/软件复位、PHY 链路检测
- 静态 IP 配置、网关 ARP 探测
- Socket0：TCP Server/Client、UDP 收发
- 轮询方式处理 Socket 中断标志

---

## 硬件连接（默认）

| 信号 | STM32 引脚 |
|------|-----------|
| SCS  | PA4       |
| SCK  | PA5 (SPI1) |
| MISO | PA6       |
| MOSI | PA7       |
| RST  | PC15      |
| INT  | PC14（预留，当前轮询模式未使用） |

配置位于 `BSP/board.h` 的 `W5500_CONFIG` 宏。

---

## 依赖模块

| 模块 | 说明 |
|------|------|
| `spi_hw` | SPI 总线通信 |
| `gpio` | CS/RST 引脚控制 |
| `delay` | 阻塞延时与超时判断 |

---

## 初始化顺序

```c
System_Init();
SPI_HW_Init((SPI_Instance_t)W5500_SPI_INSTANCE);  /* 若 SPI 未被其他模块初始化 */
W5500_HardwareReset();
W5500_Init();
W5500_SetNetConfig(&net_cfg);
W5500_DetectGateway();
W5500_SocketInit(W5500_SOCKET_0, &sock_cfg);
W5500_SocketListen(W5500_SOCKET_0);  /* 或 SocketConnect / SocketOpenUdp */
```

主循环中调用 `W5500_InterruptProcess()`，根据 `W5500_GetSocketStatus()` 处理收发。

---

## 模块开关

`System/config.h`：

```c
#define CONFIG_MODULE_W5500_ENABLED  1
#define CONFIG_MODULE_SPI_ENABLED    1
```

---

## 注意事项

1. **SPI 总线冲突**：W5500 与 W25Q/TF_SPI 默认共用 SPI1 + PA4，不可同时挂载；使用 W5500 时请关闭 `CONFIG_MODULE_W25Q_ENABLED` 或修改 `board.h` 引脚。
2. **Socket 范围**：当前完整实现 Socket0；Socket1~7 的 `W5500_SocketInit` 返回 `W5500_ERROR_NOT_IMPLEMENTED`。
3. **无 DHCP**：仅支持静态 IP，需调用方填写 `W5500_NetConfig_t`。
4. **Keil 工程**：需手动将 `Drivers/network/w5500.c` 加入工程，并添加 Include 路径 `Drivers/network`。

---

## 模板演示流程摘要（供后续建案例参考）

原模板 `main.c` 演示 TCP Server 回显：

1. 本机 IP `192.168.1.199`，网关 `192.168.1.1`，端口 `5000`
2. `W5500_SocketListen(0)` 后等待连接
3. 主循环：`W5500_InterruptProcess()` → 收到数据则 `SocketRead` + `SocketWrite` 回显
4. 每 500ms 定时发送测试字符串（由上层用 `Delay_GetTick()` 实现）

---

## 相关文件

- `w5500.h` / `w5500.c` — 驱动 API 与实现
- `w5500_regs.h` — 寄存器定义（内部使用）

---

**最后更新**：2026-06-30
