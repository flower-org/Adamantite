#include "dm9051.h"

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

  // 5. Enable MAC to access PHY via EPAR (Register 0 BMCR)
  spi_tx[0] = 0x80 | 0x0C; spi_tx[1] = 0x40 | 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  // 6. Issue PHY Read Command (EPCR)
  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x0C; // EPOS | ERPRR
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  HAL_Delay(1); // Wait for PHY read to complete (Linux driver polls ERRE bit)

  // 7. Clear PHY Read Command
  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  // 8. Read PHY Data Low
  spi_tx[0] = 0x0D; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  volatile uint8_t bmcr_l = spi_rx[1];
  (void)bmcr_l;

  // 9. Read PHY Data High
  spi_tx[0] = 0x0E; spi_tx[1] = 0x00;
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  volatile uint8_t bmcr_h = spi_rx[1];
  (void)bmcr_h;

  // 10. Read PHY BMSR (Register 1)
  spi_tx[0] = 0x80 | 0x0C; spi_tx[1] = 0x40 | 0x01; // EPAR: PHY address 0x40 | Register 1
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);

  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x0C; // EPCR: Issue Read
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  HAL_Delay(1); // Wait

  spi_tx[0] = 0x80 | 0x0B; spi_tx[1] = 0x00; // EPCR: Clear Read
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(hspi, spi_tx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  
  spi_tx[0] = 0x0D; spi_tx[1] = 0x00; // Read Low
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  volatile uint8_t bmsr_l = spi_rx[1];
  (void)bmsr_l;

  spi_tx[0] = 0x0E; spi_tx[1] = 0x00; // Read High
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(hspi, spi_tx, spi_rx, 2, 100);
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  volatile uint8_t bmsr_h = spi_rx[1];
  (void)bmsr_h;

  // Breakpoint here!
}
