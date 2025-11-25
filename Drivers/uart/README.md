# UART通信模块

## 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `UART_Init()` | 初始化UART外设和GPIO（USART1/USART2/USART3） |
| 反初始化 | `UART_Deinit()` | 反初始化UART外设 |
| 发送数据 | `UART_Transmit()` | 阻塞式发送数据缓冲区 |
| 接收数据 | `UART_Receive()` | 阻塞式接收数据缓冲区 |
| 发送单字节 | `UART_TransmitByte()` | 发送单个字节 |
| 接收单字节 | `UART_ReceiveByte()` | 接收单个字节 |
| 发送字符串 | `UART_TransmitString()` | 发送字符串（以'\0'结尾） |
| 状态检查 | `UART_IsInitialized()` | 检查是否已初始化 |
| 获取外设 | `UART_GetPeriph()` | 获取UART外设指针 |

## 重要提醒

1. **实例选择**：支持USART1、USART2、USART3三个实例，使用`UART_INSTANCE_1`、`UART_INSTANCE_2`或`UART_INSTANCE_3`
2. **配置驱动**：GPIO和UART参数在`BSP/board.h`中配置，自动初始化
3. **超时处理**：所有传输函数支持超时参数，0表示使用默认超时（1000ms）
4. **阻塞模式**：当前实现为阻塞模式，发送/接收期间会等待完成
5. **引脚配置**：TX引脚配置为复用推挽输出，RX引脚配置为浮空输入
6. **时钟管理**：自动使能UART外设时钟和GPIO时钟

## 配置说明

在`BSP/board.h`中配置UART：

```c
#define UART_CONFIGS {                                                                                    \
    {USART1, GPIOA, GPIO_Pin_9, GPIOA, GPIO_Pin_10, 115200, USART_WordLength_8b, USART_StopBits_1, USART_Parity_No, 1}, /* UART1：PA9(TX), PA10(RX)，115200，8N1，启用 */ \
}
```

## 使用示例

```c
/* 初始化UART1 */
UART_Init(UART_INSTANCE_1);

/* 发送字符串 */
UART_TransmitString(UART_INSTANCE_1, "Hello World\r\n", 0);

/* 发送数据 */
uint8_t data[] = {0x01, 0x02, 0x03};
UART_Transmit(UART_INSTANCE_1, data, sizeof(data), 0);

/* 接收数据 */
uint8_t rx_data[10];
UART_Receive(UART_INSTANCE_1, rx_data, sizeof(rx_data), 1000);
```
