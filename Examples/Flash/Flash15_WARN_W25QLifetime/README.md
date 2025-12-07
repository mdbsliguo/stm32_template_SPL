# Flash15 - W25Q寿命测试案例

## 📋 案例目的

- **核心目标**：演示W25Q系列Flash芯片的破坏性寿命极限测试流程，通过持续P/E（Program/Erase）循环将芯片测到报废，获取完整的寿命退化曲线和P/E循环极限值

### 核心功能

1. **基准数据记录**（0次循环）
   - 全片擦除时间测量（作为性能基准）
   - 编程时间统计（100页采样）
   - 读取速度测试
   - 初始错误率检测

2. **破坏性P/E循环测试**（持续到报废）
   - 全片擦除（每次循环）
   - 全片写入固定值数据（每轮循环值+1，便于错误检测）
   - 全片读取并逐字节校验
   - 实时错误统计（擦除错误、编程错误、校验错误）
   - 坏块统计与标记

3. **定期深度健康检查**（每1000次循环）
   - 擦除时间退化分析
   - 编程速度与误码率统计
   - 读干扰敏感性测试
   - 坏块分布热图生成

4. **报废判定标准**（自动终止测试）
   - 硬失效标准：连续擦除失败、连续编程失败、坏块率>5%、误码率>1e-3、擦除时间超过阈值（根据芯片容量动态计算：Block数量 × 200ms/块）
   - 软失效标准：寿命评分<20、性能退化>50%、误码率持续上升

5. **数据记录与分析**
   - 实时数据记录（每100次循环）
   - 累计统计（总循环数、总写入数据量、平均时间、错误率）
   - 性能退化率计算
   - 寿命评分计算（0-100分）

### 学习重点

- 理解Flash芯片寿命测试的完整流程
- 掌握P/E循环测试的实现方法
- 学习Flash性能退化监测技术
- 了解Flash芯片报废判定标准
- 掌握破坏性测试的数据分析方法

### 应用场景

适用于需要评估W25Q Flash芯片寿命的场景，如：
- 芯片寿命摸底测试
- 批次质量一致性验证
- 可靠性工程验证
- 寿命预测模型建立
- 产品设计验证

## ⚠️ 重要警告

**本测试为破坏性测试，会将芯片测到报废，请勿在正常使用的芯片上运行！**

- 测试芯片将被物理损坏，无法恢复
- 建议使用专用测试芯片或报废芯片
- 测试过程可能需要数天至数周（取决于芯片容量和P/E循环次数）
- 测试过程中请勿断电，避免数据丢失

## 🔧 硬件要求

### 必需外设

- **LED1**：连接到 `PA1`（系统状态指示）

### 传感器/模块

#### W25Q SPI Flash模块

| 引脚 | STM32连接 | 说明 |
|------|-----------|------|
| CS | PA11 | 片选信号（软件NSS模式） |
| SCK | PB13 | SPI2时钟信号 |
| MISO | PB14 | SPI2主入从出（数据接收） |
| MOSI | PB15 | SPI2主出从入（数据发送） |
| VCC | 3.3V | **⚠️ 重要：必须使用3.3V，不能使用5V！** |
| GND | GND | 电源地 |

**⚠️ 重要提示**：
- W25Q系列Flash使用3.3V供电，使用5V会损坏芯片
- CS引脚使用软件NSS模式，由软件控制拉低/拉高
- **测试过程中必须保持电源稳定，避免断电导致数据丢失**
- **建议使用专用测试芯片，不要在生产芯片上运行**

#### OLED显示屏（软件I2C接口）

| 引脚 | STM32连接 | 说明 |
|------|-----------|------|
| SCL | PB8 | 软件I2C时钟线 |
| SDA | PB9 | 软件I2C数据线 |
| VCC | 3.3V | 电源 |
| GND | GND | 电源地 |

#### UART1（用于详细日志输出）

| 引脚 | STM32连接 | 说明 |
|------|-----------|------|
| TX | PA9 | UART1发送 |
| RX | PA10 | UART1接收 |
| 波特率 | 115200 | 串口通信波特率 |

**连接说明**：将UART1连接到USB转串口模块，用于查看详细日志输出。建议使用串口助手软件保存完整日志，便于后续分析。

## 📦 模块依赖

### 模块依赖关系图

