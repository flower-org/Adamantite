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

typedef struct {
    SPI_HandleTypeDef* hspi;
    DMA_HandleTypeDef* hdma_rx;
    DMA_HandleTypeDef* hdma_tx;

    GPIO_TypeDef* cs_port;
    uint16_t cs_pin;

    ElasticQueue_t* lan_rx_queue;
    ElasticQueue_t* lan_tx_queue;

    ElasticQueueRef_t* current_rx_packet;

    bool rx_packet_ready;
    bool rx_dma_ready;  // Tracks if DMA is idle and ready for a new transaction
} DM9051_HandleTypeDef;

uint16_t DM9051_ReadPHY(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t phy_reg);
void DM9051_Init(DM9051_HandleTypeDef *hdm, uint8_t *mac_addr);

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm, uint8_t *data, uint16_t len);
void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm);

// To be called from HAL_SPI_TxCpltCallback
void DM9051_TxCpltCallback(DM9051_HandleTypeDef *hdm);
// To be called from HAL_SPI_RxCpltCallback
void DM9051_RxCpltCallback(DM9051_HandleTypeDef *hdm);

#endif /* __DM9051_H */
