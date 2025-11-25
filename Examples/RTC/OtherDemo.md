# DS3231其他功能演示案例

本文档列出了使用DS3231其他功能的关联案例。

---

## 🔗 关联案例

### Timer案例

#### Timer03 - DS3231外部方波输出
- **案例路径**：Examples/Timer/Timer03_DS3231ExternalSquareWave/
- **功能说明**：演示使用DS3231的SQW引脚输出1Hz方波作为TIM1定时器的外部时钟源
- **核心内容**：
  - DS3231方波输出配置（DS3231_SetSquareWave）
  - TIM1外部时钟配置（TIM_ETRClockMode2Config）
  - ETR引脚的使用和配置
  - 外部时钟驱动的计数效果
- **硬件连接**：
  - DS3231 SQW引脚 → PA12（TIM1 ETR引脚）
  - DS3231通过I2C接口配置（PB10/11）
- **详细文档**：[Timer03 README](../Timer/Timer03_DS3231ExternalSquareWave/README.md)

#### Timer04 - DS3231外部时钟32kHz输出
- **案例路径**：Examples/Timer/Timer04_DS3231ExternalClock32kHz/
- **功能说明**：演示使用DS3231的32K引脚输出32kHz时钟作为TIM1定时器的外部时钟源
- **核心内容**：
  - DS3231 32kHz输出使能（DS3231_Enable32kHz）
  - TIM1外部时钟配置（TIM_ETRClockMode2Config）
  - ETR引脚的使用和配置
  - 高频外部时钟驱动的计数效果
- **硬件连接**：
  - DS3231 32K引脚 → PA12（TIM1 ETR引脚）
  - DS3231通过I2C接口配置（PB10/11）
- **详细文档**：[Timer04 README](../Timer/Timer04_DS3231ExternalClock32kHz/README.md)

---

## 📝 说明

这两个案例展示了DS3231的两种时钟输出功能：
- **SQW引脚**：可配置频率的方波输出（1Hz、1.024kHz、4.096kHz、8.192kHz）
- **32K引脚**：固定32kHz时钟输出

两个案例都使用TIM1定时器通过ETR引脚接收外部时钟信号，演示了如何使用硬件定时器对外部时钟进行计数和测量。

---

**最后更新**：2024-01-01
