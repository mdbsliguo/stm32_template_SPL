# ModBusRTU 协议栈模�?
## 📋 模块简�?
`modbus_rtu` 是基�?ModBusRTU 协议的通信模块，提供完整的 ModBusRTU 协议栈实现，支持主机和从机模式，包含常用功能码和 CRC16 校验�?
### 核心特�?
- �?**主机模式**：支持作为主站读取和写入从站数据
- �?**从机模式**：支持作为从站响应主站请�?- �?**常用功能�?*：支�?03（读保持寄存器）�?6（写单个寄存器）�?0/16（写多个寄存器）
- �?**CRC16 校验**：完整的 CRC16 校验计算和验�?- �?**错误处理**：统一的错误码系统和异常响应处�?- �?**超时控制**：支持可配置的超时时�?- �?**多实例支�?*：支持多�?UART 实例

## 🎯 使用场景

适用于需要通过 ModBusRTU 协议与工业设备通信的应用，例如�?
- **传感器数据采�?*：读取气体传感器、温度传感器�?ModBusRTU 设备数据
- **工业控制**：控�?ModBusRTU 从站设备（如变频器、PLC 等）
- **数据采集系统**：作�?ModBusRTU 从站，响应主站的数据采集请求

## 📚 API 说明

### 主机模式函数

#### ModBusRTU_ReadHoldingRegisters

读保持寄存器（功能码 03）�?
```c
ModBusRTU_Status_t ModBusRTU_ReadHoldingRegisters(
    UART_Instance_t uart_instance,  // UART实例索引
    uint8_t slave_address,          // 从机地址�?-247�?    uint16_t start_address,         // 起始寄存器地址
    uint16_t register_count,        // 寄存器数量（1-125�?    uint16_t *data,                 // 接收数据缓冲�?    uint32_t timeout                // 超时时间（毫秒）�?表示使用默认超时
);
```

**示例**�?
```c
uint16_t registers[10];
ModBusRTU_Status_t status;

status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_1, 0x01, 0x0000, 10, registers, 1000);
if (status == ModBusRTU_OK) {
    // 读取成功，registers[0] �?registers[9] 包含读取的数�?}
```

#### ModBusRTU_WriteSingleRegister

写单个寄存器（功能码 06）�?
```c
ModBusRTU_Status_t ModBusRTU_WriteSingleRegister(
    UART_Instance_t uart_instance,  // UART实例索引
    uint8_t slave_address,          // 从机地址�?-247�?    uint16_t register_address,      // 寄存器地址
    uint16_t value,                 // 寄存器值（16位）
    uint32_t timeout                // 超时时间（毫秒）�?表示使用默认超时
);
```

**示例**�?
```c
ModBusRTU_Status_t status;

status = ModBusRTU_WriteSingleRegister(UART_INSTANCE_1, 0x01, 0x0000, 0x1234, 1000);
if (status == ModBusRTU_OK) {
    // 写入成功
}
```

#### ModBusRTU_WriteMultipleRegisters

写多个寄存器（功能码 10/16）�?
```c
ModBusRTU_Status_t ModBusRTU_WriteMultipleRegisters(
    UART_Instance_t uart_instance,  // UART实例索引
    uint8_t slave_address,          // 从机地址�?-247�?    uint16_t start_address,         // 起始寄存器地址
    uint16_t register_count,        // 寄存器数量（1-123�?    const uint16_t *data,           // 要写入的数据
    uint32_t timeout                // 超时时间（毫秒）�?表示使用默认超时
);
```

**示例**�?
```c
uint16_t data[5] = {0x0001, 0x0002, 0x0003, 0x0004, 0x0005};
ModBusRTU_Status_t status;

status = ModBusRTU_WriteMultipleRegisters(UART_INSTANCE_1, 0x01, 0x0000, 5, data, 1000);
if (status == ModBusRTU_OK) {
    // 写入成功
}
```

### 从机模式函数

#### ModBusRTU_SlaveInit

从机初始化�?
```c
ModBusRTU_Status_t ModBusRTU_SlaveInit(const ModBusRTU_Config_t *config);
```

**配置结构�?*�?
```c
typedef struct {
    UART_Instance_t uart_instance;  // UART实例索引
    uint8_t slave_address;           // 从机地址�?-247�?    uint32_t timeout_ms;             // 超时时间（毫秒）�?表示使用默认超时
    uint8_t retry_count;             // 重试次数
} ModBusRTU_Config_t;
```

