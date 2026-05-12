/**
 * @file    circ_buf.h
 * @brief   Lock-free, single-producer / single-consumer circular byte buffer.
 *
 * Designed for bare-metal Cortex-M use where one side is an ISR (producer)
 * and the other is main-loop code (consumer).  The contract is:
 *
 *   - `head` is written ONLY by the producer (USB RX ISR via CDC_Receive_FS).
 *   - `tail` is written ONLY by the consumer (main-loop / application code).
 *
 * On a single-core Cortex-M, byte/halfword/word-aligned load-store operations
 * are atomic, so no critical section is needed as long as the above contract
 * is respected.
 *
 * Buffer capacity: CIRC_BUF_SIZE - 1 bytes (one slot is sacrificed as a
 * sentinel to distinguish "empty" from "full" without a separate flag —
 * the same technique used in the mbed USBSerial / CircBuffer implementation
 * this code is modelled after).
 *
 * CIRC_BUF_SIZE must be exactly 256 when uint8_t indices are used, because
 * the natural 8-bit wrap-around IS the modulo arithmetic.  If you need a
 * larger buffer, change the index type to uint16_t and set CIRC_BUF_SIZE to
 * any power of 2 up to 32768, then update circ_buf_available / is_full
 * accordingly.
 */

#ifndef CIRC_BUF_H
#define CIRC_BUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/** Total array size.  Usable capacity = CIRC_BUF_SIZE - 1 = 255 bytes. */
#define CIRC_BUF_SIZE 256U

typedef struct
{
    volatile uint8_t head;        /**< Write index — updated by producer (ISR). */
    volatile uint8_t tail;        /**< Read  index — updated by consumer (main). */
    uint8_t          buf[CIRC_BUF_SIZE];
} circ_buf_t;

/**
 * @brief Initialise (or reset) the buffer to empty.
 */
static inline void circ_buf_init(circ_buf_t *cb)
{
    cb->head = 0U;
    cb->tail = 0U;
}

/**
 * @brief Return the number of bytes currently stored (0 … CIRC_BUF_SIZE-1).
 *
 * The subtraction wraps naturally because both indices are uint8_t and the
 * buffer size is exactly 256.
 */
static inline uint8_t circ_buf_available(const circ_buf_t *cb)
{
    return (uint8_t)(cb->head - cb->tail);
}

/** @brief True when no bytes are waiting. */
static inline bool circ_buf_is_empty(const circ_buf_t *cb)
{
    return cb->head == cb->tail;
}

/**
 * @brief True when the buffer cannot accept another byte without dropping one.
 *
 * Full is defined as (head - tail) == CIRC_BUF_SIZE - 1 == 255, i.e. every
 * slot except the sentinel is occupied.
 */
static inline bool circ_buf_is_full(const circ_buf_t *cb)
{
    return (uint8_t)(cb->head - cb->tail) == (uint8_t)(CIRC_BUF_SIZE - 1U);
}

/**
 * @brief Push one byte into the buffer.
 *
 * If the buffer is full the oldest unread byte is silently discarded (the
 * same drop-oldest / lossy policy used by the mbed CircBuffer).  Called
 * from ISR context.
 */
static inline void circ_buf_push(circ_buf_t *cb, uint8_t byte)
{
    if (circ_buf_is_full(cb))
    {
        cb->tail++;   /* Discard oldest; uint8_t wraps naturally at 256. */
    }
    cb->buf[cb->head] = byte;
    cb->head++;       /* uint8_t wraps naturally at 256. */
}

/**
 * @brief Pop one byte from the buffer.
 *
 * @param[out] byte  Destination for the retrieved byte.
 * @return           true if a byte was returned, false if the buffer was empty.
 */
static inline bool circ_buf_pop(circ_buf_t *cb, uint8_t *byte)
{
    if (circ_buf_is_empty(cb))
    {
        return false;
    }
    *byte   = cb->buf[cb->tail];
    cb->tail++;       /* uint8_t wraps naturally at 256. */
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* CIRC_BUF_H */