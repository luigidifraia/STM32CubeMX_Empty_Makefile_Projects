/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @version        : v2.0_Cube
  * @brief          : Usb device for Virtual Com Port.
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
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include "circ_buf.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/**
 * Ring buffer that backs the RX path.  Filled from CDC_Receive_FS (ISR
 * context); drained by CDC_Available / CDC_GetChar / CDC_ReadBuf (main
 * context).  Mirrors the mbed CircBuffer<uint8_t> buf(128) member of
 * USBSerial, but sized to 255 usable bytes (CIRC_BUF_SIZE - 1).
 */
static circ_buf_t cdc_rx_buf;

/**
 * Minimal CDC line-coding shadow register (9600 8N1 default).
 * Returned verbatim on CDC_GET_LINE_CODING; updated on CDC_SET_LINE_CODING.
 * Some host-side drivers will not open the port without a successful
 * SET_LINE_CODING / GET_LINE_CODING exchange.
 *
 * Byte layout per USB CDC spec Table 17:
 *   [0..3] dwDTERate   – baud rate (little-endian uint32)
 *   [4]    bCharFormat – stop bits  (0=1, 1=1.5, 2=2)
 *   [5]    bParityType – parity     (0=None, 1=Odd, 2=Even)
 *   [6]    bDataBits   – data bits  (5/6/7/8/16)
 */
static uint8_t cdc_line_coding[7] = {
    0x00, 0xC2, 0x01, 0x00,   /* 115200 baud */
    0x00,                     /* 1 stop bit  */
    0x00,                     /* no parity   */
    0x08                      /* 8 data bits */
};

/**
 * Set to 1 when the host asserts DTR (terminal open); cleared when DTR is
 * de-asserted.  Written from USB interrupt context via CDC_Control_FS.
 * Mirrors USBSerial::terminal_connected.
 */
volatile uint8_t cdc_connected = 0U;

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
  * @brief Private variables.
  * @{
  */
/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  circ_buf_init(&cdc_rx_buf);
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  cdc_connected = 0U;
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
    case CDC_SET_LINE_CODING:
      /* Host is configuring baud / framing — store it so we can echo it back. */
      memcpy(cdc_line_coding, pbuf, sizeof(cdc_line_coding));
    break;

    case CDC_GET_LINE_CODING:
      /* Host is querying our current line-coding parameters. */
      memcpy(pbuf, cdc_line_coding, sizeof(cdc_line_coding));
    break;

    case CDC_SET_CONTROL_LINE_STATE:
      /*
       * pbuf is a pointer to the raw USBD_SetupReqTypedef that the USB
       * device library passes through for class requests without a data
       * phase.  wValue holds the control-line bitmap:
       *   bit 0 – DTR (Data Terminal Ready): 1 = terminal is open
       *   bit 1 – RTS (Request To Send)
       *
       * We use DTR as the "terminal connected" indicator, matching the
       * behaviour of the mbed USBSerial::terminal_connected flag.
       */
      cdc_connected = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) ? 1U : 0U;
    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  /*
   * Push every byte from the USB packet into the ring buffer.  If the ring
   * buffer is full, circ_buf_push() drops the oldest byte (lossy), which is
   * the same overflow policy used by the mbed CircBuffer.
   *
   * This mirrors the body of USBSerial::EP2_OUT_callback().
   */
  for (uint32_t i = 0U; i < *Len; i++)
  {
    circ_buf_push(&cdc_rx_buf, Buf[i]);
  }

  /* Re-point the RX buffer and re-arm the OUT endpoint for the next packet. */
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *         @note
  *
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/* ---------------------------------------------------------------------------
 * User-facing serial API
 * Mirrors the interface provided by mbed's USBSerial class.
 * ---------------------------------------------------------------------------*/

