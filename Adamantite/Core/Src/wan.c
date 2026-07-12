#include "wan.h"
#include "usb_fs.h"
#include "main.h"
#include <string.h>

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
#pragma location=0x30000000
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
#pragma location=0x30000080
ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x30000000))) ETH_DMADescTypeDef  DMARxDscrTab[ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30000080))) ETH_DMADescTypeDef  DMATxDscrTab[ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(".RxDescripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(".TxDescripSection")));   /* Ethernet Tx DMA Descriptors */
#endif

ETH_TxPacketConfig TxConfig;
ETH_HandleTypeDef heth;

static uint8_t EthRxBuffer[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE];
static ETH_BufferTypeDef EthRxBufferNodes[ETH_RX_DESC_CNT];
static uint8_t demo_eth_tx_frame[DEMO_ETH_HEADER_LEN + DEMO_ETH_MAX_PAYLOAD_LEN];
static const uint8_t demo_tx_payload[] = {'a', 'd', 'a', 'm', 'a', 'n', 't', 'i', 't', 'e', '-', 'd', 'e', 'm', 'o'};
static uint8_t demo_tx_sent = 0U;

uint64_t demo_packet_count = 0U;
volatile uint32_t packet_leases = 0;
volatile uint32_t g_eth_tx_done = 0;

void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  static uint32_t rx_buffer_index = 0U;
  if (buff == NULL) { return; }
  *buff = EthRxBuffer[rx_buffer_index];
  rx_buffer_index = (rx_buffer_index + 1U) % ETH_RX_DESC_CNT;
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t len)
{
  static uint32_t rx_node_index = 0U;
  ETH_BufferTypeDef *rx_buffer = &EthRxBufferNodes[rx_node_index];
  if ((pStart == NULL) || (pEnd == NULL) || (buff == NULL)) { return; }
  rx_buffer->buffer = buff;
  rx_buffer->len = len;
  rx_buffer->next = NULL;
  if (*pStart == NULL) {
    *pStart = rx_buffer;
  } else {
    ((ETH_BufferTypeDef *)(*pEnd))->next = rx_buffer;
  }
  *pEnd = rx_buffer;
  rx_node_index = (rx_node_index + 1U) % ETH_RX_DESC_CNT;
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth_ptr)
{
    (void)heth_ptr;
    g_eth_tx_done++;
    packet_leases-=1;
}

const char *Demo_EtherTypeName(uint16_t ether_type)
{
  switch (ether_type)
  {
    case 0x0800: return "IPv4";
    case 0x0806: return "ARP";
    case 0x86DD: return "IPv6";
    case 0x8100: return "VLAN";
    case 0x88CC: return "LLDP";
    default:     return "unknown";
  }
}

DemoLanTxStatus Demo_SendRawEthernetFrame(const uint8_t destination_mac[6],
                                          uint16_t ether_type,
                                          const uint8_t *payload,
                                          uint16_t payload_len)
{
  uint32_t error_before;
  uint16_t frame_len;
  ETH_BufferTypeDef tx_buffer;
  if ((destination_mac == NULL) || ((payload_len > 0U) && (payload == NULL))) {
    return DEMO_LAN_TX_INVALID_ARGUMENT;
  }
  if (payload_len > DEMO_ETH_MAX_PAYLOAD_LEN) {
    return DEMO_LAN_TX_INVALID_ARGUMENT;
  }
  frame_len = DEMO_ETH_HEADER_LEN + payload_len;
  memcpy(&demo_eth_tx_frame[0], destination_mac, DEMO_ETH_MAC_ADDR_LEN);
  memcpy(&demo_eth_tx_frame[DEMO_ETH_SOURCE_MAC_OFFSET], heth.Init.MACAddr, DEMO_ETH_MAC_ADDR_LEN);
  demo_eth_tx_frame[12] = (uint8_t)(ether_type >> 8);
  demo_eth_tx_frame[13] = (uint8_t)(ether_type & 0xFFU);
  if (payload_len > 0U) {
    memcpy(&demo_eth_tx_frame[DEMO_ETH_HEADER_LEN], payload, payload_len);
  }
  tx_buffer.buffer = demo_eth_tx_frame;
  tx_buffer.len = frame_len;
  tx_buffer.next = NULL;
  TxConfig.Length = frame_len;
  TxConfig.TxBuffer = &tx_buffer;
  error_before = heth.ErrorCode;
  if (HAL_ETH_Transmit_IT(&heth, &TxConfig) == HAL_OK) {
    return DEMO_LAN_TX_OK;
  }
  if (((heth.ErrorCode & ~error_before) & HAL_ETH_ERROR_BUSY) != 0U) {
    return DEMO_LAN_TX_DESCRIPTOR_UNAVAILABLE;
  }
  return DEMO_LAN_TX_ERROR;
}

static void Demo_TrySendFrameFromRx(const ETH_BufferTypeDef *rx_packet)
{
  Demo_ReportSendingPacket();
  DemoLanTxStatus tx_status;
  const uint8_t *frame;
  const uint8_t *source_mac;
  if ((rx_packet == NULL) || (rx_packet->buffer == NULL) || (rx_packet->len < DEMO_ETH_MIN_RX_CHECK_LEN)) {
    return;
  }
  frame = rx_packet->buffer;
  source_mac = &frame[DEMO_ETH_SOURCE_MAC_OFFSET];
  tx_status = Demo_SendRawEthernetFrame(source_mac,
                                        DEMO_ETHERTYPE_CUSTOM,
                                        demo_tx_payload,
                                        (uint16_t)sizeof(demo_tx_payload));
  if (tx_status == DEMO_LAN_TX_OK) {
    demo_tx_sent = 1U;
  }
}

void Demo_ProcessLanPackets(void)
{
  void *rx_packet = NULL;
  uint32_t desccount = 0;
  while (1) {
      if (packet_leases == 0) {
        HAL_ETH_Extension_ReturnRxDescriptor(&heth, desccount);
          if (HAL_ETH_ReadData(&heth, &rx_packet, &desccount) == HAL_OK) {
            packet_leases=2;
            Demo_TrySendFrameFromRx((ETH_BufferTypeDef *)rx_packet);
            Demo_ReportPacket((ETH_BufferTypeDef *)rx_packet);
            rx_packet = NULL;
            packet_leases-=1;
          }
      }
  }
}

void MX_ETH_Init(void)
{
  static uint8_t MACAddr[6];
  heth.Instance = ETH;
  MACAddr[0] = 0x00;
  MACAddr[1] = 0x80;
  MACAddr[2] = 0xE1;
  MACAddr[3] = 0x00;
  MACAddr[4] = 0x00;
  MACAddr[5] = 0x00;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
  heth.Init.TxDesc = DMATxDscrTab;
  heth.Init.RxDesc = DMARxDscrTab;
  heth.Init.RxBuffLen = ETH_RX_BUFFER_SIZE;
  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }
  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
}