```mermaid
%%{init: {'flowchart': {'curve': 'basis'}}}%%
flowchart TB
    %% 应用层
    subgraph APP_LAYER[应用层]
        APP[Flash15案例<br/>main_example.c<br/>flash15_endurance_test.c]
    end
    
    %% 系统服务层
    subgraph SYS_LAYER[系统服务层]
        direction LR
        SYS_INIT[System_Init]
        DELAY[Delay]
        BASE_TIMER[TIM2_TimeBase]
        SYS_INIT --- DELAY
        DELAY --- BASE_TIMER
    end
    
    %% 驱动层
    subgraph DRV_LAYER[驱动层]
        direction LR
        GPIO[GPIO]
        SPI[SPI_HW]
        W25Q[W25Q]
        I2C_SW[I2C_SW]
        OLED[OLED]
        LED[LED]
        UART[UART]
    end
    
    %% 调试工具层
    subgraph DEBUG_LAYER[调试工具层]
        direction LR
        DEBUG[Debug]
        LOG[Log]
        ERROR[ErrorHandler]
        DWT[DWT]
    end
    
    %% 硬件抽象层
    subgraph BSP_LAYER[硬件抽象层]
        BSP[board.h<br/>硬件配置]
    end
    
    %% 依赖关系
    APP --> SYS_INIT
    APP --> DELAY
    APP --> W25Q
    APP --> OLED
    APP --> LED
    APP --> UART
    APP --> LOG
    APP --> ERROR
    APP --> DWT
    
    W25Q --> SPI
    SPI --> GPIO
    OLED --> I2C_SW
    I2C_SW --> GPIO
    LED --> GPIO
    UART --> GPIO
    
    DELAY --> BASE_TIMER
    LOG --> DEBUG
    DEBUG --> UART
    
    APP --> BSP
    W25Q --> BSP
    SPI --> BSP
    I2C_SW --> BSP
    OLED --> BSP
    LED --> BSP
    UART --> BSP
    
    %% 样式
    classDef appLayer fill:#fff3e0,stroke:#e65100,stroke-width:2px
    classDef sysLayer fill:#e3f2fd,stroke:#1565c0,stroke-width:2px
    classDef driverLayer fill:#e8f5e9,stroke:#1b5e20,stroke-width:2px
    classDef debugLayer fill:#fff3e0,stroke:#e65100,stroke-width:2px
    classDef bspLayer fill:#fce4ec,stroke:#880e4f,stroke-width:2px
    
    class APP appLayer
    class SYS_INIT,DELAY,BASE_TIMER sysLayer
    class GPIO,SPI,W25Q,I2C_SW,OLED,LED,UART driverLayer
    class DEBUG,LOG,ERROR,DWT debugLayer
    class BSP bspLayer
```

### 模块列表

| 模块分类 | 模块名称 | 用途 | 依赖关系 |
|---------|---------|------|---------|
| 应用层 | main_example.c | 主函数入口 | flash15_endurance_test |
| 应用层 | flash15_endurance_test | 寿命测试核心实现 | W25Q, SPI, Delay, DWT |
| 驱动层 | W25Q | W25Q Flash驱动 | SPI |
| 驱动层 | SPI_HW | SPI硬件驱动 | GPIO |
| 驱动层 | OLED | OLED显示驱动 | I2C_SW |
| 驱动层 | I2C_SW | 软件I2C驱动 | GPIO |
| 驱动层 | LED | LED驱动 | GPIO |
| 驱动层 | UART | UART驱动 | GPIO |
| 系统服务层 | System_Init | 系统初始化 | GPIO, LED |
| 系统服务层 | Delay | 延时服务 | TIM2_TimeBase |
| 调试工具层 | Debug | printf重定向 | UART |
| 调试工具层 | Log | 分级日志系统 | Debug |
| 调试工具层 | ErrorHandler | 错误处理框架 | - |
| 调试工具层 | DWT | 数据观察点单元（用于精确计时） | - |

## 🔄 实现流程

### 整体逻辑

程序执行流程分为以下几个阶段：

1. **系统初始化阶段**：初始化系统、SPI、W25Q、OLED、UART、Debug、Log、DWT等模块
2. **基准数据记录阶段**：执行全片擦除，记录初始性能基准（擦除时间、编程时间、读取速度、错误率）
3. **主循环阶段**：持续执行P/E循环，直至芯片报废
   - 每次循环：全片擦除 → 全片写入 → 全片读取校验
   - 实时统计：擦除时间、编程时间、错误数量、坏块数量
   - 累计统计：总循环数、总写入数据量、平均时间、错误率
