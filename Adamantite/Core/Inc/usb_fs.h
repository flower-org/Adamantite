#ifndef __USB_FS_H
#define __USB_FS_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

void Demo_ReportSendingPacket(void);
void Demo_ReportPacketCount(void);
void Demo_ReportPacket(const ETH_BufferTypeDef *rx_packet);

#endif /* __USB_FS_H */
