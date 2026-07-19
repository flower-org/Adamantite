#include "dm9051.h"
#include <string.h>
#include "log.h"

ElasticQueueRef_t* ref;

// Forward declarations for static MAC access functions
static void DM9051_WriteReg(DM9051_HandleTypeDef *hdm, uint8_t reg, uint8_t val);
static uint8_t DM9051_ReadReg(DM9051_HandleTypeDef *hdm, uint8_t reg);

// Write to MAC Register
static void DM9051_WriteReg(DM9051_HandleTypeDef *hdm, uint8_t reg, uint8_t val) {
    uint8_t spi_tx[2] = {0x80 | reg, val};
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hdm->hspi, spi_tx, 2, 100);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
}

// Read from MAC Register
static uint8_t DM9051_ReadReg(DM9051_HandleTypeDef *hdm, uint8_t reg) {
    uint8_t spi_tx[2] = {reg, 0x00};
    uint8_t spi_rx[2] = {0};
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, spi_tx, spi_rx, 2, 100);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    return spi_rx[1];
}

// ================================ INIT ================================

void DM9051_Init(DM9051_HandleTypeDef *hdm, uint8_t *mac_addr)
{
  hdm->rx_dma_ready = true;

  // 1. GPCR: GEP Output
  DM9051_WriteReg(hdm, DM9051_GPCR, 0x01);

  // 2. GPR: Power ON internal PHY BEFORE core reset!
  DM9051_WriteReg(hdm, DM9051_GPR, 0x00);
  
  HAL_Delay(2); // Mandatory delay after PHY power on

  // 3. NCR: Core Reset
  DM9051_WriteReg(hdm, DM9051_NCR, 0x01);

  HAL_Delay(2); // Wait for reset

  // 4. Clear NCR Reset
  DM9051_WriteReg(hdm, DM9051_NCR, 0x00);

  // 5. Check Device ID
  uint16_t vid = (DM9051_ReadReg(hdm, DM9051_VIDH) << 8) | DM9051_ReadReg(hdm, DM9051_VIDL);
  uint16_t pid = (DM9051_ReadReg(hdm, DM9051_PIDH) << 8) | DM9051_ReadReg(hdm, DM9051_PIDL);

  // Optional: Could return an error if vid != 0x0A46 or pid != 0x9051
  (void)vid;
  (void)pid;

  // 6. Set MAC Address (Registers 0x10 to 0x15)
  for (int i = 0; i < 6; i++) {
    DM9051_WriteReg(hdm, DM9051_PAR + i, mac_addr[i]);
  }

  // 7. Configure DM9051 Interrupts
  // Set Interrupt Polarity in INTCR (Register 0x39) to Active Low (0x00)
  DM9051_WriteReg(hdm, DM9051_INTCR, 0x00);

  // 8. Clear any pending interrupts in ISR (Register 0x7E)
  DM9051_WriteReg(hdm, DM9051_ISR, 0xFF);

  // 9. Enable interrupts in IMR (Register 0xFF)
  // 0x80 (SRAM pointer auto-return) | 0x01 (Packet Received) = 0x81
  DM9051_WriteReg(hdm, DM9051_IMR, 0x81);

  // 10. Enable Receiver in RCR (Register 0x05)
  DM9051_WriteReg(hdm, DM9051_RCR, DM9051_RCR_DIS_LONG | DM9051_RCR_DIS_CRC | 
                             DM9051_RCR_ALL | DM9051_RCR_RUNT | 
                             DM9051_RCR_PRMSC | DM9051_RCR_RXEN);
}

// ================================ WRITE ================================

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm, uint8_t *data, uint16_t len) {
    /*
    hdm->tx_dma_ready = false;

    // 1. Wait for NSR TX ready (TX1END or TX2END)
    // In a real application, you'd want a non-blocking timeout or interrupt driven approach
    while(!(DM9051_ReadReg(hdm, DM9051_NSR) & 0x0C));
    
    // 2. Prepare the DMA buffer
    hdm->tx_buf[0] = 0xF8; // MWCMD (Memory Write with SPI Write bit 0x80)
    memcpy(&hdm->tx_buf[1], data, len);
    hdm->tx_len = len;

    // 3. Start SPI DMA
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(hdm->hspi, hdm->tx_buf, len + 1);
    */
}

