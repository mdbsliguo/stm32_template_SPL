# Bus01 - ModBusRTU气体传感器读取示�?
## 📋 案例目的

- **核心目标**：演示如何使用ModBusRTU协议通过UART2（RS485）读取多个气体传感器数据，并在OLED上显�?- **学习重点**�?  - 理解ModBusRTU协议的基本使用方�?  - 掌握ModBusRTU模块的API调用（读保持寄存器）
  - 学习多传感器轮询读取的实现方�?  - 了解传感器数据解析和显示
  - 学习标准初始化流程（System_Init �?UART �?Debug �?Log �?ErrorHandler �?OLED�?- **应用场景**：适用于工业环境监测、气体检测、多传感器数据采集等应用场景

## 🔧 硬件要求

### 必需外设

- **USART1**�?  - TX：`PA9`
  - RX：`PA10`
  - 波特率：`115200`
  - 数据格式：`8N1`（用于Debug输出�?
- **USART2**�?  - TX：`PA2`
  - RX：`PA3`
  - 波特率：`9600`
  - 数据格式：`8N1`（用于ModBusRTU通信�?
- **RS485模块**�?  - 连接UART2（PA2/PA3�?  - 支持自动方向控制（或手动控制DE/RE引脚�?
- **OLED显示�?*�?  - SCL：`PB8`
  - SDA：`PB9`
  - 接口：软件I2C

- **气体传感�?*�?个）�?  - 地址1：温湿度传感�?  - 地址2：氧气传感器
  - 地址3：一氧化碳传感器
  - 地址4：二氧化碳传感器
  - 地址5：甲烷传感器
  - 地址6：氢气传感器
  - 协议：ModBusRTU，功能码03
  - 注意：支持部分传感器未连接的情况

### 硬件连接

| STM32F103C8T6 | 外设/模块 | 说明 |
|--------------|----------|------|
| PA9 | USB转串口模�?TX | UART1发送引脚（Debug输出�?|
| PA10 | USB转串口模�?RX | UART1接收引脚（Debug输入�?|
| PA2 | RS485模块 TX | UART2发送引脚（ModBusRTU通信�?|
| PA3 | RS485模块 RX | UART2接收引脚（ModBusRTU通信�?|
| PB8 | OLED SCL | 软件I2C时钟�?|
| PB9 | OLED SDA | 软件I2C数据�?|
| 5V | RS485模块 VCC | RS485模块电源 |
| GND | RS485模块 GND | 地线 |
| A+ | 传感器总线 A+ | RS485总线正极 |
| B- | 传感器总线 B- | RS485总线负极 |

**⚠️ 重要提示**�?- 案例是独立工程，硬件配置在案例目录下�?`board.h` �?- 如果硬件引脚不同，直接修�?`Examples/Bus/Bus01_ModBusRTU_GasSensor/board.h` 中的配置即可
- RS485模块需要正确连接到UART2，并确保总线终端电阻配置正确
- 传感器地址必须已正确配置（地址1-3，支持部分传感器未连接）

## 📦 模块依赖

### 模块依赖关系�?
展示本案例使用的模块及其依赖关系�?
```mermaid
%%{init: {'flowchart': {'curve': 'basis'}}}%%
flowchart TB
    %% 应用�?    subgraph APP_LAYER[应用层]
        APP[Bus01案例<br/>main_example.c]
    end
    
    %% 中间件层
    subgraph MW_LAYER[中间件层]
        MODBUS[ModBusRTU<br/>协议栈]
    end
    
    %% 系统服务�?    subgraph SYS_LAYER[系统服务层]
        direction LR
        SYS_INIT[System_Init]
        DELAY[Delay]
        BASE_TIMER[TIM2_TimeBase]
        SYS_INIT --- DELAY
        DELAY --- BASE_TIMER
    end
    
    %% 驱动�?    subgraph DRV_LAYER[驱动层]
        direction LR
        GPIO[GPIO]
        UART[UART]
        OLED[OLED]
        SOFT_I2C[SoftI2C]
        OLED --- SOFT_I2C
    end
    
    %% 调试工具�?    subgraph DEBUG_LAYER[调试工具层]
        direction LR
        DEBUG[Debug]
        LOG[Log]
        ERROR[ErrorHandler]
        ERROR_CODE[ErrorCode]
        DEBUG --- LOG
        LOG --- ERROR
        ERROR --- ERROR_CODE
    end
    
    %% 应用层依�?    APP --> MODBUS
    APP --> SYS_INIT
    APP --> DEBUG
    APP --> LOG
    APP --> ERROR
    APP --> OLED
    APP --> DELAY
    
    %% 中间件层依赖
    MODBUS --> UART
    
    %% 系统服务层依�?    SYS_INIT --> GPIO
    DELAY --> BASE_TIMER
    
    %% 驱动层内部依�?    UART --> GPIO
    SOFT_I2C --> GPIO
    
    %% 调试工具层依�?    DEBUG --> UART
    LOG --> BASE_TIMER
    ERROR --> UART
```

