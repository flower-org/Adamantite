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
#include "wan.h"
#include "usb_fs.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "elastic_queue.h"

// Define custom sizes based on traffic volume
#define QUEUE_WAN_USB_SIZE 30720 // 30 KB for WAN and USB
#define QUEUE_WAN_USB_MAX_REFS 100
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
ElasticQueue_t lan_rx_queues[4];
ElasticQueue_t lan_tx_queues[4];
uint8_t lan_rx_areas[4][QUEUE_LAN_SIZE];
uint8_t lan_tx_areas[4][QUEUE_LAN_SIZE];
ElasticQueueRef_t lan_rx_refs[4][QUEUE_LAN_MAX_REFS];
ElasticQueueRef_t lan_tx_refs[4][QUEUE_LAN_MAX_REFS];
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DMA_HandleTypeDef hdma_memtomem_dma1_stream0;
DMA_HandleTypeDef hdma_memtomem_dma1_stream1;
DMA_HandleTypeDef hdma_memtomem_dma1_stream2;
DMA_HandleTypeDef hdma_memtomem_dma1_stream3;
DMA_HandleTypeDef hdma_memtomem_dma1_stream4;
DMA_HandleTypeDef hdma_memtomem_dma1_stream5;
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
/* USER CODE BEGIN PFP */
#include "dma_mem_to_mem.h"

// Wrapper contexts for each DMA stream
DmaMemToMem_t dma_ctx_stream0;
DmaMemToMem_t dma_ctx_stream1;
DmaMemToMem_t dma_ctx_stream2;
DmaMemToMem_t dma_ctx_stream3;
DmaMemToMem_t dma_ctx_stream4;
DmaMemToMem_t dma_ctx_stream5;

// HAL DMA callbacks
void HAL_DMA_XferCpltCallback_Stream0(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream0); }
void HAL_DMA_XferCpltCallback_Stream1(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream1); }
void HAL_DMA_XferCpltCallback_Stream2(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream2); }
void HAL_DMA_XferCpltCallback_Stream3(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream3); }
void HAL_DMA_XferCpltCallback_Stream4(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream4); }
void HAL_DMA_XferCpltCallback_Stream5(DMA_HandleTypeDef *hdma) { DmaMemToMem_TransferComplete(&dma_ctx_stream5); }
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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
  MX_DMA_Init();
  MX_USB_DEVICE_Init();
  MX_ETH_Init();
  /* USER CODE BEGIN 2 */
  // Initialize all Elastic Queues
  ElasticQueue_Init(&wan_rx_queue, wan_rx_area, QUEUE_WAN_USB_SIZE, wan_rx_refs, QUEUE_WAN_USB_MAX_REFS);
  ElasticQueue_Init(&wan_tx_queue, wan_tx_area, QUEUE_WAN_USB_SIZE, wan_tx_refs, QUEUE_WAN_USB_MAX_REFS);

  ElasticQueue_Init(&usb_rx_queue, usb_rx_area, QUEUE_WAN_USB_SIZE, usb_rx_refs, QUEUE_WAN_USB_MAX_REFS);
  ElasticQueue_Init(&usb_tx_queue, usb_tx_area, QUEUE_WAN_USB_SIZE, usb_tx_refs, QUEUE_WAN_USB_MAX_REFS);

  for (int i = 0; i < 4; i++) {
      ElasticQueue_Init(&lan_rx_queues[i], lan_rx_areas[i], QUEUE_LAN_SIZE, lan_rx_refs[i], QUEUE_LAN_MAX_REFS);
      ElasticQueue_Init(&lan_tx_queues[i], lan_tx_areas[i], QUEUE_LAN_SIZE, lan_tx_refs[i], QUEUE_LAN_MAX_REFS);
  }

  // Initialize DMA Wrappers
  DmaMemToMem_Init(&dma_ctx_stream0, &hdma_memtomem_dma1_stream0);
  DmaMemToMem_Init(&dma_ctx_stream1, &hdma_memtomem_dma1_stream1);
  DmaMemToMem_Init(&dma_ctx_stream2, &hdma_memtomem_dma1_stream2);
  DmaMemToMem_Init(&dma_ctx_stream3, &hdma_memtomem_dma1_stream3);
  DmaMemToMem_Init(&dma_ctx_stream4, &hdma_memtomem_dma1_stream4);
  DmaMemToMem_Init(&dma_ctx_stream5, &hdma_memtomem_dma1_stream5);

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