/**
 * @brief  Return 1 if the USB device is fully enumerated and configured.
 *
 * Reads hUsbDeviceFS.dev_state, which is driven by the USB interrupt on
 * every bus event — including physical cable removal (USBD_LL_DevDisconnected
 * walks the state machine back to USBD_STATE_DEFAULT).  This is therefore
 * reliable for detecting an abrupt cable pull, unlike cdc_connected which
 * depends on the host sending SET_CONTROL_LINE_STATE and so is never cleared
 * when the cable is yanked.
 *
 * Equivalent to usb_configured() in the Teensy / AVR USB serial library.
 *
 * @return 1 if USBD_STATE_CONFIGURED, 0 otherwise.
 */
uint8_t CDC_Configured(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? 1U : 0U;
}

/**
 * @brief  Return the number of bytes waiting in the RX ring buffer.
 */
uint8_t CDC_Available(void)
{
    return circ_buf_available(&cdc_rx_buf);
}

/**
 * @brief  Blocking single-byte read.
 *
 * Spins until a byte is available in the ring buffer, then returns it.
 * Mirrors USBSerial::_getc() / the mbed Stream::getc() contract.
 */
int CDC_GetChar(void)
{
    uint8_t c;
    while (!circ_buf_pop(&cdc_rx_buf, &c))
    {
        /* Tight-loop wait — insert __WFI() here if you want to save power,
         * but only if the USB interrupt is able to wake the core. */
    }
    return (int)c;
}

/**
 * @brief  Send a single byte.
 *
 * Guards on both CDC_Configured() and cdc_connected, mirroring the two-
 * condition check in the Teensy recv_str():
 *   !usb_configured() || !(usb_serial_get_control() & USB_SERIAL_DTR)
 * This ensures the function returns immediately on a cable pull even if
 * cdc_connected is stale.
 */
int CDC_PutChar(int c)
{
    if (!CDC_Configured() || !cdc_connected)
    {
        return 0;
    }
    uint8_t byte = (uint8_t)c;
    return (int)CDC_Transmit_FS(&byte, 1U);
}

/**
 * @brief  Non-blocking bulk read from the RX ring buffer.
 *
 * Copies up to maxLen bytes into buf, returning the actual count copied.
 * Returns immediately — call CDC_Available() first if you need to know
 * how many bytes are waiting.
 */
uint16_t CDC_ReadBuf(uint8_t *buf, uint16_t maxLen)
{
    uint16_t count = 0U;
    uint8_t  byte;

    while (count < maxLen && circ_buf_pop(&cdc_rx_buf, &byte))
    {
        buf[count++] = byte;
    }
    return count;
}

/**
 * @brief  Send a block of bytes over the CDC bulk-IN endpoint.
 *
 * Equivalent to USBSerial::writeBlock() but without the 64-byte hard cap —
 * the ST USB stack segments larger transfers internally.  Returns USBD_BUSY
 * if the previous transmission has not completed; the caller should retry.
 * Guards on both CDC_Configured() and cdc_connected for the same reason as
 * CDC_PutChar().
 */
uint8_t CDC_WriteBuf(uint8_t *buf, uint16_t len)
{
    if (!CDC_Configured() || !cdc_connected)
    {
        return USBD_OK;   /* Silently succeed when not connected, consistent
                           * with CDC_PutChar() behaviour. */
    }
    return CDC_Transmit_FS(buf, len);
}

/**
 * @brief  Discard all bytes currently waiting in the RX ring buffer.
 *
 * Resets the ring buffer to empty by re-initialising its head and tail
 * indices.  Call this immediately after cdc_connected goes high to discard
 * any stale bytes that accumulated while no terminal was open — equivalent
 * to usb_serial_flush_input() in the Teensy / AVR USB serial library.
 *
 * Safe to call from main-loop context; circ_buf_init() writes both volatile
 * indices in a single store each, which is atomic on Cortex-M.
 */
void CDC_FlushInput(void)
{
    circ_buf_init(&cdc_rx_buf);
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
