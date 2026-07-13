#include "usb_fs.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "wan.h"
#include <stdio.h>
#include <string.h>

#define DEMO_USB_MESSAGE_MAX_LEN 128U
#define USB_TX_QUEUE_SIZE 16U
#define USB_RX_QUEUE_SIZE 8U

typedef struct {
    uint8_t data[DEMO_USB_MESSAGE_MAX_LEN];
    uint16_t len;
} UsbMessage_t;

static UsbMessage_t usb_tx_queue[USB_TX_QUEUE_SIZE];
static volatile uint32_t usb_tx_head = 0;
static volatile uint32_t usb_tx_tail = 0;
static volatile uint8_t usb_tx_busy = 0;

static UsbMessage_t usb_rx_queue[USB_RX_QUEUE_SIZE];
static volatile uint32_t usb_rx_head = 0;
static volatile uint32_t usb_rx_tail = 0;

static void USB_FS_SendNext(void)
{
  __disable_irq();
  if (usb_tx_busy) {
    __enable_irq();
    return;
  }
  if (usb_tx_head != usb_tx_tail) {
    usb_tx_busy = 1;
    uint16_t len = usb_tx_queue[usb_tx_tail].len;
    uint8_t *data = usb_tx_queue[usb_tx_tail].data;
    if (CDC_Transmit_HS(data, len) != USBD_OK) {
      usb_tx_busy = 0;
    }
  }
  __enable_irq();
}

void USB_FS_TxComplete(void)
{
  if (usb_tx_busy) {
    usb_tx_tail = (usb_tx_tail + 1U) % USB_TX_QUEUE_SIZE;
    usb_tx_busy = 0;
  }
  USB_FS_SendNext();
}

static void USB_FS_EnqueueMessage(const uint8_t *msg, uint16_t len)
{
  if (USB_CDC_IsConfigured() == 0U) { return; }
  if (len == 0U) { return; }
  if (len > DEMO_USB_MESSAGE_MAX_LEN) { len = DEMO_USB_MESSAGE_MAX_LEN; }

  uint32_t next_head = (usb_tx_head + 1U) % USB_TX_QUEUE_SIZE;
  if (next_head == usb_tx_tail) {
    return; // Queue full, drop packet
  }

  memcpy(usb_tx_queue[usb_tx_head].data, msg, len);
  usb_tx_queue[usb_tx_head].len = len;
  usb_tx_head = next_head;

  USB_FS_SendNext();
}

void USB_FS_RxData(const uint8_t *Buf, uint32_t Len)
{
  if (Len == 0U) { return; }
  
  // Truncate if packet is too large for our buffer
  if (Len > DEMO_USB_MESSAGE_MAX_LEN) {
    Len = DEMO_USB_MESSAGE_MAX_LEN; 
  }

  uint32_t next_head = (usb_rx_head + 1U) % USB_RX_QUEUE_SIZE;
  if (next_head == usb_rx_tail) {
    return; // Queue full, drop packet
  }

  memcpy(usb_rx_queue[usb_rx_head].data, Buf, Len);
  usb_rx_queue[usb_rx_head].len = (uint16_t)Len;
  usb_rx_head = next_head;
}

void Demo_ReportSendingPacket(void)
{
  char msg[DEMO_USB_MESSAGE_MAX_LEN];
  int message_len = snprintf(msg, sizeof(msg), "SENDING PACKET! %lu\r\n", (unsigned long)g_eth_tx_done);
  if (message_len > 0) {
    USB_FS_EnqueueMessage((uint8_t *)msg, (uint16_t)message_len);
  }
}

void Demo_ReportPacketCount(void)
{
  char msg[DEMO_USB_MESSAGE_MAX_LEN];
  int message_len = snprintf(msg, sizeof(msg), "got packet %lu\r\n", (unsigned long)demo_packet_count);
  if (message_len > 0) {
    USB_FS_EnqueueMessage((uint8_t *)msg, (uint16_t)message_len);
  }
}

extern uint64_t demo_allocate_attempts;
extern uint64_t demo_allocated;

void Demo_ReportPacket(uint8_t *frame, uint32_t frame_len, size_t active_queue_count)
{
  char msg[DEMO_USB_MESSAGE_MAX_LEN];
  uint16_t ether_type;
  const char *type_name;
  
  if (frame_len < 14U) { return; }
  
  ether_type = ((uint16_t)frame[12] << 8) | frame[13];
  type_name = Demo_EtherTypeName(ether_type);
  
  int message_len = snprintf(msg, sizeof(msg),
                         "len=%lu type=0x%04x(%s) dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x alloc=%lu/%lu q=%lu\r\n",
                         (unsigned long)frame_len, ether_type, type_name,
                         frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
                         frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
                         (unsigned long)demo_allocated, (unsigned long)demo_allocate_attempts, (unsigned long)active_queue_count);
  if (message_len > 0) {
    USB_FS_EnqueueMessage((uint8_t *)msg, (uint16_t)message_len);
  }
}