**示例**�?
```c
ModBusRTU_Config_t config;
ModBusRTU_Status_t status;

config.uart_instance = UART_INSTANCE_1;
config.slave_address = 0x01;
config.timeout_ms = 1000;
config.retry_count = 3;

status = ModBusRTU_SlaveInit(&config);
if (status == ModBusRTU_OK) {
    // 初始化成�?}
```

#### ModBusRTU_SlaveProcess

从机处理请求（轮询方式）�?
```c
ModBusRTU_Status_t ModBusRTU_SlaveProcess(
    uint8_t *function_code,      // 接收到的功能码（输出�?    uint16_t *start_address,     // 接收到的起始地址（输出）
    uint16_t *register_count,    // 接收到的寄存器数量（输出�?    uint8_t *data,                // 接收/发送数据缓冲区
    uint16_t *data_size,          // 输入时表示缓冲区大小，输出时表示实际数据长度
    uint32_t timeout              // 超时时间（毫秒）�?表示使用默认超时
);
```

**示例**�?
```c
uint8_t function_code;
uint16_t start_address;
uint16_t register_count;
uint8_t data[256];
uint16_t data_size = sizeof(data);
ModBusRTU_Status_t status;

// 在主循环中周期性调�?status = ModBusRTU_SlaveProcess(&function_code, &start_address, &register_count,
                                 data, &data_size, 1000);
if (status == ModBusRTU_OK) {
    // 接收到请求，根据功能码处�?    switch (function_code) {
        case ModBusRTU_FUNC_READ_HOLDING_REGISTERS:
            // 处理读请求，准备响应数据
            // ...
            ModBusRTU_SlaveSendResponse(UART_INSTANCE_1, 0x01, function_code, data, data_size, 1000);
            break;
        case ModBusRTU_FUNC_WRITE_SINGLE_REGISTER:
            // 处理写请�?            // ...
            ModBusRTU_SlaveSendResponse(UART_INSTANCE_1, 0x01, function_code, NULL, 0, 1000);
            break;
    }
}
```

#### ModBusRTU_SlaveSendResponse

从机发送响应�?
```c
ModBusRTU_Status_t ModBusRTU_SlaveSendResponse(
    UART_Instance_t uart_instance,  // UART实例索引
    uint8_t slave_address,          // 从机地址
    uint8_t function_code,          // 功能�?    const uint8_t *data,            // 响应数据缓冲�?    uint16_t data_length,           // 数据长度（字节数�?    uint32_t timeout                // 超时时间（毫秒）�?表示使用默认超时
);
```

#### ModBusRTU_SlaveSendException

从机发送异常响应�?
```c
ModBusRTU_Status_t ModBusRTU_SlaveSendException(
    UART_Instance_t uart_instance,  // UART实例索引
    uint8_t slave_address,          // 从机地址
    uint8_t function_code,          // 功能�?    ModBusRTU_ExceptionCode_t exception_code,  // 异常�?    uint32_t timeout                // 超时时间（毫秒）�?表示使用默认超时
);
```

### 工具函数

#### ModBusRTU_CalculateCRC16

计算 CRC16 校验码�?
```c
uint16_t ModBusRTU_CalculateCRC16(const uint8_t *data, uint16_t length);
```

## 🔧 配置说明

### 启用模块

�?`System/config.h` 中启�?ModBusRTU 模块�?
```c
#define CONFIG_MODULE_MODBUS_RTU_ENABLED  1   /**< ModBusRTU协议栈模块开�?*/
```

### UART 配置

�?`BSP/board.h` 中配�?UART（ModBusRTU 使用 UART 进行通信）：

```c
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 9600, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)�?600�?N1，启�?*/ \
}
```

**注意**：ModBusRTU 通常使用 9600 波特率�? 数据位�? 停止位、无校验位（8N1）�?
## 📊 错误码说�?
| 错误�?| 说明 |
|--------|------|
| `ModBusRTU_OK` | 操作成功 |
| `ModBusRTU_ERROR_NULL_PTR` | 空指针错�?|
| `ModBusRTU_ERROR_INVALID_PARAM` | 参数非法 |
| `ModBusRTU_ERROR_INVALID_INSTANCE` | 无效实例编号 |
| `ModBusRTU_ERROR_NOT_INITIALIZED` | 未初始化 |
| `ModBusRTU_ERROR_TIMEOUT` | 操作超时 |
| `ModBusRTU_ERROR_CRC` | CRC 校验错误 |
| `ModBusRTU_ERROR_INVALID_RESPONSE` | 无效响应 |
| `ModBusRTU_ERROR_INVALID_ADDRESS` | 无效地址 |
| `ModBusRTU_ERROR_INVALID_FUNCTION_CODE` | 无效功能�?|
| `ModBusRTU_ERROR_EXCEPTION` | 异常响应 |