void DM9051_TxCpltCallback(DM9051_HandleTypeDef *hdm) {
    /*
    // 1. Deassert CS
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);

    // 2. Write TX length
    DM9051_WriteReg(hdm, DM9051_TXPLL, hdm->tx_len & 0xFF);        // TXPLL
    DM9051_WriteReg(hdm, DM9051_TXPLH, (hdm->tx_len >> 8) & 0xFF); // TXPLH

    // 3. Issue TX request
    DM9051_WriteReg(hdm, DM9051_TCR, 0x01); // TCR_TXREQ
    
    // 4. Clear Packet Transmitted Interrupt
    DM9051_WriteReg(hdm, DM9051_ISR, 0x02); // ISR_PTS (bit 1)
    
    hdm->tx_dma_ready = true;
    */
}

// ================================ READ ================================

void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm) {
    hdm->rx_packet_ready = 0;

    // 1. Poll MRCMDX (0x70) to see if packet is ready
    // We only need 2 bytes: 1 for the command, 1 to read the "Ready" byte.
    uint8_t mrcmdx_tx[3] = {DM9051_MRCMDX, 0, 0};
    uint8_t mrcmdx_rx[3] = {0};
    
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, mrcmdx_tx, mrcmdx_rx, 3, 3);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);

    uint8_t ready = mrcmdx_rx[2]; // The first byte after the command is the Ready byte
    
    if (ready != 0x01 && ready != 0x00) {
        // Error state, need to reset FIFO
        DM9051_WriteReg(hdm, DM9051_ISR, 0x80); // ISR_STOP_MRCMD
        
        // We should actually reset the pointer or MAC here if it's permanently stuck!
        // But for now, just clear the interrupt so it doesn't lock up.
        DM9051_WriteReg(hdm, DM9051_ISR, 0x80 | 0x01);
        
        return;
    }
    if (ready == 0x00) {
        // Empty
        DM9051_WriteReg(hdm, DM9051_ISR, 0x80 | 0x01); // Clear INT anyway
        return; // No packet
    }

    // 2. Read Rx Header (4 bytes) using MRCMD (0x72)
    hdm->rx_packet_ready = 1;

    uint8_t rx_hdr_tx[5] = {DM9051_MRCMD, 0, 0, 0, 0};
    uint8_t rx_hdr_rx[5] = {0};
    
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, rx_hdr_tx, rx_hdr_rx, 5, 100);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    
    // Stop memory read for now to process header
    DM9051_WriteReg(hdm, DM9051_ISR, 0x80); // ISR_STOP_MRCMD
    
    // rx_hdr_rx[1]=Ready Byte, rx_hdr_rx[2]=Status, rx_hdr_rx[3]=LenL, rx_hdr_rx[4]=LenH
    uint16_t rxlen = rx_hdr_rx[3] | (rx_hdr_rx[4] << 8);

    // Check for errors or huge packets
    if ((rx_hdr_rx[2] & 0xBF) || rxlen > 1536) { 
        // Bad packet, reset required
        return;
    }
    
    // 3. Start DMA for payload
    ref = NULL;
    if (hdm->lan_rx_queue) {
    	ref = ElasticQueue_Allocate(hdm->lan_rx_queue, rxlen);
    }
    if (hdm->lan_rx_queue && ref) {
        hdm->rx_dma_ready = false;
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
        uint8_t cmd = DM9051_MRCMD;
        HAL_SPI_Transmit(hdm->hspi, &cmd, 1, 100);
        HAL_SPI_Receive_DMA(hdm->hspi, ref->data, ref->len);
    } else {
        // TODO: this blocks, uses CPU - BAD
        // Dummy read to flush FIFO
        uint8_t dummy;
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
        uint8_t cmd = DM9051_MRCMD;
        HAL_SPI_Transmit(hdm->hspi, &cmd, 1, 100);
        for(uint16_t i = 0; i < rxlen; i++) {
            HAL_SPI_Receive(hdm->hspi, &dummy, 1, 100);
        }
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
        DM9051_WriteReg(hdm, DM9051_ISR, 0x80); // ISR_STOP_MRCMD
    }
}

// IRQ on end of transfer of Ethernet packet payload over SPI1 using DMA.
void DM9051_RxCpltCallback(DM9051_HandleTypeDef *hdm) {
    hdm->rx_dma_ready = true;

    // 1. Deassert CS
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    
    // 2. Clear MRCMD and Packet Received Interrupt
    DM9051_WriteReg(hdm, DM9051_ISR, 0x80 | 0x01); // ISR_STOP_MRCMD (bit 7) | ISR_PRS (bit 0)
    
    if (hdm->lan_rx_queue) {
        ElasticQueue_Commit(hdm->lan_rx_queue, ref);
    }
    ref = NULL;
}