### 模块列表

本案例使用以下模块：

- `modbus_rtu`：ModBusRTU协议栈模块（核心功能�?- `uart`：UART驱动模块（ModBusRTU依赖�?- `oled_ssd1306`：OLED显示模块
- `soft_i2c`：软件I2C模块（OLED依赖�?- `debug`：Debug模块（UART输出功能�?- `log`：日志模块（分级日志输出�?- `error_handler`：错误处理模块（统一错误处理�?- `error_code`：错误码定义模块
- `delay`：延时模�?- `TIM2_TimeBase`：TIM2时间基准模块（delay依赖�?- `gpio`：GPIO驱动模块（UART、I2C依赖�?- `system_init`：系统初始化模块

## 🔄 实现流程

### 整体逻辑

1. **系统初始�?*�?   - System_Init()：系统初始化（GPIO、LED、delay、TIM2_TimeBase�?   - UART_Init()：初始化UART1和UART2
   - Debug_Init()：初始化Debug模块（UART模式�?   - Log_Init()：初始化Log模块
   - ErrorHandler：自动初始化
   - OLED_Init()：初始化OLED显示

2. **主循�?*�?   - 轮询读取3个传感器数据（地址1-3�?   - 每个传感器读取间�?00ms（符合协议要求）
   - 解析传感器数据（温度、湿度、气体浓度等�?   - 根据寄存�?的单位配置自动解析单位和小数位数
   - 在OLED上显示传感器数据�?个传感器直接显示�?   - 通过串口输出日志信息（仅输出失败信息，成功不输出�?   - 所有传感器读取完成后，等待500ms再开始下一轮读�?
### 传感器数据解�?
根据PROTOCOL.md协议文档�?
- **温度�?*：`温度(�? = (寄存�?数�?- 500) / 10`
- **湿度�?*：`湿度(%) = 寄存�?数�?/ 10`
- **气体浓度**：寄存器1数值（根据寄存�?的单位定义）
- **气体类型**：寄存器8�?位（5=CO�?=CO2�?1=CH4等）
- **传感器状�?*：寄存器5�?位（0x00=预热�?x01=正常�?x05=低报等）

### OLED显示内容

- **�?�?*：案例标�?"Bus01 ModBusRTU"
- **�?-4�?*：显�?个传感器数据

显示格式示例�?```
Bus01 ModBusRTU
1:TempHum T:24.6 H:51.0
2:O2 20.9%VOL
3:CO 0ppm
```

**显示说明**�?- 传感�?（温湿度）：显示温度（℃）和湿度�?�?- 传感�?（氧气）：根据单位配置自动显示为%VOL（如20.9%VOL�?- 传感�?（一氧化碳）：根据单位配置自动显示为ppm（如0ppm�?- 单位和小数位数根据寄存器0的配置自动解�?
### 数据流向�?
展示本案例的数据流向：传感器设备 �?ModBusRTU通信 �?数据解析 �?显示输出