4. **定期深度检查阶段**：每1000次循环执行一次深度健康检查
5. **报废判定阶段**：检查是否达到报废标准，达到则终止测试
6. **结果输出阶段**：输出完整的测试结果和统计数据

### 数据流向图

```mermaid
%%{init: {'flowchart': {'curve': 'basis'}}}%%
flowchart LR
    %% 输入
    W25Q[W25Q Flash<br/>硬件设备]
    
    %% 处理
    SPI[SPI接口<br/>spi_hw]
    W25Q_DRV[W25Q驱动<br/>w25q_spi]
    ENDURANCE[寿命测试<br/>flash15_endurance_test]
    DWT[DWT计时<br/>精确测量]
    
    %% 输出
    OLED[OLED显示<br/>当前状态]
    UART[串口输出<br/>详细日志]
    LED[LED指示<br/>系统运行]
    
    %% 数据流
    W25Q -->|SPI通信| SPI
    SPI -->|SPI命令| W25Q_DRV
    W25Q_DRV -->|Flash数据| ENDURANCE
    DWT -->|时间测量| ENDURANCE
    ENDURANCE -->|测试结果| OLED
    ENDURANCE -->|测试日志| UART
    ENDURANCE -->|状态指示| LED
    
    %% 样式
    style W25Q fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    style SPI fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
    style W25Q_DRV fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
    style ENDURANCE fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style DWT fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style OLED fill:#ffccbc,stroke:#d84315,stroke-width:2px
    style UART fill:#ffccbc,stroke:#d84315,stroke-width:2px
    style LED fill:#ffccbc,stroke:#d84315,stroke-width:2px
```

### 工作流程示意图

```mermaid
%%{init: {'flowchart': {'curve': 'basis'}}}%%
flowchart TD
    START[系统启动] --> INIT[系统初始化]
    INIT --> W25Q_INIT[W25Q初始化]
    W25Q_INIT --> ENDURANCE_INIT[寿命测试初始化]
    
    ENDURANCE_INIT --> BASELINE[记录基准数据<br/>0次循环]
    BASELINE --> CYCLE_START[开始P/E循环]
    
    CYCLE_START --> ERASE[全片擦除]
    ERASE --> WRITE[全片写入固定值]
    WRITE --> READ[全片读取并校验]
    READ --> STAT[更新统计数据]
    
    STAT --> CHECK_DEEP{是否达到<br/>深度检查间隔?}
    CHECK_DEEP -->|是| DEEP_CHECK[深度健康检查]
    CHECK_DEEP -->|否| CHECK_DEAD
    DEEP_CHECK --> CHECK_DEAD
    
    CHECK_DEAD{是否达到<br/>报废标准?}
    CHECK_DEAD -->|是| RESULT[输出测试结果]
    CHECK_DEAD -->|否| CYCLE_START
    
    RESULT --> END[测试结束]
    
    %% 样式
    style START fill:#e1f5ff,stroke:#01579b,stroke-width:2px
    style INIT fill:#f3e5f5,stroke:#4a148c,stroke-width:2px
    style BASELINE fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style ERASE fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style WRITE fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style READ fill:#fff3e0,stroke:#e65100,stroke-width:2px
    style DEEP_CHECK fill:#ffccbc,stroke:#d84315,stroke-width:2px
    style CHECK_DEAD fill:#ffcdd2,stroke:#c62828,stroke-width:2px
    style RESULT fill:#c8e6c9,stroke:#2e7d32,stroke-width:2px
    style END fill:#e1f5ff,stroke:#01579b,stroke-width:2px
```

## 📚 关键函数说明

### 寿命测试模块相关函数

- **`EnduranceTest_Init()`**：寿命测试模块初始化
  - 检查W25Q是否已初始化
  - 初始化DWT（数据观察点单元）用于精确计时
  - 设置模块初始化标志

- **`EnduranceTest_RecordBaseline()`**：记录基准数据（0次循环）
  - 执行全片擦除，测量初始擦除时间
  - 测试100页编程时间，计算平均编程时间
  - 测试读取速度
  - 检测初始错误率

- **`EnduranceTest_RunSingleCycle()`**：执行单次P/E循环
  - 全片擦除并测量时间
  - 全片写入固定值数据（当前循环编号的低8位）
  - 全片读取并逐字节校验
  - 统计错误（擦除错误、编程错误、校验错误）
  - 更新累计统计数据

