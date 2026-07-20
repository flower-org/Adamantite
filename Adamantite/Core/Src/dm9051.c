#include "dm9051.h"
#include <string.h>
#include "log.h"

// TODO: change all transmits to interrupts for best CPU utilization?

// Global trash buffer for safely dropping packets via DMA without CPU blocking
extern uint8_t global_dma_trash_buffer[ETH_BUFFER_SIZE];
static uint32_t start_count;
static uint32_t end_count;
// Forward declarations for static MAC access functions
static void DM9051_WriteReg(DM9051_HandleTypeDef *hdm, uint8_t reg, uint8_t val);
static uint8_t DM9051_ReadReg(DM9051_HandleTypeDef *hdm, uint8_t reg);

// Write to MAC Register
static void DM9051_WriteReg(DM9051_HandleTypeDef *hdm, uint8_t reg, uint8_t val) {
    uint8_t spi_tx[2] = {0x80 | reg, val};
    uint8_t spi_rx[2] = {0};
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, spi_tx, spi_rx, 2, 10);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
}

// Read from MAC Register
static uint8_t DM9051_ReadReg(DM9051_HandleTypeDef *hdm, uint8_t reg) {
    uint8_t spi_tx[2] = {reg, 0x00};
    uint8_t spi_rx[2] = {0};
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, spi_tx, spi_rx, 2, 10);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    return spi_rx[1];
}

// ================================ INIT ================================

void DM9051_Init(DM9051_HandleTypeDef *hdm, uint8_t *mac_addr)
{
  hdm->dma_status = DM9051_DMA_READY;
  //hdm->can_write_packet = true;
  Log_Printf("init hdm->can_write_packet = true;\r\n");

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
  DM9051_WriteReg(hdm, DM9051_IMR, DM9051_IMR_PAR | DM9051_IMR_PTM | DM9051_IMR_PRM | 
                                   DM9051_IMR_LNKCHGM | DM9051_IMR_ROM | 
                                   DM9051_IMR_ROOM | DM9051_IMR_UDRUNM);

  // 10. Enable Receiver in RCR (Register 0x05)
  DM9051_WriteReg(hdm, DM9051_RCR, DM9051_RCR_DIS_LONG | DM9051_RCR_DIS_CRC | 
                             DM9051_RCR_ALL | DM9051_RCR_RUNT | 
                             DM9051_RCR_PRMSC | DM9051_RCR_RXEN);
}

// ================================ WRITE ================================

void DM9051_WritePacket_DMA(DM9051_HandleTypeDef *hdm) {
    if (hdm->dma_status != DM9051_DMA_READY) { return; }
    //if (!hdm->can_write_packet) {
    	// we need this crutch since INT is not always called correctly
        if (!(DM9051_ReadReg(hdm, DM9051_NSR) & 0x0C)) {
        	return;
        }
        //hdm->can_write_packet = true;
    //}

    uint8_t *data;
    size_t len;
    if (ElasticQueue_Lock(hdm->lan_tx_queue, 1, &data, &len) != ELASTIC_QUEUE_OK) {
        return;
    }

    // if (!(DM9051_ReadReg(hdm, DM9051_NSR) & 0x0C)) return;

    //hdm->can_write_packet = false;
    Log_Printf("111 hdm->can_write_packet = false;\r\n");
    
    // 2. Prepare the DMA buffer by sending MWCMD first
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    uint8_t cmd = 0xF8; // MWCMD (Memory Write with SPI Write bit 0x80)
    uint8_t cmd_rx;
    HAL_SPI_TransmitReceive(hdm->hspi, &cmd, &cmd_rx, 1, 10);

    // 3. Start SPI DMA for the payload
    hdm->dma_status = DM9051_DMA_TX;
    Log_Printf("DM9051 Transmit\r\n");
    HAL_SPI_TransmitReceive_DMA(hdm->hspi, data, global_dma_trash_buffer, len);
}

// ================================ INTERRUPT ================================