```mermaid
graph LR
    %% 传感器设�?    subgraph "传感器层"
        SENSOR1[传感�?<br/>温湿度传感器<br/>地址1]
        SENSOR2[传感�?<br/>氧气传感�?br/>地址2]
        SENSOR3[传感�?<br/>一氧化碳传感器<br/>地址3]
    end
    
    %% RS485总线
    RS485[RS485总线<br/>A+/B-<br/>9600波特率]
    
    %% UART通信
    UART2[UART2<br/>PA2/PA3<br/>ModBusRTU通信]
    
    %% ModBusRTU协议�?    MODBUS[ModBusRTU协议�?br/>功能�?3<br/>读保持寄存器<br/>CRC16校验]
    
    %% 数据解析
    PARSE[数据解析<br/>- 温度/湿度计算<br/>- 单位自动解析<br/>- 小数位处理]
    
    %% 应用逻辑
    APP_LOGIC[应用逻辑<br/>主循环控�?br/>- 轮询读取<br/>- 数据解析<br/>- 错误处理]
    
    %% 输出设备
    OLED_DISP[OLED显示<br/>PB8/PB9<br/>I2C通信<br/>传感器数据]
    UART_OUT[UART输出<br/>PA9/PA10<br/>串口调试<br/>错误日志]
    
    %% 数据�?    SENSOR1 -->|ModBusRTU| RS485
    SENSOR2 -->|ModBusRTU| RS485
    SENSOR3 -->|ModBusRTU| RS485
    RS485 -->|RS485信号| UART2
    UART2 -->|UART数据| MODBUS
    MODBUS -->|寄存器数据| PARSE
    PARSE -->|解析后数据| APP_LOGIC
    APP_LOGIC -->|显示数据| OLED_DISP
    APP_LOGIC -->|错误日志| UART_OUT
    
    %% 样式
    classDef sensor fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
    classDef bus fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    classDef comm fill:#bbdefb,stroke:#1565c0,stroke-width:2px
    classDef protocol fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    classDef process fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    classDef output fill:#ffccbc,stroke:#d84315,stroke-width:2px
    
    class SENSOR1,SENSOR2,SENSOR3 sensor
    class RS485 bus
    class UART2 comm
    class MODBUS protocol
    class PARSE,APP_LOGIC process
    class OLED_DISP,UART_OUT output
```

**数据流说�?*�?
1. **传感器层**�?   - 3个传感器通过RS485总线连接（地址1-3�?   - 每个传感器支持ModBusRTU协议（功能码03�?
2. **通信�?*�?   - RS485总线传输ModBusRTU数据�?   - UART2接收/发送RS485信号�?600波特率）

3. **协议栈层**�?   - ModBusRTU协议栈解析数据帧
   - 验证CRC16校验
   - 提取寄存器数�?
4. **数据解析�?*�?   - 解析温度、湿度、气体浓�?   - 根据寄存�?配置自动解析单位和小数位�?   - 计算实际物理�?
5. **应用逻辑�?*�?   - 主循环轮询读�?个传感器
   - 处理错误和重�?   - 管理显示更新

6. **输出设备�?*�?   - **OLED显示**：实时显示传感器数据
   - **UART输出**：输出错误日志（成功不输出）

## 📚 代码说明

### 初始化流�?
遵循标准初始化流程：

```c
1. System_Init()          // 系统初始�?2. UART_Init()            // UART初始化（UART1和UART2�?3. Debug_Init()           // Debug模块初始�?4. Log_Init()             // Log模块初始�?5. ErrorHandler           // 错误处理模块（自动初始化�?6. OLED_Init()            // OLED初始�?```

### 传感器读�?
使用ModBusRTU模块�?`ModBusRTU_ReadHoldingRegisters()` 函数�?
```c
ModBusRTU_Status_t status;
uint16_t registers[10];

status = ModBusRTU_ReadHoldingRegisters(
    UART_INSTANCE_2,    // UART2实例
    0x01,               // 传感器地址
    0x0000,             // 起始寄存器地址
    10,                 // 寄存器数�?    registers,          // 数据缓冲�?    1000                // 超时时间（毫秒）
);
```

### 数据解析

```c
// 解析温度
float temperature = ((float)registers[7] - 500.0f) / 10.0f;

// 解析湿度
float humidity = (float)registers[9] / 10.0f;

// 获取气体浓度
uint16_t concentration = registers[1];

// 获取气体类型
uint8_t gas_type = (uint8_t)((registers[8] >> 8) & 0xFF);

// 获取传感器状�?uint8_t status = (uint8_t)(registers[5] & 0xFF);
```

## 🚀 使用步骤

### 步骤1：硬件准�?
- STM32F103C8T6开发板（或兼容板）
- USB转串口模块（连接UART1，用于Debug输出�?- RS485模块（连接UART2�?- OLED显示屏（SSD1306，I2C接口�?- 3个气体传感器（地址1-3已配置，支持部分传感器未连接�?- RS485总线连接（A+、B-�?
### 步骤2：打开案例工程

- 双击 `Examples/Bus/Bus01_ModBusRTU_GasSensor/Examples.uvprojx` 打开案例独立工程

### 步骤3：检�?修改硬件配置

- 检查案例目录下�?`board.h` 配置是否正确
- 如果硬件引脚不同，直接修改案例目录下�?`board.h`
- 确认传感器地址是否正确（地址1-3�?
### 步骤4：编译项�?
- 点击工具栏的 **编译按钮**（或�?`F7`�?- 等待编译完成，应该显示：`0 Error(s), 0 Warning(s)`

