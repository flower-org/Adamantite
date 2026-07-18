#include "dm9051.h"
#include <string.h>

uint16_t DM9051_ReadPHY(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, uint8_t phy_reg)
{
  uint8_t spi_tx[2];
  uint8_t spi_rx[2];
  uint16_t phy_data = 0;

  // 1. Enable MAC to access PHY via EPAR (Register phy_reg)
  spi_tx[0] = 0x80 | 0x0C; spi_tx[1] = 0x40 | phy_reg;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 2. Issue PHY Read Command (EPCR)
  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x0C; // EPOS | ERPRR
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  HAL_Delay(1); // Wait for PHY read to complete

  // 3. Clear PHY Read Command
  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  // 4. Read PHY Data Low
  spi_tx[0] = 0x0D; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  phy_data = spi_rx[1];

  // 5. Read PHY Data High
  spi_tx[0] = 0x0E; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  phy_data |= (spi_rx[1] << 8);

  return phy_data;
}

void DM9051_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
  // DM9051 PHY Wake-Up & Read
  uint8_t spi_tx[2];
  uint8_t spi_rx[2];

  // 1. GPCR: GEP Output
  spi_tx[0] = 0x80 | 0x1E; spi_tx[1] = 0x01;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 2. GPR: Power ON internal PHY BEFORE core reset!
  spi_tx[0] = 0x80 | 0x1F; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  HAL_Delay(2); // Mandatory delay after PHY power on

  // 3. NCR: Core Reset
  spi_tx[0] = 0x80 | 0x00; spi_tx[1] = 0x01;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  HAL_Delay(2); // Wait for reset

  // 4. Clear NCR Reset
  spi_tx[0] = 0x80 | 0x00; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 5. Configure DM9051 Interrupts
  // Set Interrupt Polarity in INTCR (Register 0x39) to Active Low (0x00)
  spi_tx[0] = 0x80 | 0x39; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 6. Clear any pending interrupts in ISR (Register 0x7E)
  spi_tx[0] = 0x80 | 0x7E; spi_tx[1] = 0xFF;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 7. Enable interrupts in IMR (Register 0xFF)
  // 0x80 (SRAM pointer auto-return) | 0x01 (Packet Received) = 0x81
  spi_tx[0] = 0x80 | 0xFF; spi_tx[1] = 0x81;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

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

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm, uint8_t *data, uint16_t len) {
    // 1. Wait for NSR TX ready (TX1END or TX2END)
    // In a real application, you'd want a non-blocking timeout or interrupt driven approach
    while(!(DM9051_ReadReg(hdm, 0x01) & 0x0C)); 
    
    // 2. Prepare the DMA buffer
    hdm->tx_buf[0] = 0xF8; // MWCMD (Memory Write with SPI Write bit 0x80)
    memcpy(&hdm->tx_buf[1], data, len);
    hdm->tx_len = len;

    // 3. Start SPI DMA
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(hdm->hspi, hdm->tx_buf, len + 1);
}

void DM9051_TxCpltCallback(DM9051_HandleTypeDef *hdm) {
    // 1. Deassert CS
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);

    // 2. Write TX length
    DM9051_WriteReg(hdm, 0x7C, hdm->tx_len & 0xFF);        // TXPLL
    DM9051_WriteReg(hdm, 0x7D, (hdm->tx_len >> 8) & 0xFF); // TXPLH

    // 3. Issue TX request
    DM9051_WriteReg(hdm, 0x02, 0x01); // TCR_TXREQ
}

void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm) {
    // 1. Poll MRCMDX (0x70) to see if packet is ready
    uint8_t ready = DM9051_ReadReg(hdm, 0x70);
    if (ready != 0x01 && ready != 0x00) {
        // Error state, need to reset FIFO
        DM9051_WriteReg(hdm, 0x7E, 0x80); // ISR_STOP_MRCMD
        return;
    }
    if (ready == 0x00) return; // No packet

    // 2. Read Rx Header (4 bytes) using MRCMD (0x72)
    uint8_t rx_hdr_tx[5] = {0x72, 0, 0, 0, 0};
    uint8_t rx_hdr_rx[5] = {0};
    
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, rx_hdr_tx, rx_hdr_rx, 5, 100);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    
    // Stop memory read for now to process header
    DM9051_WriteReg(hdm, 0x7E, 0x80); // ISR_STOP_MRCMD
    
    // hdr_rx[0] is dummy. hdr_rx[1]=Ready Byte, hdr_rx[2]=Status, hdr_rx[3]=LenL, hdr_rx[4]=LenH
    uint16_t rxlen = rx_hdr_rx[3] | (rx_hdr_rx[4] << 8);
    
    // Check for errors or huge packets
    if ((rx_hdr_rx[2] & 0xBF) || rxlen > 1536) { 
        // Bad packet, reset required
        return; 
    }
    
    hdm->rx_len = rxlen;
    
    // 3. Start DMA for payload
    hdm->tx_buf[0] = 0x72; // Re-use tx_buf for the SPI dummy clocks
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive_DMA(hdm->hspi, hdm->tx_buf, hdm->rx_buf, rxlen + 1); // +1 for MRCMD byte
}

void DM9051_RxCpltCallback(DM9051_HandleTypeDef *hdm) {
    // 1. Deassert CS
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    
    // 2. Clear MRCMD
    DM9051_WriteReg(hdm, 0x7E, 0x80); // ISR_STOP_MRCMD
    
    // Packet is now in hdm->rx_buf[1] through hdm->rx_buf[rxlen]
}
