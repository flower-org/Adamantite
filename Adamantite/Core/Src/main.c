/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include "string.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ETH_RX_BUFFER_SIZE 1536U
#define DEMO_USB_MESSAGE_MAX_LEN 128U
#define DEMO_ETH_HEADER_LEN 14U
#define DEMO_ETH_SOURCE_MAC_OFFSET 6U
#define DEMO_ETH_MIN_RX_CHECK_LEN (DEMO_ETH_SOURCE_MAC_OFFSET + 6U)
#define DEMO_ETH_MAX_PAYLOAD_LEN 1500U
#define DEMO_ETH_TX_TIMEOUT_MS 20U
#define DEMO_ETHERTYPE_CUSTOM 0x88B5U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
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

/* USER CODE BEGIN PV */
static uint8_t EthRxBuffer[ETH_RX_DESC_CNT][ETH_RX_BUFFER_SIZE];
static ETH_BufferTypeDef EthRxBufferNodes[ETH_RX_DESC_CNT];
static uint8_t demo_usb_message[DEMO_USB_MESSAGE_MAX_LEN];
static uint8_t demo_eth_tx_frame[DEMO_ETH_HEADER_LEN + DEMO_ETH_MAX_PAYLOAD_LEN];
static const uint8_t demo_tx_payload[] = "adamantite-demo";
static uint8_t demo_tx_sent = 0U;
static uint64_t demo_packet_count = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_ETH_Init(void);
/* USER CODE BEGIN PFP */
static void Demo_ProcessLanPackets(void);
static void Demo_ReportPacketCount(void);
static void Demo_TrySendFrameFromRx(const ETH_BufferTypeDef *rx_packet);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
  static uint32_t rx_buffer_index = 0U;

  if (buff == NULL)
  {
    return;
  }

  *buff = EthRxBuffer[rx_buffer_index];
  rx_buffer_index = (rx_buffer_index + 1U) % ETH_RX_DESC_CNT;
}

void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t len)
{
  static uint32_t rx_node_index = 0U;
  ETH_BufferTypeDef *rx_buffer = &EthRxBufferNodes[rx_node_index];

  if ((pStart == NULL) || (pEnd == NULL) || (buff == NULL))
  {
    return;
  }

  rx_buffer->buffer = buff;
  rx_buffer->len = len;
  rx_buffer->next = NULL;

  if (*pStart == NULL)
  {
    *pStart = rx_buffer;
  }
  else
  {
    ((ETH_BufferTypeDef *)(*pEnd))->next = rx_buffer;
  }

  *pEnd = rx_buffer;
  rx_node_index = (rx_node_index + 1U) % ETH_RX_DESC_CNT;
}

static void Demo_ReportPacketCount(void)
{
  int message_len;

  if (USB_CDC_IsConfigured() == 0U)
  {
    return;
  }

  message_len = snprintf((char *)demo_usb_message,
                         sizeof(demo_usb_message),
                         "got packet %lu\r\n",
                         (unsigned long)demo_packet_count);

  if (message_len <= 0)
  {
    return;
  }

  if ((uint32_t)message_len >= sizeof(demo_usb_message))
  {
    message_len = (int)(sizeof(demo_usb_message) - 1U);
  }

  (void)CDC_Transmit_HS(demo_usb_message, (uint16_t)message_len);
}

static const char *Demo_EtherTypeName(uint16_t ether_type)
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

