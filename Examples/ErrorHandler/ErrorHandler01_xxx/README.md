# 案例6 - 错误处理模块测试

## 📋 概述

案例6用于测试 `error_handler` 错误处理模块的完整功能，展示错误处理的完整流程�?
## 🎯 功能说明

### 测试内容

案例6包含6个测试阶段，每个阶段持续5秒，循环执行�?
1. **测试阶段1：OLED模块错误处理**
   - 测试无效行号（行�?4�?   - 测试无效列号（列�?16�?   - 测试空指针参�?
2. **测试阶段2：LED模块错误处理**
   - 测试无效LED ID（LED_3不存在）
   - 测试无效LED ID（LED_0不存在）
   - 测试空指针参数（LED_GetState�?
3. **测试阶段3：clock_manager模块错误处理**
   - 测试未初始化时的错误（如果可能）
   - 测试模式冲突（在自动模式下设置固定频率）

4. **测试阶段4：错误回调功�?*
   - 注册错误回调函数
   - 触发错误，观察回调是否被调用（LED1闪烁�?   - 取消注册回调，再次触发错误，观察回调是否不再被调�?
5. **测试阶段5：错误码转换功能**
   - 测试OLED错误码到字符串转�?   - 测试LED错误码到字符串转�?   - 测试CLKM错误码到字符串转�?
6. **测试阶段6：ErrorHandler_Check功能**
   - 测试成功情况（无错误�?   - 测试错误情况（有错误�?
### 显示输出

- **OLED显示**�?  - �?行：当前测试阶段和测试项
  - �?-3行：错误信息（模块名称、错误码、错误描述）

- **LED显示**�?  - **LED1**：错误发生时闪烁提示（通过错误回调函数�?
## 🔧 硬件要求

- **LED1**：连接到PA1（错误提示）
- **OLED显示�?*（SSD1306，I2C接口�?  - SCL连接到PB8
  - SDA连接到PB9

## 📦 模块依赖

- `error_handler`：统一错误处理模块
- `error_code`：错误码定义
- `OLED`：SSD1306软件I2C驱动
- `LED`：LED驱动模块
- `clock_manager`：时钟管理模�?- `delay`：延时模�?- `TIM2_TimeBase`：TIM2时间基准模块
- `system_init`：系统初始化模块

## ⚙️ 配置要求

### 系统配置（system/config.h�?
案例6需要以下配置：

```c
#define CONFIG_MODULE_ERROR_HANDLER_ENABLED 1   /* 错误处理模块必须启用 */
#define CONFIG_ERROR_HANDLER_STATS_EN       1   /* 错误统计功能（可选） */
```

### 回调函数使用限制

⚠️ **重要**：错误回调函数必须遵守以下限制：

- �?**允许**：简单的LED闪烁、状态更新等快速操�?- �?**禁止**：调�?`ErrorHandler_Handle()`（会导致嵌套死锁�?- �?**禁止**：执行阻塞操作（影响实时性）
- �?**禁止**：在ISR中调�?`ErrorHandler_Handle()`（除非标记为ISR-safe�?
**示例**�?
```c
// �?正确：简单的LED闪烁
static void ErrorCallback(error_code_t error_code, const char* module_name)
{
    LED1_On();
    Delay_ms(100);
    LED1_Off();
}

// �?错误：嵌套调用会导致死锁
static void ErrorCallback(error_code_t error_code, const char* module_name)
{
    ErrorHandler_Handle(error_code, module_name);  // 禁止�?}
```

## 🚀 使用方法

### 步骤1：配置硬�?
根据实际硬件连接，修�?`board.h` 中的配置�?
```c
/* LED配置 */
#define LED_CONFIGS { \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1},  /* LED1：PA1，低电平点亮，启�?*/ \
}

/* OLED I2C配置 */
#define OLED_I2C_CONFIG { \
    GPIOB, GPIO_Pin_8,  /* SCL: PB8 */ \
    GPIOB, GPIO_Pin_9,  /* SDA: PB9 */ \
}
```

### 步骤2：复制案例代�?
将案例代码复制到项目主文件：

```powershell
# 复制main_example.c到Core/main.c
Copy-Item Examples\example6_error_handler\main_example.c Core\main.c -Force

# 复制board.h到BSP/board.h（需要手动合并配置）
# 注意：不要直接替换，需要手动合并到案例目录下的 board.h�?```

### 步骤3：编译运�?
1. 在Keil中编译项目（F7�?2. 下载到开发板（F8�?3. 观察OLED显示和LED闪烁

## 📊 预期效果

### OLED显示

- **欢迎界面**�?秒）�?
```text
Error Handler
Test Case 6
Press Reset
to Start
```

- **测试阶段0**�?秒）�?
```text
Error Handler
Test Case 6
6 Test Phases
~7s per phase
```

- **测试阶段1**（约7秒）�?
```text
Test1.1: Invalid
OLED:-102
OLED: Invalid param
```

- **测试阶段2**（约7秒）�?
```text
Test2.1: Invalid
LED:-401
LED: Invalid ID
```

- **测试阶段3**（约7秒）�?
```text
Test3.2: Conflict
CLKM:-515
CLKM: Mode conflict
```

- **测试阶段4**（约7秒）�?
```text
Test4.1: Trigger
OLED:-102
OLED: Invalid param
```

（LED1应该闪烁，表示错误回调被调用�?
- **测试阶段5**（约7秒）�?
```text
OLED Errors:
OLED: Not init
```

- **测试阶段6**（约7秒）�?
```text
Test6.1: OK
No Error
```

### LED显示

- **LED1**：错误发生时闪烁（通过错误回调函数�?
## 🔍 错误处理流程

案例6展示了完整的错误处理流程�?
1. **错误检�?*：模块函数返回错误码
2. **错误处理**：调�?`ErrorHandler_Handle()` �?`ErrorHandler_Check()`
3. **错误记录**：自动记录错误日志（如果日志系统可用�?4. **错误回调**：调用注册的错误回调函数（如果已注册�?5. **错误显示**：在OLED上显示错误信�?
## 📝 代码示例

### 错误处理示例

```c
// 方式1：使用ErrorHandler_Check（推荐）
OLED_Status_t status = OLED_ShowString(5, 1, "Test");  /* 无效行号 */
if (ErrorHandler_Check(status, "OLED")) {
    // 错误已记录和处理
    return status;
}

// 方式2：手动错误处理（详细控制�?OLED_Status_t status = OLED_ShowString(5, 1, "Test");
if (status != OLED_OK) {
    const char* err_str = ErrorHandler_GetString(status);
    ErrorHandler_Handle(status, "OLED");
    // 根据错误类型进行不同处理
}

// 方式3：注册错误回调（注意：回调中禁止嵌套调用ErrorHandler_Handle�?void MyErrorCallback(error_code_t code, const char* module) {
    // �?正确：简单的LED闪烁
    LED1_On();
    Delay_ms(100);
    LED1_Off();
    
    // �?错误：嵌套调用会导致死锁
    // ErrorHandler_Handle(code, module);
}

ErrorHandler_RegisterCallback(MyErrorCallback);
```

## ⚠️ 注意事项

1. **board.h配置**�?   - 案例6只需要LED1，不需要LED2
   - 确保OLED I2C引脚配置正确

2. **错误回调**�?   - 错误回调函数会在错误发生时立即调�?   - 回调函数中不要执行耗时操作
   - ⚠️ **禁止**在回调中调用 `ErrorHandler_Handle()`（会导致嵌套死锁�?   - ⚠️ **禁止**在回调中执行阻塞操作（影响实时性）

3. **测试循环**�?   - 6个测试阶段循环执行，每个阶段5�?   - 可以通过复位重新开始测�?
## 🔗 相关文档

- **错误处理模块**：`common/README.md`
- **错误码定�?*：`common/error_code.h`
- **错误处理接口**：`common/error_handler.h`

---

**最后更�?*�?024-01-01（案�?更新：添加配置管理和回调限制说明�?