## ⚠️ 注意事项

1. **UART 初始�?*：使�?ModBusRTU 前，必须先初始化对应�?UART 实例
2. **超时设置**：根据实际通信环境设置合适的超时时间，建�?500ms 以上
3. **从机地址范围**：从机地址范围�?1-247�? �?248-255 为保留地址
4. **寄存器数量限�?*�?   - 读保持寄存器：最�?125 个寄存器
   - 写多个寄存器：最�?123 个寄存器
5. **CRC 校验**：所有帧都包�?CRC16 校验，自动验�?6. **异常响应**：从机可以发送异常响应，主机会自动识别并返回 `ModBusRTU_ERROR_EXCEPTION`
7. **从机模式**：从机模式使用轮询方式，需要周期性调�?`ModBusRTU_SlaveProcess()`

## 🔗 依赖关系

### 必需依赖

- **UART 驱动�?*：通过 `uart.h` 接口调用 `UART_Transmit()` �?`UART_Receive()`
- **Delay 模块**：用于超时处理（可选，如果使用阻塞延时�?
### 不直接访问硬�?
- 遵循项目解耦原则，中间件层不直接访问硬�?- 所有硬件操作通过驱动层接口完�?
## 📖 使用示例

### 主机模式示例：读取气体传感器数据

```c
#include "modbus_rtu.h"
#include "uart.h"

int main(void)
{
    uint16_t registers[10];
    ModBusRTU_Status_t status;
    
    /* 初始化系�?*/
    System_Init();
    
    /* 初始化UART1 */
    UART_Init(UART_INSTANCE_1);
    
    /* 读取10个保持寄存器（地址0x0000开始） */
    status = ModBusRTU_ReadHoldingRegisters(UART_INSTANCE_1, 0x01, 0x0000, 10, registers, 1000);
    
    if (status == ModBusRTU_OK) {
        /* 解析数据 */
        uint16_t gas_concentration = registers[1];  /* 寄存�?：当前气体浓�?*/
        uint16_t temperature = registers[7];        /* 寄存�?：环境温�?*/
        /* ... */
    }
    
    while(1) {
        /* 主循�?*/
    }
}
```

### 从机模式示例：响应主站请�?
```c
#include "modbus_rtu.h"
#include "uart.h"

int main(void)
{
    ModBusRTU_Config_t config;
    uint8_t function_code;
    uint16_t start_address;
    uint16_t register_count;
    uint8_t data[256];
    uint16_t data_size;
    ModBusRTU_Status_t status;
    
    /* 初始化系�?*/
    System_Init();
    
    /* 初始化UART1 */
    UART_Init(UART_INSTANCE_1);
    
    /* 配置从机 */
    config.uart_instance = UART_INSTANCE_1;
    config.slave_address = 0x01;
    config.timeout_ms = 1000;
    config.retry_count = 3;
    
    ModBusRTU_SlaveInit(&config);
    
    while(1) {
        /* 处理请求 */
        data_size = sizeof(data);
        status = ModBusRTU_SlaveProcess(&function_code, &start_address, &register_count,
                                        data, &data_size, 1000);
        
        if (status == ModBusRTU_OK) {
            switch (function_code) {
                case ModBusRTU_FUNC_READ_HOLDING_REGISTERS:
                    /* 准备响应数据 */
                    /* ... */
                    ModBusRTU_SlaveSendResponse(UART_INSTANCE_1, 0x01, function_code,
                                                data, register_count * 2, 1000);
                    break;
                    
                case ModBusRTU_FUNC_WRITE_SINGLE_REGISTER:
                    /* 处理写请�?*/
                    /* ... */
                    ModBusRTU_SlaveSendResponse(UART_INSTANCE_1, 0x01, function_code, NULL, 0, 1000);
                    break;
            }
        }
    }
}
```

## 📝 更新日志

### v1.0.0 (2024-01-01)

- 初始版本
- 实现主机模式（读保持寄存器、写单个寄存器、写多个寄存器）
- 实现从机模式（初始化、请求处理、响应发送）
- 实现 CRC16 校验计算和验�?
