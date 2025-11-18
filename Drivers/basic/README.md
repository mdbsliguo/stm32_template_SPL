# 基础外设模块

## GPIO模块

### 功能列表

| 功能 | 函数/宏 | 说明 |
|------|---------|------|
| GPIO配置 | `GPIO_Config()` | 配置GPIO模式、速度，自动使能时钟 |
| 时钟使能 | `GPIO_EnableClock()` | 手动使能GPIO端口时钟 |
| 写入电平 | `GPIO_WritePin()` | 设置引脚电平（高/低） |
| 读取电平 | `GPIO_ReadPin()` | 读取引脚当前电平 |
| 翻转电平 | `GPIO_Toggle()` | 翻转引脚电平状态 |
| 快捷宏 | `GPIO_Init_Output()` | 快速配置为推挽输出 |
| 快捷宏 | `GPIO_SetPin()` / `GPIO_ResetPin()` | 快速设置/复位引脚 |

### 重要提醒

1. **自动时钟管理**：`GPIO_Config()`会自动使能对应GPIO端口的时钟，无需手动调用`GPIO_EnableClock()`
2. **模式支持**：支持输入（浮空/上拉/下拉/模拟）、输出（推挽/开漏）、复用（推挽/开漏）等全模式
3. **错误处理**：所有函数返回`GPIO_Status_t`错误码，需检查返回值
4. **向后兼容**：保留了`GPIO_Init_Output()`、`GPIO_SetPin()`等宏定义，兼容旧代码

---

## LED模块

### 功能列表

| 功能 | 函数 | 说明 |
|------|------|------|
| 初始化 | `LED_Init()` | 根据配置表初始化所有启用的LED |
| 反初始化 | `LED_Deinit()` | 关闭所有LED并释放资源 |
| 设置状态 | `LED_SetState()` | 设置LED状态（ON/OFF） |
| 获取状态 | `LED_GetState()` | 获取LED当前状态 |
| 点亮 | `LED_On()` | 点亮指定LED |
| 熄灭 | `LED_Off()` | 熄灭指定LED |
| 翻转 | `LED_Toggle()` | 翻转LED状态 |
| 闪烁 | `LED_Blink()` | LED闪烁一次（阻塞延时） |
| 快捷宏 | `LED1_On()` / `LED2_On()` | LED1/LED2快捷操作 |

### 重要提醒

1. **配置表驱动**：LED配置在`BSP/board.h`的`LED_CONFIGS`中定义，支持多LED管理
2. **有效电平处理**：模块自动处理有效电平（高电平点亮/低电平点亮），无需关心硬件连接
3. **运行时控制**：支持运行时使能/禁用LED，只初始化`enabled=1`的LED
4. **阻塞式闪烁**：`LED_Blink()`使用阻塞延时，执行期间CPU不处理其他任务
5. **错误处理**：所有函数返回`LED_Status_t`错误码，需检查返回值