- **`EnduranceTest_Run()`**：运行完整寿命测试流程
  - 记录基准数据
  - 持续执行P/E循环，直至芯片报废
  - 每1000次循环执行深度健康检查
  - 每100次循环记录日志
  - 检查报废判定标准
  - 返回测试结果

- **`EnduranceTest_CheckEndOfLife()`**：检查是否达到报废标准
  - 检查硬失效标准（擦除失败、编程失败、坏块率、误码率、擦除时间）
  - 检查软失效标准（寿命评分、性能退化率）
  - 返回芯片是否已报废

- **`EnduranceTest_CalculateLifetimeScore()`**：计算寿命评分（0-100分）
  - 基于擦除时间退化率、误码率、坏块率、读干扰错误计算综合评分
  - 评分越低表示芯片寿命越接近终点

### 辅助函数

- **`GeneratePattern()`**：生成测试数据模式
  - 使用固定值填充缓冲区（每轮循环值+1）
  - 便于错误检测和调试

- **`CountBitErrors()`**：统计位错误数量
  - 逐字节比较期望值和实际值
  - 统计所有位错误

## 📊 数据解读

### 块擦除时间性能档位参考

| 档位 | 擦除时间 | 性能描述 | 参考型号 | 备注 |
|------|----------|----------|----------|------|
| **优秀** | **≤120ms** | 卓越性能 | W25Q-RV系列 | 高于标准规格，适合高频更新场景 |
| **良好** | **121-180ms** | 正常性能 | W25Q16/32/64/128 | 符合典型值150ms标准，主流水平 |
| **一般** | **181-500ms** | 可接受范围 | 老化芯片/低温环境 | 性能下降但仍可用，建议监控 |
| **偏弱** | **>500ms** | 性能偏低 | 故障风险或极端条件 | 接近2000ms最大值，需排查 |

**说明**：
- 擦除时间指单个Block（64KB）的擦除时间
- 全片擦除时间 = Block数量 × 单Block擦除时间
- 对于8MB芯片（128个Block），全片擦除时间约为：128 × 150ms ≈ 19.2秒
- 测试中显示的擦除时间包含全片擦除时间（秒）和每块平均时间（ms/块）
- **报废判定阈值**：根据芯片容量动态计算，阈值 = Block数量 × 200ms/块
  - 8MB: 128 × 200ms = 25.6秒
  - 16MB: 256 × 200ms = 51.2秒
  - 32MB: 512 × 200ms = 102.4秒
  - 64MB: 1024 × 200ms = 204.8秒
  - 128MB: 2048 × 200ms = 409.6秒

### 关键指标说明

1. **总P/E循环次数**：
   - 表示芯片完成的完整擦写循环次数
   - 典型W25Q芯片寿命：10,000-100,000次P/E循环
   - 测试将持续到芯片报废

2. **总写入数据量**：
   - 计算公式：总循环数 × 芯片容量
   - 例如：1000次循环 × 8MB = 8GB
   - 用于评估芯片累计写入量

3. **擦除时间退化率**：
   - 计算公式：`(当前平均擦除时间 - 基准擦除时间) / 基准擦除时间 × 100%`
   - 正常值：< 30%
   - 预警值：30-50%
   - 危险值：> 50%

4. **误码率**：
   - 计算公式：`校验错误位数 / 总数据位数`
   - 正常值：< 1e-8
   - 预警值：1e-8 - 1e-4
   - 危险值：> 1e-4

5. **坏块率**：
   - 计算公式：`坏块数量 / 总块数量 × 100%`
   - 正常值：0%
   - 预警值：1-5%
   - 危险值：> 5%

6. **寿命评分**：
   - 综合评分：0-100分
   - 计算公式：`100 - (擦除退化率×0.3 + 误码率×0.3 + 坏块率×0.2 + 读干扰×0.2)`
   - 优秀：> 80分
   - 良好：60-80分
   - 一般：40-60分
   - 差：< 40分
   - 报废：< 20分

### 测试结果示例

```
=== 寿命测试完成（芯片已报废） ===

【关键数据 - 总写入数据量】
  总P/E循环次数: 15234 次
  总写入数据量: 121872 MB (119.02 GB)

【时间统计】
  最终擦除时间: 18.45 秒 (144.14 ms/块)
  平均擦除时间: 17.23 秒 (134.61 ms/块)
  最小擦除时间: 16.74 秒 (130.78 ms/块)
  最大擦除时间: 22.15 秒 (172.66 ms/块)
  平均编程时间: 1.504 ms/页

【错误统计】
  擦除错误次数: 0
  编程错误次数: 0
  校验错误次数: 1250 位
  坏块数量: 2
  最终误码率: 1.25e-05
  读干扰错误: 0

【性能退化】
  擦除时间退化率: 2.93%
  编程时间退化率: 0.03%
  基准擦除时间: 16.74 秒
  基准编程时间: 1.504 ms/页

【最终状态】
  最终寿命评分: 95.50
  芯片状态: 已报废（误码率超标）
```