void DM9051_ProcessInterrupt(DM9051_HandleTypeDef *hdm) {
	return;
    /*uint8_t isr = DM9051_ReadReg(hdm, DM9051_ISR);

    uint8_t clear_isr = 0;

    // Interrupt flags
    bool packet_received_flag = isr & DM9051_ISR_PRS;
    bool packet_transmitted_flag = isr & DM9051_ISR_PTS;
    bool link_status_changed_flag = isr & DM9051_ISR_LNKCHG;
    bool receive_overflow_flag = isr & DM9051_ISR_ROS;
    bool overflow_counter_overflow_flag = isr & DM9051_ISR_ROOS;
    bool transmit_under_run_flag = isr & DM9051_ISR_UDRUN;

    // Link Status Change
    if (link_status_changed_flag) {
        Log_Printf("DM9051 Link Status Changed\r\n");
        clear_isr |= DM9051_ISR_LNKCHG;
    }

    // Receive Overflow
    if (receive_overflow_flag) {
        Log_Printf("DM9051 Receive Overflow!\r\n");
        clear_isr |= DM9051_ISR_ROS;
    }

    // Receive Overflow Counter Overflow
    if (overflow_counter_overflow_flag) {
        Log_Printf("DM9051 Receive Overflow Counter Overflow!\r\n");
        clear_isr |= DM9051_ISR_ROOS;
    }

    // Transmit Under-run
    if (transmit_under_run_flag) {
        Log_Printf("DM9051 Transmit Under-run!\r\n");
        clear_isr |= DM9051_ISR_UDRUN;
    }

    // Packet Transmitted
    if (packet_transmitted_flag) {
        Log_Printf("DM9051 Packet Transmitted\r\n");
        clear_isr |= DM9051_ISR_PTS;

        if (!hdm->can_write_packet) {
            hdm->can_write_packet = true;
            Log_Printf("222 hdm->can_write_packet = true;\r\n");
        } else {
            hdm->can_write_packet = true;
            Log_Printf("333 CAN NO DO hdm->can_write_packet = true;\r\n");
        }
    }

    // Packet Received (starts DMA, so we must clear other ISR flags BEFORE this)
    if (packet_received_flag) {
    	Log_Printf("DM9051 Packet Received\r\n");
    	// don't clear PRS here, or else DM9051_MRCMDX in DM9051_ReadPacket_DMA_Start will return {0,0,0}
        //clear_isr |= DM9051_ISR_PRS;

        //hdm->can_read_packet = true;
    }

    // clear interrupt
    if (clear_isr) {
        DM9051_WriteReg(hdm, DM9051_ISR, clear_isr);
    }*/
}

// ================================ READ ================================

