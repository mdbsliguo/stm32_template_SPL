# EXTI模块

外部中断模块，提供外部中断配置、中断回调等功能。

## 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `EXTI_Init()` | 初始化EXTI线 |
| 反初始化 | `EXTI_Deinit()` | 反初始化EXTI线 |
| 设置回调 | `EXTI_SetCallback()` | 设置中断回调函数 |
| 使能中断 | `EXTI_Enable()` | 使能EXTI中断 |
| 禁用中断 | `EXTI_Disable()` | 禁用EXTI中断 |
| 清除挂起 | `EXTI_ClearPending()` | 清除EXTI挂起标志 |
| 获取挂起状态 | `EXTI_GetPendingStatus()` | 获取EXTI挂起标志状态 |
| 生成软件中断 | `EXTI_GenerateSWInterrupt()` | 生成软件中断 |
| 检查初始化 | `EXTI_IsInitialized()` | 检查EXTI是否已初始化 |
| 中断处理 | `EXTI_IRQHandler()` | EXTI中断处理函数（在ISR中调用） |

## 重要提醒

1. **EXTI线选择**：
   - EXTI线0-15：需要配置GPIO端口和引脚
   - EXTI线16-19：特殊功能（PVD、RTC Alarm、USB Wakeup、Ethernet Wakeup），不需要GPIO配置

2. **触发模式**：
   - `EXTI_TRIGGER_RISING`：上升沿触发
   - `EXTI_TRIGGER_FALLING`：下降沿触发
   - `EXTI_TRIGGER_RISING_FALLING`：上升沿和下降沿触发

3. **模式选择**：
   - `EXTI_MODE_INTERRUPT`：中断模式（会触发中断服务程序）
   - `EXTI_MODE_EVENT`：事件模式（不会触发中断，用于唤醒CPU）

4. **中断向量**：
   - EXTI0-4：独立中断向量（EXTI0_IRQn ~ EXTI4_IRQn）
   - EXTI5-9：共享中断向量（EXTI9_5_IRQn）
   - EXTI10-15：共享中断向量（EXTI15_10_IRQn）
   - EXTI16-19：特殊功能中断向量

5. **配置驱动**：EXTI参数在`BSP/board.h`中配置

6. **中断回调**：支持设置中断回调函数，在中断服务程序中自动调用

## 使用示例

```c
#include "exti.h"
#include "gpio.h"

/* EXTI中断回调函数 */
void exti0_callback(EXTI_Line_t line, void *user_data)
{
    /* 处理EXTI0中断 */
}

int main(void)
{
    /* 初始化EXTI0，PA0，上升沿触发，中断模式 */
    EXTI_Init(EXTI_LINE_0, EXTI_TRIGGER_RISING, EXTI_MODE_INTERRUPT);
    
    /* 设置中断回调函数 */
    EXTI_SetCallback(EXTI_LINE_0, exti0_callback, NULL);
    
    /* 使能EXTI中断 */
    EXTI_Enable(EXTI_LINE_0);
    
    while (1)
    {
        /* 主循环 */
    }
}

/* EXTI0中断服务程序 */
void EXTI0_IRQHandler(void)
{
    EXTI_IRQHandler(EXTI_LINE_0);
}
```

## 配置说明

在`BSP/board.h`中配置EXTI：

```c
#define EXTI_CONFIGS {                                                                                    \
    {EXTI_LINE_0, GPIOA, GPIO_Pin_0, EXTI_TRIGGER_RISING, EXTI_MODE_INTERRUPT, 1}, /* EXTI0：PA0，上升沿，中断模式，启用 */ \
}
```

## 🔗 相关模块

- **GPIO驱动**：`Drivers/basic/gpio.c/h` - GPIO配置
- **错误处理**：`common/error_handler.c/h` - 统一错误处理
- **硬件配置**：`BSP/board.h` - EXTI配置

---

**最后更新**：2024-01-01

