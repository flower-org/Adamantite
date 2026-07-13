#ifndef __USB_FS_H
#define __USB_FS_H

#include <stdint.h>
#include "stm32h7xx_hal.h"

void Demo_ReportSendingPacket(void);
void Demo_ReportPacketCount(void);
void Demo_ReportPacket(uint8_t *frame, uint32_t frame_len, size_t active_queue_count);

void USB_FS_TxComplete(void);
void USB_FS_RxData(const uint8_t *Buf, uint32_t Len);

#endif /* __USB_FS_H */