void DM9051_ReadPacket_DMA_Start(DM9051_HandleTypeDef *hdm) {
    if (hdm->dma_status != DM9051_DMA_READY) { return; }
    //if (!hdm->can_read_packet) {
        // 1. Poll MRCMDX (0x70) to see if packet is ready
        // We only need 2 bytes: 1 for the command, 1 to read the "Ready" byte.
        uint8_t mrcmdx_tx[3] = {DM9051_MRCMDX, 0, 0};
        uint8_t mrcmdx_rx[3] = {0};

        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
        HAL_SPI_TransmitReceive(hdm->hspi, mrcmdx_tx, mrcmdx_rx, 3, 10);
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);

        uint8_t ready = mrcmdx_rx[2]; // The first byte after the command is the Ready byte
        
        if (ready != 0x01 && ready != 0x00) {
            // Error state, need to reset FIFO
            DM9051_WriteReg(hdm, DM9051_ISR, DM9051_ISR_IOMODE); // ISR_STOP_MRCMD

            // We should actually reset the pointer or MAC here if it's permanently stuck!
            // But for now, just clear the interrupt so it doesn't lock up.
            DM9051_WriteReg(hdm, DM9051_ISR, DM9051_ISR_IOMODE | DM9051_ISR_PRS);

            return;
        }
        if (ready == 0x00) {
            // Empty
        	// TODO: do we need to clear here?
            DM9051_WriteReg(hdm, DM9051_ISR, DM9051_ISR_IOMODE | DM9051_ISR_PRS); // Clear INT anyway
            return; // No packet
        }
    //}

    // 2. Read Rx Header (4 bytes) using MRCMD (0x72)
    //hdm->can_read_packet = 0;

    uint8_t rx_hdr_tx[5] = {DM9051_MRCMD, 0, 0, 0, 0};
    uint8_t rx_hdr_rx[5] = {0};
    
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hdm->hspi, rx_hdr_tx, rx_hdr_rx, 5, 10);
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);
    
    // Stop memory read for now to process header
    DM9051_WriteReg(hdm, DM9051_ISR, DM9051_ISR_IOMODE); // ISR_STOP_MRCMD
    
    // rx_hdr_rx[1]=Ready Byte, rx_hdr_rx[2]=Status, rx_hdr_rx[3]=LenL, rx_hdr_rx[4]=LenH
    uint16_t rxlen = rx_hdr_rx[3] | (rx_hdr_rx[4] << 8);

    // Check for errors or huge packets
    if ((rx_hdr_rx[2] & 0xBF) || rxlen > ETH_BUFFER_SIZE) {
        // Bad packet, reset required
        return;
    }
    
    // 3. Start DMA for payload
    hdm->current_rx_packet = NULL;
    if (hdm->lan_rx_queue) {
        hdm->current_rx_packet = ElasticQueue_Allocate(hdm->lan_rx_queue, rxlen);
    }
    if (hdm->lan_rx_queue && hdm->current_rx_packet) {
        hdm->dma_status = DM9051_DMA_RX;
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
        uint8_t cmd = DM9051_MRCMD;
        uint8_t cmd_rx;
        HAL_SPI_TransmitReceive(hdm->hspi, &cmd, &cmd_rx, 1, 10);
        HAL_SPI_TransmitReceive_DMA(hdm->hspi, global_dma_trash_buffer, hdm->current_rx_packet->data, hdm->current_rx_packet->len);
        //Log_Printf("HAL_SPI_TransmitReceive_DMA! %d (NORMAL)\r\n", ++start_count);
    } else {
        // Drop packet using DMA to a global trash buffer to save CPU cycles
        hdm->dma_status = DM9051_DMA_RX;
        
        HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_RESET);
        uint8_t cmd = DM9051_MRCMD;
        uint8_t cmd_rx;
        HAL_SPI_TransmitReceive(hdm->hspi, &cmd, &cmd_rx, 1, 10);
        
        // Start DMA transfer into the global trash buffer
        HAL_SPI_TransmitReceive_DMA(hdm->hspi, global_dma_trash_buffer, global_dma_trash_buffer, rxlen);
        //Log_Printf("HAL_SPI_TransmitReceive_DMA! %d (DROP)\r\n", ++start_count);
    }
}

// ================================ SPI DMA Callback ================================

// IRQ on end of transfer of Ethernet packet payload over SPI1 using DMA.
void DM9051_TxRxCpltCallback(DM9051_HandleTypeDef *hdm) {
	bool is_tx_callback = hdm->dma_status == DM9051_DMA_TX;
    hdm->dma_status = DM9051_DMA_READY;

    // 1. Deassert CS
    HAL_GPIO_WritePin(hdm->cs_port, hdm->cs_pin, GPIO_PIN_SET);

    if (is_tx_callback) {
    	// TX done
        ElasticQueueRef_t* ref = ElasticQueue_PeekLocked(hdm->lan_tx_queue);
        if (ref) {
            // 2. Write TX length
            DM9051_WriteReg(hdm, DM9051_TXPLL, ref->len & 0xFF);        // TXPLL
            DM9051_WriteReg(hdm, DM9051_TXPLH, (ref->len >> 8) & 0xFF); // TXPLH

            // 3. Issue TX request
            DM9051_WriteReg(hdm, DM9051_TCR, 0x01); // TCR_TXREQ

            ElasticQueue_Done(hdm->lan_tx_queue);
        }
    } else {
    	// RX done
        // 2. Clear MRCMD and Packet Received Interrupt
        DM9051_WriteReg(hdm, DM9051_ISR, DM9051_ISR_IOMODE | DM9051_ISR_PRS); // ISR_STOP_MRCMD | ISR_PRS

        if (hdm->lan_rx_queue && hdm->current_rx_packet) {
            ElasticQueue_Commit(hdm->lan_rx_queue, hdm->current_rx_packet);
            hdm->current_rx_packet = NULL;
        }
    }
}
