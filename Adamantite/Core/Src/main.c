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
#include "string.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "wan.h"
#include <stdio.h>
#include "usb_fs.h"
#include "dm9051.h"
#include "log.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "elastic_queue.h"
#include "dma_mem_to_mem.h"

// Define custom sizes based on traffic volume
#define QUEUE_WAN_USB_SIZE 30720 // 30 KB for WAN and USB
#define QUEUE_WAN_USB_MAX_REFS 100
#define LAN_COUNT     	   4
#define INTERFACE_COUNT    6
#define QUEUE_LAN_SIZE     15360 // 15 KB for LAN ports
#define QUEUE_LAN_MAX_REFS     50

// WAN Queues (High Traffic)
ElasticQueue_t wan_rx_queue;
ElasticQueue_t wan_tx_queue;
uint8_t wan_rx_area[QUEUE_WAN_USB_SIZE];
uint8_t wan_tx_area[QUEUE_WAN_USB_SIZE];
ElasticQueueRef_t wan_rx_refs[QUEUE_WAN_USB_MAX_REFS];
ElasticQueueRef_t wan_tx_refs[QUEUE_WAN_USB_MAX_REFS];

// USB Queues (High Traffic - Sniffer)
ElasticQueue_t usb_rx_queue;
ElasticQueue_t usb_tx_queue;
uint8_t usb_rx_area[QUEUE_WAN_USB_SIZE];
uint8_t usb_tx_area[QUEUE_WAN_USB_SIZE];
ElasticQueueRef_t usb_rx_refs[QUEUE_WAN_USB_MAX_REFS];
ElasticQueueRef_t usb_tx_refs[QUEUE_WAN_USB_MAX_REFS];

// LAN Queues (4 Ports - Lower Traffic per port)
ElasticQueue_t lan_rx_queues[LAN_COUNT];
ElasticQueue_t lan_tx_queues[LAN_COUNT];
uint8_t lan_rx_areas[LAN_COUNT][QUEUE_LAN_SIZE];
uint8_t lan_tx_areas[LAN_COUNT][QUEUE_LAN_SIZE];
ElasticQueueRef_t lan_rx_refs[LAN_COUNT][QUEUE_LAN_MAX_REFS];
ElasticQueueRef_t lan_tx_refs[LAN_COUNT][QUEUE_LAN_MAX_REFS];
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
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

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;

DMA_HandleTypeDef hdma_memtomem_dma1_stream0;
DMA_HandleTypeDef hdma_memtomem_dma1_stream1;
DMA_HandleTypeDef hdma_memtomem_dma1_stream2;
DMA_HandleTypeDef hdma_memtomem_dma1_stream3;
DMA_HandleTypeDef hdma_memtomem_dma1_stream4;
DMA_HandleTypeDef hdma_memtomem_dma1_stream5;
/* USER CODE BEGIN PV */
uint8_t global_dma_trash_buffer[ETH_BUFFER_SIZE];

DM9051_HandleTypeDef hdm9051_1;
uint8_t dm9051_mac[6] = {0x02, 0x00, 0x00, 0x11, 0x22, 0x33}; // Locally administered MAC

// NULLs allowed in this array
DM9051_HandleTypeDef* lan_dm9051_interfaces[LAN_COUNT] = { &hdm9051_1, NULL, NULL, NULL };

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ETH_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
#include "dma_mem_to_mem.h"

// Wrapper contexts for each DMA stream
DmaMemToMem_t wan_rx_dma_ctx_stream;
DmaMemToMem_t usb_rx_dma_ctx_stream;
DmaMemToMem_t lan1_rx_dma_ctx_stream;
DmaMemToMem_t lan2_rx_dma_ctx_stream;
DmaMemToMem_t lan3_rx_dma_ctx_stream;
DmaMemToMem_t lan4_rx_dma_ctx_stream;

DmaMemToMem_t* const all_dma_streams[INTERFACE_COUNT] = {
    &wan_rx_dma_ctx_stream,
    &usb_rx_dma_ctx_stream,
    &lan1_rx_dma_ctx_stream,
    &lan2_rx_dma_ctx_stream,
    &lan3_rx_dma_ctx_stream,
    &lan4_rx_dma_ctx_stream
};

DmaMemToMem_t* const lan_dma_streams[LAN_COUNT] = {
    &lan1_rx_dma_ctx_stream,
    &lan2_rx_dma_ctx_stream,
    &lan3_rx_dma_ctx_stream,
    &lan4_rx_dma_ctx_stream
};

