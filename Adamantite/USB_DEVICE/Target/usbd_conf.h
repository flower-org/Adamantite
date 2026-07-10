#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_pcd.h"
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION   1U
#define USBD_MAX_STR_DESC_SIZ        512U
#define USBD_SUPPORT_USER_STRING     0U
#define USBD_SELF_POWERED            1U
#define USBD_DEBUG_LEVEL             0U
#define USBD_LPM_ENABLED             0U

#define DEVICE_FS                    0U

#define USBD_malloc         malloc
#define USBD_free           free
#define USBD_memset         memset
#define USBD_memcpy         memcpy
#define USBD_Delay          HAL_Delay

#if (USBD_DEBUG_LEVEL > 0U)
#define USBD_UsrLog(...)    do { } while(0)
#else
#define USBD_UsrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 1U)
#define USBD_ErrLog(...)    do { } while(0)
#else
#define USBD_ErrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 2U)
#define USBD_DbgLog(...)    do { } while(0)
#else
#define USBD_DbgLog(...)
#endif

#ifdef __cplusplus
 }
#endif

#endif /* __USBD_CONF_H */