static void Demo_ReportPacket(const ETH_BufferTypeDef *rx_packet)
{
  const uint8_t *frame;
  uint32_t frame_len;
  uint16_t ether_type;
  const char *type_name;
  int message_len;

  if ((rx_packet == NULL) || (rx_packet->buffer == NULL))
  {
    return;
  }

  frame = rx_packet->buffer;
  frame_len = rx_packet->len;

  if (frame_len < 14U)
  {
    return;
  }

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

  if ((message_len > 0) && (USB_CDC_IsConfigured() != 0U))
  {
    if ((uint32_t)message_len >= sizeof(demo_usb_message))
    {
      message_len = (int)(sizeof(demo_usb_message) - 1U);
    }

    (void)CDC_Transmit_HS(demo_usb_message, (uint16_t)message_len);
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

  if ((destination_mac == NULL) || ((payload_len > 0U) && (payload == NULL)))
  {
    return DEMO_LAN_TX_INVALID_ARGUMENT;
  }

  if (payload_len > DEMO_ETH_MAX_PAYLOAD_LEN)
  {
    return DEMO_LAN_TX_INVALID_ARGUMENT;
  }

  frame_len = DEMO_ETH_HEADER_LEN + payload_len;

  memcpy(&demo_eth_tx_frame[0], destination_mac, 6U);
  memcpy(&demo_eth_tx_frame[6], heth.Init.MACAddr, 6U);
  demo_eth_tx_frame[12] = (uint8_t)(ether_type >> 8);
  demo_eth_tx_frame[13] = (uint8_t)(ether_type & 0xFFU);

  if (payload_len > 0U)
  {
    memcpy(&demo_eth_tx_frame[DEMO_ETH_HEADER_LEN], payload, payload_len);
  }

  tx_buffer.buffer = demo_eth_tx_frame;
  tx_buffer.len = frame_len;
  tx_buffer.next = NULL;

  TxConfig.Length = frame_len;
  TxConfig.TxBuffer = &tx_buffer;

  error_before = heth.ErrorCode;
  if (HAL_ETH_Transmit(&heth, &TxConfig, DEMO_ETH_TX_TIMEOUT_MS) == HAL_OK)
  {
    return DEMO_LAN_TX_OK;
  }

  if (((heth.ErrorCode & ~error_before) & HAL_ETH_ERROR_BUSY) != 0U)
  {
    return DEMO_LAN_TX_DESCRIPTOR_UNAVAILABLE;
  }

  return DEMO_LAN_TX_ERROR;
}

static void Demo_TrySendFrameFromRx(const ETH_BufferTypeDef *rx_packet)
{
  DemoLanTxStatus tx_status;
  const uint8_t *frame;
  const uint8_t *source_mac;

  if ((demo_tx_sent != 0U) || (rx_packet == NULL) || (rx_packet->buffer == NULL) ||
      (rx_packet->len < DEMO_ETH_MIN_RX_CHECK_LEN))
  {
    return;
  }

  frame = rx_packet->buffer;
  source_mac = &frame[DEMO_ETH_SOURCE_MAC_OFFSET];
  tx_status = Demo_SendRawEthernetFrame(source_mac,
                                        DEMO_ETHERTYPE_CUSTOM,
                                        demo_tx_payload,
                                        ((uint16_t)sizeof(demo_tx_payload) - 1U));

  if (tx_status == DEMO_LAN_TX_OK)
  {
    demo_tx_sent = 1U;
  }
}

/*static void Demo_ProcessLanPackets(void)
{
  void *rx_packet = NULL;

  while (HAL_ETH_ReadData(&heth, &rx_packet) == HAL_OK)
  {
    demo_packet_count++;
    HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_7);
    Demo_ReportPacketCount();
    rx_packet = NULL;
  }
}*/

static void Demo_ProcessLanPackets(void)
{
  void *rx_packet = NULL;

  while (HAL_ETH_ReadData(&heth, &rx_packet) == HAL_OK)
  {
    Demo_TrySendFrameFromRx((ETH_BufferTypeDef *)rx_packet);
    Demo_ReportPacket((ETH_BufferTypeDef *)rx_packet);
    rx_packet = NULL;
  }
}

/* USER CODE END 0 */
void LED_Init(void)
{
    // Enable GPIOG clock
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  LED_Init();             // initialize PG7 LED

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  MX_ETH_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_ETH_Start(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    Demo_ProcessLanPackets();
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = 64;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ETH Initialization Function
  * @param None
  * @retval None
  */
static void MX_ETH_Init(void)
{

  /* USER CODE BEGIN ETH_Init 0 */

  /* USER CODE END ETH_Init 0 */

   static uint8_t MACAddr[6];

  /* USER CODE BEGIN ETH_Init 1 */

  /* USER CODE END ETH_Init 1 */
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

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  memset(&TxConfig, 0 , sizeof(ETH_TxPacketConfig));
  TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
  TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
  TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;
  /* USER CODE BEGIN ETH_Init 2 */

  /* USER CODE END ETH_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
