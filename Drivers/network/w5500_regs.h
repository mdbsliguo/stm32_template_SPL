/**
 * @file w5500_regs.h
 * @brief W5500寄存器地址与位定义（自模板提取，无硬件引脚宏）
 */

#ifndef W5500_REGS_H
#define W5500_REGS_H

/* ==================== 通用寄存器 ==================== */

#define W5500_MR           0x0000U
#define W5500_MR_RST         0x80U
#define W5500_MR_WOL         0x20U
#define W5500_MR_PB          0x10U
#define W5500_MR_PPP         0x08U
#define W5500_MR_FARP        0x02U

#define W5500_GAR          0x0001U
#define W5500_SUBR         0x0005U
#define W5500_SHAR         0x0009U
#define W5500_SIPR         0x000fU

#define W5500_INTLEVEL     0x0013U
#define W5500_IR           0x0015U
#define W5500_IR_CONFLICT    0x80U
#define W5500_IR_UNREACH     0x40U
#define W5500_IR_PPPOE       0x20U
#define W5500_IR_MP          0x10U

#define W5500_IMR          0x0016U
#define W5500_SIMR         0x0018U
#define W5500_SIR          0x0017U
#define W5500_S0_INT         0x01U

#define W5500_RTR          0x0019U
#define W5500_RCR          0x001bU

#define W5500_PHYCFGR      0x002eU
#define W5500_PHY_LINK       0x01U

#define W5500_VERR         0x0039U

/* ==================== Socket寄存器 ==================== */

#define W5500_Sn_MR          0x0000U
#define W5500_Sn_MR_TCP        0x01U
#define W5500_Sn_MR_UDP        0x02U

#define W5500_Sn_CR          0x0001U
#define W5500_Sn_CR_OPEN       0x01U
#define W5500_Sn_CR_LISTEN     0x02U
#define W5500_Sn_CR_CONNECT    0x04U
#define W5500_Sn_CR_CLOSE      0x10U
#define W5500_Sn_CR_SEND       0x20U
#define W5500_Sn_CR_RECV       0x40U

#define W5500_Sn_IR          0x0002U
#define W5500_Sn_IR_SEND_OK    0x10U
#define W5500_Sn_IR_TIMEOUT    0x08U
#define W5500_Sn_IR_RECV       0x04U
#define W5500_Sn_IR_DISCON     0x02U
#define W5500_Sn_IR_CON        0x01U

#define W5500_Sn_SR          0x0003U
#define W5500_SOCK_CLOSED      0x00U
#define W5500_SOCK_INIT        0x13U
#define W5500_SOCK_LISTEN      0x14U
#define W5500_SOCK_ESTABLISHED 0x17U
#define W5500_SOCK_CLOSE_WAIT  0x1CU
#define W5500_SOCK_UDP         0x22U

#define W5500_Sn_PORT        0x0004U
#define W5500_Sn_DHAR        0x0006U
#define W5500_Sn_DIPR        0x000cU
#define W5500_Sn_DPORTR      0x0010U
#define W5500_Sn_MSSR        0x0012U
#define W5500_Sn_RXBUF_SIZE  0x001eU
#define W5500_Sn_TXBUF_SIZE  0x001fU
#define W5500_Sn_TX_FSR      0x0020U
#define W5500_Sn_TX_WR       0x0024U
#define W5500_Sn_RX_RSR      0x0026U
#define W5500_Sn_RX_RD       0x0028U
#define W5500_Sn_IMR         0x002cU

/* ==================== SPI控制字节 ==================== */

#define W5500_VDM            0x00U
#define W5500_FDM1           0x01U
#define W5500_FDM2           0x02U
#define W5500_FDM4           0x03U
#define W5500_RWB_READ       0x00U
#define W5500_RWB_WRITE      0x04U
#define W5500_COMMON_R       0x00U

/* Socket缓冲区大小（每Socket 2KB，与模板一致） */
#define W5500_S_RX_SIZE      2048U
#define W5500_S_TX_SIZE      2048U

/* 芯片版本寄存器期望值 */
#define W5500_EXPECTED_VERSION  0x04U

#endif /* W5500_REGS_H */
