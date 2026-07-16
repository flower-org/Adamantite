#ifndef __DM9051_H
#define __DM9051_H

#include "main.h"

void DM9051_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    
    // TX Buffer: 1 byte for command (0xF8) + 1522 bytes for max Ethernet frame
    uint8_t tx_buf[1524]; 
    uint16_t tx_len;
    
    // RX Buffer
    uint8_t rx_buf[1524];
    uint16_t rx_len;
} DM9051_HandleTypeDef;

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm, uint8_t *data, uint16_t len);
void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm);

// To be called from HAL_SPI_TxCpltCallback
void DM9051_TxCpltCallback(DM9051_HandleTypeDef *hdm);
// To be called from HAL_SPI_RxCpltCallback
void DM9051_RxCpltCallback(DM9051_HandleTypeDef *hdm);

#endif /* __DM9051_H */
