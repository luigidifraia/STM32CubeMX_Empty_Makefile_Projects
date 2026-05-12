/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.h
  * @version        : v2.0_Cube
  * @brief          : Header for usbd_cdc_if.c file.
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

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

/* USER CODE BEGIN INCLUDE */

/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief For Usb device.
  * @{
  */

/** @defgroup USBD_CDC_IF USBD_CDC_IF
  * @brief Usb VCP device module
  * @{
  */

/** @defgroup USBD_CDC_IF_Exported_Defines USBD_CDC_IF_Exported_Defines
  * @brief Defines.
  * @{
  */
/* Define size for the receive and transmit buffer over CDC */
#define APP_RX_DATA_SIZE  1024
#define APP_TX_DATA_SIZE  1024
/* USER CODE BEGIN EXPORTED_DEFINES */

/* USER CODE END EXPORTED_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Types USBD_CDC_IF_Exported_Types
  * @brief Types.
  * @{
  */

/* USER CODE BEGIN EXPORTED_TYPES */

/* USER CODE END EXPORTED_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Macros USBD_CDC_IF_Exported_Macros
  * @brief Aliases.
  * @{
  */

/* USER CODE BEGIN EXPORTED_MACRO */

/* USER CODE END EXPORTED_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

/** CDC Interface callback. */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/**
 * Set to 1 when the host asserts DTR (i.e. a terminal is open), cleared
 * when DTR is de-asserted.  Mirrors the mbed USBSerial::terminal_connected
 * flag.  Written from USB interrupt context — always read via a local copy
 * or treat as a hint rather than a guarantee.
 */
extern volatile uint8_t cdc_connected;

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_FunctionsPrototype USBD_CDC_IF_Exported_FunctionsPrototype
  * @brief Public functions declaration.
  * @{
  */

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

/* USER CODE BEGIN EXPORTED_FUNCTIONS */

/**
 * @brief  Return 1 if the USB device is fully enumerated and configured.
 *
 * Reads hUsbDeviceFS.dev_state directly, which is updated by the USB
 * interrupt on every bus event including physical cable removal.  This
 * is the STM32 equivalent of usb_configured() in the Teensy / AVR USB
 * serial library.
 *
 * Use this together with cdc_connected to distinguish two disconnect cases:
 *   - Cable pulled  → CDC_Configured() == 0, cdc_connected may still be 1
 *   - Terminal closed (DTR dropped, cable still present)
 *                   → CDC_Configured() == 1, cdc_connected == 0
 *
 * @return 1 if configured, 0 otherwise.
 */
uint8_t CDC_Configured(void);

/**
 * @brief  Return the number of bytes waiting in the RX ring buffer.
 *
 * Equivalent to USBSerial::available().
 *
 * @return Number of bytes available (0 … 255).
 */
uint8_t CDC_Available(void);

/**
 * @brief  Read one byte, blocking until one arrives.
 *
 * Equivalent to USBSerial::_getc() / Stream::getc().
 * Spins in a tight loop — add a timeout or use CDC_Available() first if
 * blocking indefinitely is undesirable.
 *
 * @return The received byte cast to int.
 */
int CDC_GetChar(void);

/**
 * @brief  Send one byte.
 *
 * Equivalent to USBSerial::_putc().  Returns without sending if the host
 * terminal is not open (cdc_connected == 0).
 *
 * @param  c  Byte to send.
 * @return    USBD_OK (0) on success, USBD_BUSY (2) if the TX endpoint is
 *            occupied, or 0 if not connected.
 */
int CDC_PutChar(int c);

/**
 * @brief  Drain up to maxLen bytes from the RX ring buffer into buf.
 *
 * Non-blocking — returns immediately with however many bytes were available.
 * Equivalent to calling CDC_GetChar() in a loop but without the blocking.
 *
 * @param[out] buf     Destination buffer (must be at least maxLen bytes).
 * @param[in]  maxLen  Maximum number of bytes to read.
 * @return             Actual number of bytes copied.
 */
uint16_t CDC_ReadBuf(uint8_t *buf, uint16_t maxLen);

/**
 * @brief  Send a block of data over the CDC bulk-IN endpoint.
 *
 * Equivalent to USBSerial::writeBlock().  Unlike writeBlock() there is no
 * hard 64-byte limit enforced here; the ST USB stack will segment larger
 * transfers internally.  The call returns USBD_BUSY if a previous
 * transmission has not completed yet — the caller should retry if needed.
 *
 * @param[in] buf   Data to transmit.
 * @param[in] len   Number of bytes to transmit.
 * @return          USBD_OK (0), USBD_BUSY (2), or USBD_FAIL (1).
 */
uint8_t CDC_WriteBuf(uint8_t *buf, uint16_t len);

/**
 * @brief  Discard all bytes currently waiting in the RX ring buffer.
 *
 * Equivalent to usb_serial_flush_input() on the Teensy / AVR USB serial
 * library.  Call this immediately after a terminal connects (DTR asserts)
 * to discard any stale bytes — OS-generated modem "AT command" probes,
 * leftovers from a previous session, etc. — before showing a banner or
 * entering a command loop.
 */
void CDC_FlushInput(void);

/**
 * @brief  Robust connection test — use this in preference to reading
 *         cdc_connected directly.
 *
 * Returns non-zero only when BOTH of the following are true:
 *   1. cdc_connected == 1  (host asserted DTR via SET_CONTROL_LINE_STATE)
 *   2. hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED  (USB stack is up)
 *
 * Gating on dev_state catches physical cable removal on independently-
 * powered boards (e.g. Blue Pill Plus running from 3.3 V / 5 V pins).
 * When the cable is pulled the D+ line discharges, the USB peripheral
 * sees a bus-reset / disconnect event and transitions dev_state away from
 * USBD_STATE_CONFIGURED via the HAL interrupt — even without explicit VBUS
 * sensing — whereas cdc_connected may remain stale because the host never
 * gets the chance to send SET_CONTROL_LINE_STATE with DTR = 0.
 *
 * Always use CDC_IsConnected() in loop conditions; never test cdc_connected
 * directly in application code.
 *
 * @return  1 if a terminal is open and the USB link is active, 0 otherwise.
 */
uint8_t CDC_IsConnected(void);

/* USER CODE END EXPORTED_FUNCTIONS */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */

