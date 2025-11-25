# STM32快速开发工程模板（SPL库）

**基于STM32标准外设库（SPL）的快速开发工程模板，遵循AUTOSAR分层思想与防御性编程原则。**

---

## 📖 项目简介

STM32F103C8T6快速开发工程模板，提供模块化架构、硬件抽象、统一错误处理等核心特性，帮助开发者快速构建STM32应用。

### 核心特性

- ✅ **模块化架构**：清晰的分层设计，职责分离
- ✅ **硬件隔离**：BSP层实现硬件抽象，跨型号移植便捷
- ✅ **统一错误处理**：错误码系统、错误处理框架、错误统计
- ✅ **配置驱动**：通过配置表而非硬编码，运行时可配置
- ✅ **防御性编程**：参数校验、边界检查、空指针保护
- ✅ **完整文档**：AI规则、案例参考

### 快速链接

- 📗 [AI规则与项目约束](PROJECT_KEYWORDS.md) - 开发规范和约束
- 📕 [案例参考](Examples/README.md) - 12个演示案例

---

## 🚀 快速开始

### 1. 硬件配置

编辑 `BSP/board.h`，配置硬件引脚：

```c
/* LED配置 */
#define LED_CONFIGS { \
    {GPIOA, GPIO_Pin_1, Bit_RESET, 1}, \
}
```

### 2. 系统初始化

在 `Core/main.c` 中初始化系统：

```c
#include "system_init.h"
#include "led.h"

int main(void)
{
    System_Init();  // 自动初始化GPIO、LED、delay、TIM2_TimeBase
    
    while(1) {
        LED1_On();
        Delay_ms(500);
        LED1_Off();
        Delay_ms(500);
    }
}
```

### 3. 使用模块

```c
#include "led.h"
#include "gpio.h"

LED1_On();  // LED控制
GPIO_Config(GPIOA, GPIO_Pin_5, GPIO_MODE_OUTPUT_PP, GPIO_Speed_50MHz);  // GPIO操作
```

**详细步骤请参考案例代码** [Examples/](../Examples/)

---

## ⭐ 核心特性

### 驱动模块

- **GPIO**：全模式GPIO抽象，动态时钟使能
- **LED**：配置表驱动LED控制
- **OLED_SSD1306**：SSD1306驱动，支持软硬I2C，字符/字符串/数字显示
- **硬件I2C/软件I2C**：I2C1/I2C2主模式，7位/10位地址，总线扫描
- **硬件SPI/软件SPI**：支持多种SPI模式，8位/16位数据格式
- **DS3231**：实时时钟驱动，支持软硬I2C可选

### 系统服务

- **TIM2_TimeBase**：TIM2时间基准，1ms中断，动态调频自适应
- **delay**：阻塞式+非阻塞式延时，频率自适应
- **clock_manager**：时钟管理，DVFS自动调频，9档位（8MHz~72MHz）
- **system_init**：统一初始化框架
- **iwdg**：独立看门狗管理
- **system_monitor**：系统监控，CPU/内存监控、健康检查

### 工具模块

- **error_handler**：统一错误处理框架
- **log**：分级日志系统（DEBUG/INFO/WARN/ERROR）

> **详细配置说明请参考各模块README和案例代码**

---

## 📁 项目结构

```text
stm32_template_SPL/
├── BSP/              # 硬件抽象层（board.h硬件配置隔离）
├── Core/             # 应用层（main.c，中断处理）
├── Drivers/          # 驱动层（按类型分类：basic/i2c/spi/display/sensors等）
├── System/           # 系统服务层（delay、clock_manager等）
├── Drivers/timer/    # 定时器驱动（TIM2_TimeBase、PWM、输入捕获等）
├── Debug/            # 调试工具层（debug、log、assert）
├── Common/           # 公共模块（error_code、error_handler）
├── Middlewares/      # 中间件层（协议栈、算法）
├── Examples/         # 演示案例（12个独立工程案例）
├── Library/          # STM32标准外设库（SPL）
└── Start/            # 启动文件
```

**各目录详细说明：**

- [BSP/README.md](BSP/README.md) - 硬件抽象层说明
- [Drivers/README.md](Drivers/README.md) - 驱动层说明
- [System/README.md](System/README.md) - 系统服务层说明
- [Debug/README.md](Debug/README.md) - 调试工具层说明

---

## 📚 文档导航

### 开发规范类

- [AI规则与项目约束](PROJECT_KEYWORDS.md) - **AI的唯一规则来源**（包含项目描述和开发约束，新建会话时请复制使用）


### 其他文档

- [OLED模块I2C解耦设计](Docs/OLED_I2C_DECOUPLING.md) - OLED模块I2C解耦的完整设计文档（可作为其他模块解耦的参考）

---

## 🔧 编译环境

### 环境要求

- **IDE**：Keil MDK v5
- **编译器**：ARMCC v5.06
- **目标芯片**：STM32F103C8T6
- **库版本**：STM32F10x_StdPeriph_Lib V3.5.0

### 编译步骤

1. 打开 `Project.uvprojx`
2. 点击编译（F7）
3. 编译输出在 `Build/Keil/Objects/`

**编译环境要求**：Keil MDK v5，ARMCC v5.06，STM32F103C8T6

---

## 📝 代码规范

项目遵循统一的代码规范，包括命名规范、错误处理、防御性编程等。

**详细规范请参考** [AI规则与项目约束](PROJECT_KEYWORDS.md)

---

## ⚠️ 重要说明

- **OLED默认显示**：除案例1外，所有案例默认使用OLED作为显示输出设备，详细说明见 [Examples/README.md](Examples/README.md)
- **独立工程**：每个案例和测试工程都是独立的Keil工程，可直接编译运行
- **代码规范**：所有模块函数返回`error_code_t`，详细规范见 [PROJECT_KEYWORDS.md](PROJECT_KEYWORDS.md)

---


## 📄 许可证

MIT License - 随意使用，自由修改


---

**最后更新**：2024-01-01（v0.1.0 - 初始化 STM32 模块化基础结构）
