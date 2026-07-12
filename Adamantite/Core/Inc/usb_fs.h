#ifndef __USB_FS_H
#define __USB_FS_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

void Demo_ReportSendingPacket(void);
void Demo_ReportPacketCount(void);
void Demo_ReportPacket(const ETH_BufferTypeDef *rx_packet);

void USB_FS_TxComplete(void);
void USB_FS_RxData(const uint8_t *Buf, uint32_t Len);

#endif /* __USB_FS_H */