// HAL DMA callbacks
void HAL_DMA_XferCpltCallback_Stream0(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&wan_rx_dma_ctx_stream); }
void HAL_DMA_XferCpltCallback_Stream1(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&usb_rx_dma_ctx_stream); }
void HAL_DMA_XferCpltCallback_Stream2(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&lan1_rx_dma_ctx_stream); }
void HAL_DMA_XferCpltCallback_Stream3(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&lan2_rx_dma_ctx_stream); }
void HAL_DMA_XferCpltCallback_Stream4(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&lan3_rx_dma_ctx_stream); }
void HAL_DMA_XferCpltCallback_Stream5(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&lan4_rx_dma_ctx_stream); }
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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

void main_loop(void) {
	/* Infinite loop */
	while (1)
	{
		// 1) DMA RX-to-TX start. Trigger: can lock RX queue.
		if (ElasticQueue_IsLockable(&wan_rx_queue)) {
		    if (DmaMemToMem_IsReady(&wan_rx_dma_ctx_stream)) {
	            uint8_t *out_buf;
	            size_t out_len;
	            if (ElasticQueue_Lock(&wan_rx_queue, 2, &out_buf, &out_len) == ELASTIC_QUEUE_OK) {
	                ElasticQueue_t *dests[] = { &usb_tx_queue, &lan_tx_queues[0] };
	                DmaMemToMem_StartBroadcast(&wan_rx_dma_ctx_stream, dests, 2);
	            }
		    }
		}
		if (ElasticQueue_IsLockable(&usb_rx_queue)) {
			// TODO: implement
		}
		for (int i = 0; i < LAN_COUNT; i++) {
			if (ElasticQueue_IsLockable(&lan_rx_queues[i])) {
				if (lan_dm9051_interfaces[i] != NULL) {
				    if (DmaMemToMem_IsReady(lan_dma_streams[i])) {
			            uint8_t *out_buf;
			            size_t out_len;
			            if (ElasticQueue_Lock(&lan_rx_queues[i], 2, &out_buf, &out_len) == ELASTIC_QUEUE_OK) {
			                ElasticQueue_t *dests[] = { &usb_tx_queue, &wan_tx_queue };
			                DmaMemToMem_StartBroadcast(lan_dma_streams[i], dests, 2);
			            }
				    }
				} // TODO: else try ENCJ?
			}
		}

		// ----------------------------------------------------------

		// 2) DMA RX-to-TX continue. Trigger: DmaMemToMem_t in state DMA_STATE_TRANSFER_DONE.
		for (int i = 0; i < INTERFACE_COUNT; i++) {
			if (all_dma_streams[i]->state == DMA_STATE_TRANSFER_DONE) {
				DmaMemToMem_Process(all_dma_streams[i]);
			}
		}

		// ----------------------------------------------------------

		// 3) HW to RX. Trigger: flag set by HW IRQ, implementation may vary for HW.
		WAN_TriggerPacketRead();

		for (int i = 0; i < LAN_COUNT; i++) {
    		DM9051_HandleTypeDef* lan_dm9051 = lan_dm9051_interfaces[i];
		    if (lan_dm9051 && lan_dm9051->interrupt_pending && lan_dm9051->dma_ready) {
                DM9051_ProcessInterrupt(lan_dm9051);
		    }
		}

        /*// 4) Check for MAC/DMA silent death and recover
        if (heth.gState == HAL_ETH_STATE_ERROR) {
            Log_Printf("ETH DEAD! err=0x%lx dma=0x%lx\r\n", 
                       (unsigned long)heth.ErrorCode, (unsigned long)heth.DMAErrorCode);
                       
            // Try to force restart
            HAL_ETH_Stop(&heth);
            HAL_ETH_Start_IT(&heth);
        } else if (heth.gState == HAL_ETH_STATE_STARTED && (heth.Instance->DMACSR & ETH_DMACSR_RBU)) {
            // Stuck in RBU without HAL knowing? (Shouldn't happen, but just in case)
            heth.Instance->DMACSR = ETH_DMACSR_RBU; // Clear flag
            // Dummy write to tail pointer to wake up DMA
            heth.Instance->DMACRDTPR = 0;
        }*/

		// TODO: implement the rest

		// ----------------------------------------------------------

		// 4) TX to HW. Trigger: can lock TX queue, implementation may vary for HW.
		if (ElasticQueue_IsLockable(&wan_tx_queue)) {
			// TODO: implement
            uint8_t *out_buf;
            size_t out_len;
            if (ElasticQueue_Lock(&wan_tx_queue, 1, &out_buf, &out_len) == ELASTIC_QUEUE_OK) {
                // Drops packet
                ElasticQueue_Abort(&wan_tx_queue);
            }
		}
		if (ElasticQueue_IsLockable(&usb_tx_queue)) {
            uint8_t *out_buf;
            size_t out_len;
            if (ElasticQueue_Lock(&usb_tx_queue, 1, &out_buf, &out_len) == ELASTIC_QUEUE_OK) {
                // TODO: DMA to USB
            	Demo_ReportPacket(out_buf, out_len, wan_rx_queue.num_refs);
                ElasticQueue_Done(&usb_tx_queue);
            }
		}
		for (int i = 0; i < LAN_COUNT; i++) {
            DM9051_HandleTypeDef* lan_dm9051 = lan_dm9051_interfaces[i];
            if (lan_dm9051) {
    			if (lan_dm9051->dma_ready && ElasticQueue_IsLockable(lan_dm9051->lan_tx_queue)) {
					DM9051_WritePacket_DMA(lan_dm9051_interfaces[i]);
    			}
			} // else try ENCJ?
		}
	}
}
/* USER CODE END 0 */

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

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  MX_ETH_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  // Initialize all Elastic Queues
  ElasticQueue_Init(&wan_rx_queue, wan_rx_area, QUEUE_WAN_USB_SIZE, wan_rx_refs, QUEUE_WAN_USB_MAX_REFS);
  ElasticQueue_Init(&wan_tx_queue, wan_tx_area, QUEUE_WAN_USB_SIZE, wan_tx_refs, QUEUE_WAN_USB_MAX_REFS);

  ElasticQueue_Init(&usb_rx_queue, usb_rx_area, QUEUE_WAN_USB_SIZE, usb_rx_refs, QUEUE_WAN_USB_MAX_REFS);
  ElasticQueue_Init(&usb_tx_queue, usb_tx_area, QUEUE_WAN_USB_SIZE, usb_tx_refs, QUEUE_WAN_USB_MAX_REFS);

  for (int i = 0; i < LAN_COUNT; i++) {
      ElasticQueue_Init(&lan_rx_queues[i], lan_rx_areas[i], QUEUE_LAN_SIZE, lan_rx_refs[i], QUEUE_LAN_MAX_REFS);
      ElasticQueue_Init(&lan_tx_queues[i], lan_tx_areas[i], QUEUE_LAN_SIZE, lan_tx_refs[i], QUEUE_LAN_MAX_REFS);
  }

  hdm9051_1.hspi = &hspi1;
  hdm9051_1.hdma_rx = &hdma_spi1_rx;
  hdm9051_1.hdma_tx = &hdma_spi1_tx;
  hdm9051_1.cs_port = GPIOA;
  hdm9051_1.cs_pin = GPIO_PIN_4;
  hdm9051_1.lan_rx_queue = &lan_rx_queues[0]; // Assuming DM9051 maps to LAN Port 1
  hdm9051_1.lan_tx_queue = &lan_tx_queues[0];
  DM9051_Init(&hdm9051_1, dm9051_mac);

  // Initialize DMA Wrappers
  DmaMemToMem_Init(&wan_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream0, &wan_rx_queue);
  DmaMemToMem_Init(&usb_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream1, &usb_rx_queue);
  DmaMemToMem_Init(&lan1_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream2, &lan_rx_queues[0]);
  DmaMemToMem_Init(&lan2_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream3, &lan_rx_queues[1]);
  DmaMemToMem_Init(&lan3_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream4, &lan_rx_queues[2]);
  DmaMemToMem_Init(&lan4_rx_dma_ctx_stream, &hdma_memtomem_dma1_stream5, &lan_rx_queues[3]);

  // Register HAL completion callbacks
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream0, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream0);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream1, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream1);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream2, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream2);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream3, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream3);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream4, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream4);
  HAL_DMA_RegisterCallback(&hdma_memtomem_dma1_stream5, HAL_DMA_XFER_CPLT_CB_ID, HAL_DMA_XferCpltCallback_Stream5);

  // Start ETH in interrupt mode
  if (HAL_ETH_Start_IT(&heth) != HAL_OK)
  {
    Error_Handler();
  }

  // This is needed for sniffer
  ETH_MACFilterConfigTypeDef filter;
  HAL_ETH_GetMACFilterConfig(&heth, &filter);
  filter.PromiscuousMode = ENABLE;
  filter.ReceiveAllMode  = ENABLE;
  HAL_ETH_SetMACFilterConfig(&heth, &filter);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    main_loop();
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 44;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  heth.Init.RxBuffLen = 1536;

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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * Enable DMA controller clock
  * Configure DMA for memory to memory transfers
  *   hdma_memtomem_dma1_stream0
  *   hdma_memtomem_dma1_stream1
  *   hdma_memtomem_dma1_stream2
  *   hdma_memtomem_dma1_stream3
  *   hdma_memtomem_dma1_stream4
  *   hdma_memtomem_dma1_stream5
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* Configure DMA request hdma_memtomem_dma1_stream0 on DMA1_Stream0 */
  hdma_memtomem_dma1_stream0.Instance = DMA1_Stream0;
  hdma_memtomem_dma1_stream0.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream0.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream0.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream0.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream0.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream0.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream0.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream0.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream0.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream0) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_stream1 on DMA1_Stream1 */
  hdma_memtomem_dma1_stream1.Instance = DMA1_Stream1;
  hdma_memtomem_dma1_stream1.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream1.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream1.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream1.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream1.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream1.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream1.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream1.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream1.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream1.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream1) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_stream2 on DMA1_Stream2 */
  hdma_memtomem_dma1_stream2.Instance = DMA1_Stream2;
  hdma_memtomem_dma1_stream2.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream2.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream2.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream2.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream2.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream2.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream2.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream2.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream2.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream2.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream2.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream2.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream2) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_stream3 on DMA1_Stream3 */
  hdma_memtomem_dma1_stream3.Instance = DMA1_Stream3;
  hdma_memtomem_dma1_stream3.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream3.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream3.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream3.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream3.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream3.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream3.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream3.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream3.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream3.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream3.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream3.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream3) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_stream4 on DMA1_Stream4 */
  hdma_memtomem_dma1_stream4.Instance = DMA1_Stream4;
  hdma_memtomem_dma1_stream4.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream4.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream4.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream4.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream4.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream4.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream4.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream4.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream4.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream4.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream4.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream4.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream4) != HAL_OK)
  {
    Error_Handler( );
  }

  /* Configure DMA request hdma_memtomem_dma1_stream5 on DMA1_Stream5 */
  hdma_memtomem_dma1_stream5.Instance = DMA1_Stream5;
  hdma_memtomem_dma1_stream5.Init.Request = DMA_REQUEST_MEM2MEM;
  hdma_memtomem_dma1_stream5.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma1_stream5.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma1_stream5.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma1_stream5.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream5.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_memtomem_dma1_stream5.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma1_stream5.Init.Priority = DMA_PRIORITY_LOW;
  hdma_memtomem_dma1_stream5.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma1_stream5.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma1_stream5.Init.MemBurst = DMA_MBURST_SINGLE;
  hdma_memtomem_dma1_stream5.Init.PeriphBurst = DMA_PBURST_SINGLE;
  if (HAL_DMA_Init(&hdma_memtomem_dma1_stream5) != HAL_OK)
  {
    Error_Handler( );
  }

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  /* DMA1_Stream7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_3) { // PA3 is DM9051_1 INT
        hdm9051_1.interrupt_pending = 1;
        //Log_Printf("DM9051 EXTI\r\n");
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI1) {
        DM9051_TxCpltCallback(&hdm9051_1);
        //Log_Printf("DM9051 TX Cplt\r\n");
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI1) {
        //Log_Printf("SPI Error! ErrorCode: 0x%08lX\r\n", hspi->ErrorCode);
        
        // If it is a DMA-related error (bit 4 set)
        //if (hspi->ErrorCode & 0x00000010UL) { // HAL_SPI_ERROR_DMA
            if (hspi->hdmatx != NULL && hspi->hdmatx->ErrorCode != 0) {
                //Log_Printf("TX DMA Error: 0x%08lX\r\n", hspi->hdmatx->ErrorCode);
            }
            if (hspi->hdmarx != NULL && hspi->hdmarx->ErrorCode != 0) {
                //Log_Printf("RX DMA Error: 0x%08lX\r\n", hspi->hdmarx->ErrorCode);
            }
        //}
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI1) {
        DM9051_RxCpltCallback(&hdm9051_1);
        //Log_Printf("DM9051 RX Cplt\r\n");
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi->Instance == SPI1) {
        DM9051_RxCpltCallback(&hdm9051_1);
        //Log_Printf("DM9051 TXRX Cplt\r\n");
    }
}

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
