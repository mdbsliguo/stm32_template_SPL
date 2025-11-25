# 模拟外设模块 - Analog

本分类用于存放模拟外设驱动模块，目前已实现ADC功能。

---

## 📋 模块功能清单

| 功能 | 说明 | 状态 |
|------|------|------|
| ADC初始化 | 配置ADC1，GPIO引脚，采样时间 | ✅ 已实现 |
| ADC单次转换 | 单通道单次转换，阻塞式读取 | ✅ 已实现 |
| ADC多通道扫描 | 多通道顺序扫描转换 | ✅ 已实现 |
| ADC连续转换 | 单通道连续转换模式 | ✅ 已实现 |
| DAC初始化 | 配置DAC1/2，GPIO引脚 | ✅ 已实现（仅HD/CL/HD_VL/MD_VL型号支持） |
| DAC输出 | 单通道/双通道电压输出 | ✅ 已实现（仅HD/CL/HD_VL/MD_VL型号支持） |

---

## 🎯 职责

1. **SPL库封装**：封装STM32 ADC/DAC标准外设库函数，提供统一的模拟接口
2. **GPIO配置**：自动配置ADC/DAC引脚为模拟输入/输出
3. **时钟管理**：自动使能ADC/DAC外设和GPIO时钟
4. **错误处理**：参数校验，错误码返回，超时处理

---

## 🔧 ADC使用方法

### 1. 配置模块开关

在 `System/config.h` 中启用ADC模块：
```c
#define CONFIG_MODULE_ADC_ENABLED 1
```

### 2. 配置硬件引脚

在 `BSP/board.h` 中配置ADC实例：
```c
#define ADC_CONFIGS {                                                                                    \
    {ADC1, {ADC_Channel_0, ADC_Channel_1}, 2, ADC_SampleTime_55Cycles5, 1}, /* ADC1：PA0, PA1，双通道，启用 */ \
}
```

### 3. 初始化ADC

```c
#include "adc.h"

ADC_Status_t status = ADC_ModuleInit(ADC_INSTANCE_1);
if (status != ADC_OK) {
    // 处理错误
}
```

### 4. 读取ADC值

```c
/* 单次转换 */
uint16_t value;
ADC_ReadChannel(ADC_INSTANCE_1, ADC_Channel_0, &value, 0);

/* 多通道扫描 */
uint8_t channels[] = {ADC_Channel_0, ADC_Channel_1};
uint16_t values[2];
ADC_ReadChannels(ADC_INSTANCE_1, channels, 2, values, 0);

/* 连续转换 */
ADC_StartContinuous(ADC_INSTANCE_1, ADC_Channel_0);
while (1) {
    uint16_t value;
    ADC_ReadContinuous(ADC_INSTANCE_1, &value);
    Delay_ms(100);
}
ADC_StopContinuous(ADC_INSTANCE_1);
```

---

## ⚠️ 重要说明

1. **通道映射**：ADC通道对应GPIO引脚（如ADC_Channel_0对应PA0）
2. **采样时间**：采样时间越长，精度越高，但转换时间也越长
3. **连续转换**：连续转换模式下，ADC会自动重复转换，需要定期读取结果
4. **电压计算**：ADC值为12位（0-4095），对应0V到参考电压（通常3.3V）

---

## 📚 相关模块

- **GPIO驱动**：`Drivers/basic/gpio.c/h` - 用于配置ADC引脚
- **延时模块**：`System/delay.c/h` - 用于超时处理
- **错误处理**：`Common/error_handler.c/h` - 统一错误日志
- **硬件配置**：`BSP/board.h` - ADC硬件配置表
- **模块开关**：`System/config.h` - ADC模块启用/禁用

---

**最后更新**：2024-01-01
