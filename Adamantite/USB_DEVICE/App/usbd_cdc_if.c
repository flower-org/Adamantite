#include "usbd_cdc_if.h"

#define APP_RX_DATA_SIZE  2048U
#define APP_TX_DATA_SIZE  2048U

uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
  return (int8_t)USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  (void)cmd;
  (void)pbuf;
  (void)length;
  return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  uint16_t txLen = (*Len > 0xFFFFU) ? 0xFFFFU : (uint16_t)(*Len);
  uint8_t txStatus = CDC_Transmit_FS(Buf, txLen);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);

  if (txStatus == (uint8_t)USBD_BUSY)
  {
    return (int8_t)USBD_BUSY;
  }

  if (txStatus == (uint8_t)USBD_FAIL)
  {
    return (int8_t)USBD_FAIL;
  }

  return (int8_t)USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

  if (hcdc == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  if (hcdc->TxState != 0U)
  {
    return (uint8_t)USBD_BUSY;
  }

  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
