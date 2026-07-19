#ifndef __WAN_H
#define __WAN_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define DEMO_ETH_MAC_ADDR_LEN 6U
#define DEMO_ETH_HEADER_LEN 14U
#define DEMO_ETH_SOURCE_MAC_OFFSET DEMO_ETH_MAC_ADDR_LEN
#define DEMO_ETH_MIN_RX_CHECK_LEN (DEMO_ETH_SOURCE_MAC_OFFSET + DEMO_ETH_MAC_ADDR_LEN)
#define DEMO_ETH_MAX_PAYLOAD_LEN 1500U
#define DEMO_ETHERTYPE_CUSTOM 0x88B5U

typedef enum
{
  DEMO_LAN_TX_OK = 0,
  DEMO_LAN_TX_INVALID_ARGUMENT,
  DEMO_LAN_TX_DESCRIPTOR_UNAVAILABLE,
  DEMO_LAN_TX_ERROR
} DemoLanTxStatus;

extern ETH_HandleTypeDef heth;
extern ETH_TxPacketConfig TxConfig;
extern uint64_t demo_packet_count;
extern volatile uint32_t g_eth_tx_done;


void WAN_TriggerPacketRead(void);
DemoLanTxStatus Demo_SendRawEthernetFrame(const uint8_t destination_mac[6],
                                          uint16_t ether_type,
                                          const uint8_t *payload,
                                          uint16_t payload_len);
const char *Demo_EtherTypeName(uint16_t ether_type);

#endif /* __WAN_H */