## ⚠️ 注意事项与重点

### ⚠️ 重要提示

1. **破坏性测试警告**：
   - **本测试会将芯片测到报废，请勿在正常使用的芯片上运行！**
   - 测试芯片将被物理损坏，无法恢复
   - 建议使用专用测试芯片或报废芯片
   - 测试前请备份重要数据（如果有）

2. **测试时间**：
   - 单次P/E循环时间：约2-5分钟（取决于芯片容量）
   - 完整测试时间：数天至数周（取决于芯片寿命）
   - 8MB芯片示例：1000次循环约需33-83小时
   - 建议使用稳定的电源和良好的散热

3. **电源稳定性**：
   - **测试过程中必须保持电源稳定，避免断电**
   - 断电可能导致数据丢失和测试中断
   - 建议使用UPS或稳定的电源适配器

4. **散热管理**：
   - 频繁擦写会产生热量
   - 建议在芯片上加装散热片
   - 确保良好的通风环境

5. **数据记录**：
   - 建议使用串口助手保存完整日志
   - 日志文件可用于后续分析和报告生成
   - 每100次循环自动记录数据

6. **芯片适配**：
   - 测试自动适配不同容量的W25Q芯片（8MB、16MB、32MB、64MB、128MB等）
   - 自动识别芯片型号和容量
   - 根据芯片容量动态调整测试参数：
     - 擦除时间阈值：Block数量 × 200ms/块
     - WaitReady超时时间：Block数量 × 200ms/块 × 3（安全系数）
     - 预计时间提示：根据实际容量和页数动态计算
     - 日志信息：显示实际容量、页数、块数，不再硬编码

### 🔑 关键点

1. **固定值写入模式**：
   - 每轮循环写入相同的固定值（循环编号的低8位）
   - 便于快速检测写入错误
   - 读取时立即发现数据不匹配

2. **精确计时**：
   - 使用DWT（数据观察点单元）进行精确计时
   - 分辨率：微秒级
   - 用于测量擦除时间、编程时间、读取时间

3. **实时监控**：
   - OLED显示当前循环数和关键状态
   - UART输出详细日志（每100次循环）
   - LED指示系统运行状态

4. **报废判定**：
   - 硬失效：立即终止（擦除失败、编程失败、坏块率>5%）
   - 软失效：综合评分判定（寿命评分<20）
   - 自动终止测试，避免无效循环

5. **数据统计**：
   - 实时统计：当前循环的错误和时间
   - 累计统计：总循环数、总写入数据量、平均时间
   - 性能退化：擦除时间退化率、编程时间退化率

## 🔍 常见问题排查

### 问题1：测试立即终止

**可能原因**：
- 芯片已损坏
- SPI通信问题
- 电源不稳定

**解决方法**：
- 检查SPI连接是否正确
- 检查电源是否稳定（3.3V）
- 尝试更换测试芯片
- 检查串口日志中的错误信息

### 问题2：擦除时间异常长

**可能原因**：
- 芯片已老化
- 温度过低
- SPI时钟频率设置过低

**解决方法**：
- 检查SPI时钟频率设置（当前为2分频，约18MHz）
- 检查环境温度（建议室温）
- 擦除时间阈值根据芯片容量动态计算（Block数量 × 200ms/块），如果超过阈值，芯片将被判定为报废

### 问题3：误码率持续上升

**可能原因**：
- 芯片寿命接近终点
- 电源不稳定
- 温度过高

**解决方法**：
- 检查电源稳定性
- 检查散热情况
- 这是正常的寿命退化现象，测试会继续直到报废

### 问题4：测试时间过长

**可能原因**：
- 芯片容量大
- SPI时钟频率低
- 循环次数多

**解决方法**：
- 这是正常现象，寿命测试需要较长时间
- 可以调整深度检查间隔（默认1000次）
- 可以调整日志记录间隔（默认100次）
- 建议使用串口助手保存日志，然后离线分析

### 问题5：内存不足

