#include "usb_fs.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "wan.h"
#include <stdio.h>

#define DEMO_USB_MESSAGE_MAX_LEN 128U
static uint8_t demo_usb_message[DEMO_USB_MESSAGE_MAX_LEN];

void Demo_ReportSendingPacket(void)
{
  int message_len;
  if (USB_CDC_IsConfigured() == 0U) { return; }
  message_len = snprintf((char *)demo_usb_message,
                         sizeof(demo_usb_message),
                         "SENDING PACKET! %lu\r\n",
                         (unsigned long)g_eth_tx_done);
  if (message_len <= 0) { return; }
  if ((uint32_t)message_len >= sizeof(demo_usb_message)) {
    message_len = (int)(sizeof(demo_usb_message) - 1U);
  }
  (void)CDC_Transmit_HS(demo_usb_message, (uint16_t)message_len);
}

void Demo_ReportPacketCount(void)
{
  int message_len;
  if (USB_CDC_IsConfigured() == 0U) { return; }
  message_len = snprintf((char *)demo_usb_message,
                         sizeof(demo_usb_message),
                         "got packet %lu\r\n",
                         (unsigned long)demo_packet_count);
  if (message_len <= 0) { return; }
  if ((uint32_t)message_len >= sizeof(demo_usb_message)) {
    message_len = (int)(sizeof(demo_usb_message) - 1U);
  }
  (void)CDC_Transmit_HS(demo_usb_message, (uint16_t)message_len);
}

void Demo_ReportPacket(const ETH_BufferTypeDef *rx_packet)
{
  const uint8_t *frame;
  uint32_t frame_len;
  uint16_t ether_type;
  const char *type_name;
  int message_len;
  if ((rx_packet == NULL) || (rx_packet->buffer == NULL)) { return; }
  frame = rx_packet->buffer;
  frame_len = rx_packet->len;
  if (frame_len < 14U) { return; }
  ether_type = ((uint16_t)frame[12] << 8) | frame[13];
  type_name = Demo_EtherTypeName(ether_type);
  message_len = snprintf((char *)demo_usb_message,
                         sizeof(demo_usb_message),
                         "len=%lu type=0x%04x(%s) dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                         (unsigned long)frame_len,
                         ether_type,
                         type_name,
                         frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
                         frame[6], frame[7], frame[8], frame[9], frame[10], frame[11]);
  if ((message_len > 0) && (USB_CDC_IsConfigured() != 0U)) {
    if ((uint32_t)message_len >= sizeof(demo_usb_message)) {
      message_len = (int)(sizeof(demo_usb_message) - 1U);
    }
    (void)CDC_Transmit_HS(demo_usb_message, (uint16_t)message_len);
  }
}
