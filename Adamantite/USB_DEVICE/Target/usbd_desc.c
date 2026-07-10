#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID                     0x0483U
#define USBD_PID_FS                  0x5740U
#define USBD_LANGID_STRING           0x409U
#define USBD_MANUFACTURER_STRING     "STMicroelectronics"
#define USBD_PRODUCT_STRING_FS       "STM32 Virtual ComPort"
#define USBD_CONFIGURATION_STRING_FS "CDC Config"
#define USBD_INTERFACE_STRING_FS     "CDC Interface"

#define USB_LEN_DEV_DESC             18U
#define USB_LEN_LANGID_STR_DESC      4U
#define USB_MAX_EP0_SIZE             64U
#define USBD_SERIAL_STRING           "00000000001A"

static uint8_t USBD_FS_DeviceDesc[USB_LEN_DEV_DESC] = {
  0x12,
  USB_DESC_TYPE_DEVICE,
  0x00,
  0x02,
  0x02,
  0x00,
  0x00,
  USB_MAX_EP0_SIZE,
  LOBYTE(USBD_VID),
  HIBYTE(USBD_VID),
  LOBYTE(USBD_PID_FS),
  HIBYTE(USBD_PID_FS),
  0x00,
  0x02,
  USBD_IDX_MFC_STR,
  USBD_IDX_PRODUCT_STR,
  USBD_IDX_SERIAL_STR,
  USBD_MAX_NUM_CONFIGURATION
};

static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] = {
  USB_LEN_LANGID_STR_DESC,
  USB_DESC_TYPE_STRING,
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING)
};

static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];
static uint8_t *USBD_FS_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(USBD_FS_DeviceDesc);
  return USBD_FS_DeviceDesc;
}

static uint8_t *USBD_FS_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  *length = sizeof(USBD_LangIDDesc);
  return USBD_LangIDDesc;
}

static uint8_t *USBD_FS_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
}

static uint8_t *USBD_FS_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return USBD_GetString((uint8_t *)USBD_PRODUCT_STRING_FS, USBD_StrDesc, length);
}

static uint8_t *USBD_FS_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return USBD_GetString((uint8_t *)USBD_SERIAL_STRING, USBD_StrDesc, length);
}

static uint8_t *USBD_FS_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return USBD_GetString((uint8_t *)USBD_CONFIGURATION_STRING_FS, USBD_StrDesc, length);
}

static uint8_t *USBD_FS_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
  (void)speed;
  return USBD_GetString((uint8_t *)USBD_INTERFACE_STRING_FS, USBD_StrDesc, length);
}

USBD_DescriptorsTypeDef FS_Desc = {
  USBD_FS_DeviceDescriptor,
  USBD_FS_LangIDStrDescriptor,
  USBD_FS_ManufacturerStrDescriptor,
  USBD_FS_ProductStrDescriptor,
  USBD_FS_SerialStrDescriptor,
  USBD_FS_ConfigStrDescriptor,
  USBD_FS_InterfaceStrDescriptor
};