**可能原因**：
- 测试缓冲区过大
- 系统其他模块占用内存过多

**解决方法**：
- 测试已优化内存使用（使用静态缓冲区）
- 如果仍有问题，可以减少测试区域（修改代码）
- STM32F103C8T6有20KB RAM，通常足够使用

### 问题6：芯片未报废但测试停止

**可能原因**：
- 达到硬失效标准（擦除失败、编程失败、坏块率>5%）
- 达到软失效标准（寿命评分<20）
- 擦除时间超过阈值（>120秒）

**解决方法**：
- 检查串口日志中的报废判定原因
- 这是正常的测试终止，芯片已达到报废标准
- 可以查看最终统计数据

## 💡 扩展练习

### 循序渐进部分

1. **观察基准数据**：
   - 运行测试，观察基准数据记录
   - 理解擦除时间、编程时间、读取速度的含义
   - 验证数据是否在正常范围内

2. **监控前几次循环**：
   - 观察前10次循环的数据
   - 检查擦除时间是否稳定
   - 检查是否有错误产生

3. **分析性能退化**：
   - 观察擦除时间退化率的变化
   - 观察误码率的变化趋势
   - 理解性能退化的规律

### 实际场景坑点部分

4. **处理断电情况**：
   - 测试过程中断电会导致数据丢失
   - 需要实现数据持久化（如保存到SD卡）
   - 需要实现断点续测功能

5. **优化测试速度**：
   - SPI时钟频率：当前为2分频，约18MHz（已优化）
   - 可以进一步优化擦除和写入流程
   - 注意平衡速度和稳定性

6. **多芯片并行测试**：
   - 可以使用多个SPI接口同时测试多片芯片
   - 需要独立的SPI总线和CS信号
   - 可以大幅提高测试效率

7. **数据分析与可视化**：
   - 导出串口日志数据
   - 使用Python/Excel分析数据
   - 绘制擦除时间退化曲线
   - 绘制误码率累计曲线
   - 生成寿命预测模型

## 📖 相关文档

### 模块文档

- **W25Q驱动**：`Drivers/flash/w25q_spi.c/h`
- **SPI驱动**：`Drivers/spi/spi_hw.c/h`
- **Delay模块**：`System/delay.c/h`
- **OLED驱动**：`Drivers/display/oled_ssd1306.c/h`
- **UART驱动**：`Drivers/uart/uart.c/h`
- **Log模块**：`Debug/log.c/h`
- **错误处理**：`Common/error_handler.c/h`
- **DWT模块**：`Debug/debug.c/h`（DWT相关函数）

### 业务文档

- **主程序代码**：`Examples/Flash/Flash15_W25QLifetime/main_example.c`
- **寿命测试实现**：`Examples/Flash/Flash15_W25QLifetime/flash15_endurance_test.c/h`
- **硬件配置**：`Examples/Flash/Flash15_W25QLifetime/board.h`
- **模块配置**：`Examples/Flash/Flash15_W25QLifetime/config.h`
- **项目规范**：`PROJECT_KEYWORDS.md`
- **相关案例**：`Examples/Flash/Flash14_TestW25QQuality/README.md`（品质测试案例）

## 📝 测试报告模板

测试完成后，建议记录以下信息：

```
=== W25Q寿命测试报告 ===

【测试信息】
  测试日期：YYYY-MM-DD
  测试芯片型号：W25Q64（8MB）
  测试芯片批次：XXXX
  测试环境温度：XX°C

【测试结果】
  总P/E循环次数：XXXX 次
  总写入数据量：XXXX MB (XX.XX GB)
  测试持续时间：XX小时XX分钟

【性能数据】
  初始擦除时间：XX.XX 秒 (XXX.XX ms/块)
  最终擦除时间：XX.XX 秒 (XXX.XX ms/块)
  擦除时间退化率：XX.XX%
  最终误码率：X.XXe-XX
  坏块数量：X
  最终寿命评分：XX.XX

【报废原因】
  [ ] 擦除失败
  [ ] 编程失败
  [ ] 坏块率超标（>5%）
  [ ] 误码率超标（>1e-3）
  [ ] 擦除时间超标（>120秒）
  [ ] 寿命评分过低（<20）

【结论】
  芯片寿命：XXXX 次P/E循环
  性能评级：[优秀/良好/一般/偏弱]
  建议：[继续使用/监控使用/立即更换]
```

---

**最后更新**：2024-01-01（v1.0.0 - 初始版本）

