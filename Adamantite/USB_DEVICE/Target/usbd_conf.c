#include "usbd_conf.h"
#include "usbd_core.h"
#include "main.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (pcdHandle->Instance == USB_OTG_FS)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

    HAL_NVIC_SetPriority(OTG_FS_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{
  if (pcdHandle->Instance == USB_OTG_FS)
  {
    __HAL_RCC_USB_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
  }
}

void MX_USB_OTG_FS_PCD_Init(void)
{
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;

  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0U, 0x40U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1U, 0x80U);
  HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2U, 0x40U);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

  switch (hpcd->Init.speed)
  {
    case PCD_SPEED_HIGH:
      speed = USBD_SPEED_HIGH;
      break;
    case PCD_SPEED_FULL:
      speed = USBD_SPEED_FULL;
      break;
    default:
      speed = USBD_SPEED_FULL;
      break;
  }

  USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
  USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  MX_USB_OTG_FS_PCD_Init();

  hpcd_USB_OTG_FS.pData = pdev;
  pdev->pData = &hpcd_USB_OTG_FS;

  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  return (USBD_StatusTypeDef)HAL_PCD_DeInit((PCD_HandleTypeDef *)pdev->pData);
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  return (USBD_StatusTypeDef)HAL_PCD_Start((PCD_HandleTypeDef *)pdev->pData);
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  return (USBD_StatusTypeDef)HAL_PCD_Stop((PCD_HandleTypeDef *)pdev->pData);
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_Open((PCD_HandleTypeDef *)pdev->pData, ep_addr, ep_mps, ep_type);
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_Close((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_Flush((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_SetStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_ClrStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

  if ((ep_addr & 0x80U) == 0x80U)
  {
    return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
  }

  return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  return (USBD_StatusTypeDef)HAL_PCD_SetAddress((PCD_HandleTypeDef *)pdev->pData, dev_addr);
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_Transmit((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, size);
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  return (USBD_StatusTypeDef)HAL_PCD_EP_Receive((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, size);
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

void *USBD_static_malloc(uint32_t size)
{
  static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4U) + 1U];
  (void)size;
  return mem;
}

void USBD_static_free(void *p)
{
  (void)p;
}
