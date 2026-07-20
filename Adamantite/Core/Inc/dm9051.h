#ifndef __DM9051_H
#define __DM9051_H

#include "main.h"
#include "elastic_queue.h"

// DM9051 Registers
#define DM9051_NCR      0x00
#define DM9051_NSR      0x01
#define DM9051_TCR      0x02
#define DM9051_RCR      0x05
#define DM9051_EPCR     0x0B
#define DM9051_EPAR     0x0C
#define DM9051_EPDRL    0x0D
#define DM9051_EPDRH    0x0E
#define DM9051_PAR      0x10
#define DM9051_GPCR     0x1E
#define DM9051_GPR      0x1F
#define DM9051_VIDL     0x28
#define DM9051_VIDH     0x29
#define DM9051_PIDL     0x2A
#define DM9051_PIDH     0x2B
#define DM9051_INTCR    0x39
#define DM9051_MRCMDX   0x70
#define DM9051_MRCMD    0x72
#define DM9051_TXPLL    0x7C
#define DM9051_TXPLH    0x7D
#define DM9051_ISR      0x7E
#define DM9051_IMR      0xFF

// DM9051 Receive Control Register (RCR) Flags
#define DM9051_RCR_RXEN      0x01 // RX Enable
#define DM9051_RCR_PRMSC     0x02 // Promiscuous Mode
#define DM9051_RCR_RUNT      0x04 // Pass Runt Packet
#define DM9051_RCR_ALL       0x08 // Pass All Multicast
#define DM9051_RCR_DIS_CRC   0x10 // Discard CRC Error Packet
#define DM9051_RCR_DIS_LONG  0x20 // Discard Long Packet
#define DM9051_RCR_WTDIS     0x40 // Watchdog Timer Disable
#define DM9051_RCR_HASH_ALL  0x80 // Hash All

// DM9051 Interrupt Status Register (ISR) Flags
#define DM9051_ISR_PRS       0x01 // Packet Received Status
#define DM9051_ISR_PTS       0x02 // Packet Transmitted Status
#define DM9051_ISR_ROS       0x04 // Receive Overflow Status
#define DM9051_ISR_ROOS      0x08 // Receive Overflow Counter Overflow
#define DM9051_ISR_LNKCHG    0x20 // Link Status Change
#define DM9051_ISR_UDRUN     0x40 // Transmit Under Run
#define DM9051_ISR_IOMODE    0x80 // I/O Mode (or Stop MRCMD)

// DM9051 Interrupt Mask Register (IMR) Flags
#define DM9051_IMR_PRM       0x01 // Packet Received Mask
#define DM9051_IMR_PTM       0x02 // Packet Transmitted Mask
#define DM9051_IMR_ROM       0x04 // Receive Overflow Mask
#define DM9051_IMR_ROOM      0x08 // Receive Overflow Counter Mask
#define DM9051_IMR_LNKCHGM   0x20 // Link Status Change Mask
#define DM9051_IMR_UDRUNM    0x40 // Transmit Under Run Mask
#define DM9051_IMR_PAR       0x80 // Pointer Auto Return

#define DM9051_DMA_READY     0x00 // Packet Received Mask
#define DM9051_DMA_RX        0x01 // Packet Transmitted Mask
#define DM9051_DMA_TX        0x02 // Receive Overflow Mask

typedef struct {
    SPI_HandleTypeDef* hspi;
    DMA_HandleTypeDef* hdma_rx;
    DMA_HandleTypeDef* hdma_tx;

    GPIO_TypeDef* cs_port;
    uint16_t cs_pin;

    ElasticQueue_t* lan_rx_queue;
    ElasticQueue_t* lan_tx_queue;

    ElasticQueueRef_t* current_rx_packet;

    bool can_read_packet;
    uint8_t mac_tx_slots;

    // On STM32 SPI we have 2 physical SPI DMA channels and we can use both RX and TX simultaneously.
    // However DM9051 doesn't support that, so we have to alternate.
    // Moreover, we have to use both RX and TX to avoid SPI error HAL_SPI_ERROR_OVR.
    // That's why logically it's pretty much the same DMA.
    uint8_t dma_status;
} DM9051_HandleTypeDef;

uint16_t DM9051_ReadPHY(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t phy_reg);
void DM9051_Init(DM9051_HandleTypeDef *hdm, uint8_t *mac_addr);

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm);
void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm);
void DM9051_ProcessInterrupt(DM9051_HandleTypeDef *hdm);

// To be called from HAL_SPI_TxCpltCallback
//void DM9051_TxCpltCallback(DM9051_HandleTypeDef *hdm);
// To be called from HAL_SPI_RxCpltCallback
//void DM9051_RxCpltCallback(DM9051_HandleTypeDef *hdm);
void DM9051_TxRxCpltCallback(DM9051_HandleTypeDef *hdm);

#endif /* __DM9051_H */