### 步骤5：下载到开发板

- 点击工具栏的 **下载按钮**（或�?`F8`�?- 等待下载完成，程序会自动运行

### 步骤6：观察效�?
- **OLED显示**：观察OLED上显示的传感器数�?- **串口输出**：打开串口助手�?15200波特率），查看日志输�?- **数据更新**：传感器数据�?00ms更新一�?
## 📊 数据解析说明

### 寄存器映�?
| 寄存器序�?| 地址 | 说明 | 解析公式 |
|-----------|------|------|----------|
| 0 | 0x0000 | 数值单位和小数点位�?| Bit15-12:单位，Bit11-8:小数�?|
| 1 | 0x0001 | 当前气体浓度 | 直接使用 |
| 2 | 0x0002 | 低报警�?| 直接使用 |
| 3 | 0x0003 | 高报警�?| 直接使用 |
| 4 | 0x0004 | 全量程�?| 直接使用 |
| 5 | 0x0005 | 传感器状�?| �?位：状态码 |
| 6 | 0x0006 | 实时AD�?| 直接使用 |
| 7 | 0x0007 | 环境温度 | (数�?500)/10 = 温度(�? |
| 8 | 0x0008 | 气体类型 | �?位：气体类型编号 |
| 9 | 0x0009 | 环境湿度 | 数�?10 = 湿度(%) |

### 状态码说明

| 状态码 | 工作状�?| 说明 |
|--------|----------|------|
| 0x00 | 预热 | 传感器正在预�?|
| 0x01 | 正常 | 传感器工作正�?|
| 0x02 | 数据错误 | 传感器数据异�?|
| 0x03 | 传感器故�?| 传感器硬件故�?|
| 0x04 | 预警 | 浓度达到预警�?|
| 0x05 | 低报 | 浓度达到低报警�?|
| 0x06 | 高报 | 浓度达到高报警�?|
| 0x07 | 访问故障 | 通信故障 |

### 气体类型说明

常见气体类型�?0进制）：
- 0：NULL
- 5：CO（一氧化碳）
- 6：CO2（二氧化碳）
- 11：CH4（甲烷）
- 52：H2S（硫化氢�?- 63：NH3（氨气）
- 69：PH3（磷化氢�?- 72：SO2（二氧化硫）

## ⚠️ 注意事项

1. **RS485方向控制**�?   - 如果使用自动方向控制的RS485模块，无需额外配置
   - 如果需要手动控制DE/RE引脚，需要添加GPIO控制代码

2. **轮询间隔**�?   - PROTOCOL.md建议每个地址问询间隔500ms以上
   - 本案例在每个传感器读取后等待500ms，符合协议要�?   - 所有传感器读取完成后，等待500ms再开始下一轮读�?
3. **重试机制**�?   - ModBusRTU模块已实现自动重试（默认3次）
   - 如果通信失败，会自动重试，提高可靠�?
4. **数据格式**�?   - 16位数据高字节在前，符合ModBusRTU标准
   - CRC校验码低位在前，高位在后

5. **传感器地址**�?   - 当前配置支持3个传感器（地址1-3�?   - 如果地址不同，需要修改代码中的传感器配置
   - 支持部分传感器未连接的情况（会显�?Error"�?   - 传感�?-6的配置已预留（代码中已注释），需要时可取消注�?
6. **数据单位自动解析**�?   - 代码会根据寄存器0的配置自动解析单位和小数位数
   - 氧气传感器通常使用%VOL单位，需要除�?0得到百分�?   - 一氧化碳传感器通常使用ppm单位，直接显示原始�?   - 显示时会自动使用正确的单位（%VOL、ppm等）

7. **日志输出**�?   - 读取成功时不输出日志（减少串口输出）
   - 读取失败时才输出错误日志，便于排查问�?
8. **标准初始�?*�?   - 必须遵循 System_Init �?UART �?Debug �?Log �?ErrorHandler �?OLED 的顺�?   - 这是项目规范要求的标准初始化流程

## 🔍 故障排查

### 问题1：传感器读取失败

**现象**：OLED显示 "Error"，串口输出读取失败错�?
**排查步骤**�?1. 检查RS485模块连接是否正确
2. 检查传感器地址是否正确（地址1-3�?3. 检查传感器是否上电
4. 检查RS485总线终端电阻配置
5. 使用串口助手查看UART2的通信数据

### 问题2：OLED无显�?
**现象**：OLED屏幕无显�?
**排查步骤**�?1. 检查OLED连接是否正确（PB8/PB9�?2. 检查OLED是否上电
3. 检查软件I2C配置是否正确
4. 查看串口日志，确认OLED初始化是否成�?
### 问题3：数据解析错�?
**现象**：显示的数据明显不正�?
**排查步骤**�?1. 检查寄存器数据是否正确读取
2. 检查数据解析公式是否正�?3. 查看串口日志，确认原始寄存器数据
4. 参考PROTOCOL.md确认数据格式

## 🧪 安全测试方法（在家可操作�?
本部分提供了在家中可以安全操作的传感器测试方法，用于验证传感器功能是否正常�?
### 🌡�?温度传感器测�?
**升温测试**�?- 用手掌捂住传感器30秒，观察温度是否上升
- 靠近热水杯（注意不要直接接触，避免水汽）
- 使用电吹风低档，保持适当距离（避免过热损坏）

**降温测试**�?- 放在冰箱门口吹冷气（不要放入冰箱内部，避免结露）
- 用塑料袋包点冰块靠近传感器（注意防水�?
**预期效果**�?- 温度值应该明显变化（上升或下降）
- 正常室温�?0-30℃，变化范围应在合理区间

### 💧 湿度传感器测�?
**增湿测试**�?- 对着传感器哈气（保持适当距离，避免水汽直接接触）
- 在旁边放一杯热水（利用水蒸气）
- 使用加湿器喷雾（保持适当距离�?
**降湿测试**�?- 放在空调或除湿机出风�?- 用硅胶干燥剂靠近传感�?
**预期效果**�?- 湿度值应该明显变化（上升或下降）
- 正常室内湿度�?0-70%，变化范围应在合理区�?
### 🫧 氧气传感器测�?
**降低浓度测试**�?- 用塑料袋罩住传感器，里面放少量燃烧的香或火柴（注意安全）
- 通过燃烧消耗氧气，观察氧气浓度是否下降
- **⚠️ 注意**：保持通风，避免烟雾过�?
**正常情况测试**�?- 开窗通风，观察氧气浓度是否基本无变化（约20.9%VOL�?- 在正常室内环境中，氧气浓度应该稳定在20.9%VOL左右

**预期效果**�?- 正常空气中氧气浓度约20.9%VOL�?090/10�?- 在密闭空间或燃烧后，氧气浓度会下�?- 开窗通风后应恢复到正常�?
### ⚠️ 一氧化碳传感器测试（务必小心）

**⚠️ 重要安全提示**�?- **永远不要在密闭空间人为制造CO�?*
- 测试后立即通风
- 如有不适，立即离开现场并就�?
**产生微量CO测试方法**�?- 点燃一根香放在旁边（保持通风，不要直接接触传感器�?- 燃气灶小火不点火几秒钟（**不推�?*，风险较高）

**预期效果**�?- 正常情况下CO浓度应为0ppm
- 在产生微量CO后，浓度值会上升
- 通风后应恢复�?ppm

**安全建议**�?- 优先使用香等低风险方�?- 避免使用燃气灶等高风险方�?- 测试时保持房间通风
- 测试后立即开窗通风

## 📖 扩展练习

1. **添加更多传感�?*�?   - 扩展代码支持更多传感器（地址4�?�?等）
   - 实现动态传感器配置

2. **数据存储**�?   - 添加数据存储功能（如Flash存储�?   - 实现历史数据记录

3. **报警功能**�?   - 根据传感器状态实现报警功�?   - 添加LED或蜂鸣器报警指示
   - 实现阈值报警（温度、湿度、气体浓度超限报警）

4. **数据上传**�?   - 实现数据上传到上位机
   - 添加数据包格式定�?   - 实现数据记录和统计功�?
5. **显示优化**�?   - 实现分页显示（显示更多信息）
   - 添加数据曲线显示
   - 实现单位自动转换和显�?
6. **传感器校�?*�?   - 实现传感器校准功�?   - 添加零点校准和满量程校准
   - 实现校准数据存储

## 📝 参考资�?
- **PROTOCOL.md**：传感器协议文档（案例目录下�?- **Official_Example**：官方案例实现（参考数据解析逻辑�?- **ModBusRTU模块**：`Middlewares/protocols/modbus_rtu/`
- **UART模块**：`Drivers/uart/`
- **OLED模块**：`Drivers/display/oled_ssd1306/`

## 🔗 相关案例

- **UART01_Error_Log_Output**：UART错误日志输出示例
- **OLED01_helloword**：OLED显示测试

---

**最后更�?*�?024-01-01
