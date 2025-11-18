# DMA模块

## 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `DMA_Init()` | 初始化DMA通道 |
| 反初始化 | `DMA_Deinit()` | 反初始化DMA通道 |
| 配置传输 | `DMA_ConfigTransfer()` | 配置外设到内存/内存到外设传输 |
| 配置内存传输 | `DMA_ConfigMemoryToMemory()` | 配置内存到内存传输 |
| 启动传输 | `DMA_Start()` | 启动DMA传输 |
| 停止传输 | `DMA_Stop()` | 停止DMA传输 |
| 等待完成 | `DMA_WaitComplete()` | 阻塞式等待传输完成 |
| 检查完成 | `DMA_IsComplete()` | 非阻塞式检查传输是否完成 |
| 获取剩余量 | `DMA_GetRemainingDataSize()` | 获取剩余传输数据量 |
| 设置回调 | `DMA_SetTransferCompleteCallback()` | 设置传输完成回调函数 |

## 重要提醒

1. **通道选择**：
   - **DMA1**：所有型号支持7个通道（Channel1-7）
   - **DMA2**：仅HD/CL/HD_VL型号支持5个通道（Channel1-5），MD/LD型号不支持
   - 模块会根据芯片型号自动适配，使用条件编译
2. **配置驱动**：DMA参数在`BSP/board.h`中配置，自动初始化
3. **传输方向**：支持外设到内存、内存到外设、内存到内存三种传输方向
4. **数据宽度**：支持字节（1）、半字（2）、字（4）三种数据宽度
5. **传输模式**：支持正常模式和循环模式
6. **回调函数**：支持传输完成回调，可在中断中调用

## 配置说明

在`BSP/board.h`中配置DMA：

```c
#define DMA_CONFIGS {                                                                                    \
    {(uint32_t)&USART1->DR, DMA_DIR_MEMORY_TO_PERIPHERAL, DMA_PeripheralInc_Disable, DMA_MemoryInc_Enable, DMA_PeripheralDataSize_Byte, DMA_Mode_Normal, DMA_Priority_Medium, 1}, /* DMA1_CH1：UART1 TX，启用 */ \
}
```

## 使用示例

```c
/* 初始化DMA通道 */
DMA_Init(DMA_CHANNEL_1_1);

/* 配置UART1 TX DMA传输 */
uint8_t tx_data[] = "Hello DMA!";
DMA_ConfigTransfer(DMA_CHANNEL_1_1, (uint32_t)&USART1->DR, (uint32_t)tx_data,
                   sizeof(tx_data), DMA_DIR_MEMORY_TO_PERIPHERAL, 1);

/* 启动传输 */
DMA_Start(DMA_CHANNEL_1_1);

/* 等待传输完成 */
DMA_WaitComplete(DMA_CHANNEL_1_1, 1000);

/* 配置ADC DMA传输（循环模式） */
uint16_t adc_buffer[100];
DMA_ConfigTransfer(DMA_CHANNEL_2_1, (uint32_t)&ADC1->DR, (uint32_t)adc_buffer,
                   100, DMA_DIR_PERIPHERAL_TO_MEMORY, 2);
DMA_Start(DMA_CHANNEL_2_1);

/* 内存到内存传输 */
uint8_t src[100], dst[100];
DMA_ConfigMemoryToMemory(DMA_CHANNEL_1_7, (uint32_t)src, (uint32_t)dst,
                         100, 1);
DMA_Start(DMA_CHANNEL_1_7);
DMA_WaitComplete(DMA_CHANNEL_1_7, 0);
```

## 通道与外设映射

| DMA通道 | 常用外设 | 说明 |
|---------|---------|------|
| DMA1_CH1 | USART1_TX | UART1发送 |
| DMA1_CH2 | USART1_RX | UART1接收 |
| DMA1_CH3 | USART2_TX | UART2发送 |
| DMA1_CH4 | USART2_RX | UART2接收 |
| DMA1_CH5 | USART3_TX | UART3发送 |
| DMA1_CH6 | USART3_RX | UART3接收 |
| DMA1_CH7 | 内存到内存 | 通用内存传输 |
| DMA2_CH1 | ADC1 | ADC转换结果 |
| DMA2_CH2 | SPI1_RX | SPI1接收 |
| DMA2_CH3 | SPI1_TX | SPI1发送 |
| DMA2_CH4 | SPI2_RX | SPI2接收 |
| DMA2_CH5 | SPI2_TX | SPI2发送 |

## ⚠️ 重要说明

1. **内存到内存传输**：只能使用正常模式，不支持循环模式
2. **循环模式**：适用于连续数据采集（如ADC），需要定期读取数据
3. **数据对齐**：使用字传输时，数据大小需要按4字节对齐
4. **中断处理**：DMA传输完成中断需要在中断服务函数中调用`DMA_IsComplete()`清除标志

## 📚 相关模块

- **UART驱动**：`Drivers/uart/uart.c/h` - 可配合DMA实现非阻塞传输
- **ADC驱动**：`Drivers/analog/adc.c/h` - 可配合DMA实现连续采样
- **SPI驱动**：`Drivers/spi/spi_hw.c/h` - 可配合DMA实现高速传输
- **硬件配置**：`BSP/board.h` - DMA硬件配置表
- **模块开关**：`system/config.h` - DMA模块启用/禁用

---

**最后更新**：2024-01-01

