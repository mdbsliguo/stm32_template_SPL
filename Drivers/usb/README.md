# USB - USB设备模块

## 📝 概述

USB（Universal Serial Bus）通用串行总线模块，用于实现STM32微控制器上的USB 2.0全速设备接口。本模块提供基础框架，完整的USB协议栈需要根据具体应用（HID/CDC/MSC等）扩展。

## ✨ 功能特性

- **USB 2.0全速设备**：支持12Mbps全速USB通信
- **端点配置**：支持最多8个端点（EP0-EP7）的配置
- **中断处理**：提供USB中断处理框架
- **事件回调**：支持USB事件回调函数
- **条件编译**：仅MD/HD/CL/XL型号支持，需要外部48MHz晶振

## ⚠️ 重要说明

1. **硬件要求**：
   - 仅MD/HD/CL/XL型号支持USB
   - 需要外部48MHz晶振或通过PLL生成48MHz时钟
   - USB引脚：PA11（D-）、PA12（D+）

2. **协议栈扩展**：
   - 本模块只提供基础框架
   - 完整的USB协议栈需要根据具体应用扩展：
     - **HID**：人机接口设备（键盘、鼠标等）
     - **CDC**：通信设备类（虚拟串口）
     - **MSC**：大容量存储设备（U盘）
     - **自定义设备类**

3. **端点配置**：
   - EP0：控制端点（必须）
   - EP1-EP7：数据端点（可选）

## 🚀 如何使用

### 1. 配置模块开关

在 `System/config.h` 中启用USB模块：

```c
#define CONFIG_MODULE_USB_ENABLED 1
```

### 2. 初始化USB

```c
#include "usb.h"

USB_Status_t status = USB_Init();
if (status != USB_OK) {
    // 处理错误
}
```

### 3. 配置端点

```c
USB_EP_Config_t ep_config = {
    .endpoint = 1,
    .type = USB_EP_TYPE_BULK,
    .tx_size = 64,
    .rx_size = 64,
    .enabled = 1
};

USB_ConfigEndpoint(&ep_config);
```

### 4. 使能USB

```c
USB_Enable();
```

### 5. 设置事件回调

```c
void usb_event_handler(void *user_data) {
    // 处理USB事件（连接、断开、数据传输等）
}

USB_SetEventCallback(usb_event_handler, NULL);
```

### 6. 发送/接收数据

```c
/* 发送数据 */
uint8_t tx_data[] = "Hello USB";
USB_Send(1, tx_data, sizeof(tx_data));

/* 接收数据 */
uint8_t rx_data[64];
uint16_t rx_length = sizeof(rx_data);
USB_Receive(1, rx_data, &rx_length);
```

## 📚 API 参考

- `USB_Status_t USB_Init(void)` - 初始化USB
- `USB_Status_t USB_Deinit(void)` - 反初始化USB
- `USB_Status_t USB_ConfigEndpoint(const USB_EP_Config_t *config)` - 配置端点
- `USB_Status_t USB_Send(uint8_t endpoint, const uint8_t *data, uint16_t length)` - 发送数据
- `USB_Status_t USB_Receive(uint8_t endpoint, uint8_t *data, uint16_t *length)` - 接收数据
- `USB_Status_t USB_SetEventCallback(USB_EventCallback_t callback, void *user_data)` - 设置事件回调
- `USB_Status_t USB_Enable(void)` - 使能USB
- `USB_Status_t USB_Disable(void)` - 禁用USB
- `uint8_t USB_IsConnected(void)` - 检查USB是否已连接
- `uint8_t USB_GetStatus(void)` - 获取USB当前状态

## 🔧 扩展建议

1. **实现USB描述符**：
   - 设备描述符
   - 配置描述符
   - 接口描述符
   - 端点描述符
   - 字符串描述符

2. **实现标准请求处理**：
   - GetDescriptor
   - SetAddress
   - SetConfiguration
   - GetStatus
   - ClearFeature
   - SetFeature

3. **实现设备类特定功能**：
   - HID报告描述符和处理
   - CDC接口配置
   - MSC块设备操作

## ⚠️ 注意事项

- **时钟配置**：确保系统时钟配置正确，USB需要48MHz时钟
- **中断优先级**：USB中断优先级应设置合理，避免影响实时性
- **缓冲区管理**：USB数据缓冲区需要正确管理，避免数据丢失
- **协议栈实现**：完整的USB协议栈实现需要参考USB 2.0规范和STM32 USB应用笔记

---

**最后更新**：2024-01-01

