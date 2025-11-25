# CAN总线模块

## 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `CAN_Init()` | 初始化CAN外设和GPIO（CAN1/CAN2） |
| 反初始化 | `CAN_Deinit()` | 反初始化CAN外设 |
| 发送消息 | `CAN_Transmit()` | 阻塞式发送CAN消息（标准帧/扩展帧） |
| 接收消息 | `CAN_Receive()` | 阻塞式接收CAN消息 |
| 检查待接收消息 | `CAN_GetPendingMessageCount()` | 获取FIFO中待接收消息数量 |
| 配置过滤器 | `CAN_ConfigFilter()` | 配置CAN消息过滤器 |
| 状态检查 | `CAN_IsInitialized()` | 检查是否已初始化 |
| 获取外设 | `CAN_GetPeriph()` | 获取CAN外设指针 |

## 重要提醒

1. **实例选择**：支持CAN1、CAN2两个实例，使用`CAN_INSTANCE_1`或`CAN_INSTANCE_2`
2. **配置驱动**：GPIO和CAN参数在`BSP/board.h`中配置，自动初始化
3. **帧类型**：支持标准帧（11位ID）和扩展帧（29位ID）
4. **过滤器**：支持14个过滤器（CAN1）或28个过滤器（CAN2），可配置ID和掩码
5. **FIFO**：CAN有两个接收FIFO（FIFO0和FIFO1），每个过滤器可指定FIFO
6. **CAN2特殊处理**：CAN2需要调用`CAN_SlaveStartBank()`设置过滤器起始bank

## 配置说明

在`BSP/board.h`中配置CAN：

```c
#define CAN_CONFIGS {                                                                                    \
    {CAN1, GPIOA, GPIO_Pin_12, GPIOA, GPIO_Pin_11, 9, CAN_Mode_Normal, CAN_SJW_1tq, CAN_BS1_4tq, CAN_BS2_3tq, 1}, /* CAN1：PA12(TX), PA11(RX)，500kHz，正常模式，启用 */ \
}
```

## 使用示例

```c
/* 初始化CAN1 */
CAN_Init(CAN_INSTANCE_1);

/* 发送标准帧消息 */
CAN_Message_t tx_msg;
tx_msg.id = 0x123;
tx_msg.type = CAN_FRAME_STANDARD;
tx_msg.rtr = CAN_RTR_DATA;
tx_msg.dlc = 8;
tx_msg.data[0] = 0x01;
tx_msg.data[1] = 0x02;
/* ... */
CAN_Transmit(CAN_INSTANCE_1, &tx_msg, 0);

/* 接收消息 */
CAN_Message_t rx_msg;
CAN_Receive(CAN_INSTANCE_1, &rx_msg, 0, 1000);

/* 配置过滤器（接收ID 0x123的标准帧） */
CAN_ConfigFilter(CAN_INSTANCE_1, 0, 0x123, 0x7FF, CAN_FRAME_STANDARD, 0);
```
