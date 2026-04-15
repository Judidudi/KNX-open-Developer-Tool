/**
 * @file knx_hal.h
 * @brief KNX Hardware Abstraction Layer interface
 *
 * Platform-independent HAL interface for the KNX stack.
 * All platform-specific implementations must populate a knx_hal_t struct
 * and pass it to knx_init().
 */
#ifndef KNX_HAL_H
#define KNX_HAL_H

#include <stdint.h>

/**
 * @brief HAL function pointer struct.
 *
 * All function pointers must be non-NULL before passing to knx_init().
 */
typedef struct {
    /** Send a single byte over UART (TPUART2). Blocking or IRQ-buffered. */
    void     (*uart_send_byte)(uint8_t byte);

    /** Register a callback that is called (from ISR or main loop) for each
     *  received UART byte. Must be called before any reception occurs. */
    void     (*uart_set_rx_callback)(void (*cb)(uint8_t byte));

    /** Drive the TPUART2 hardware reset pin.
     *  level=0 → assert reset (active-low), level=1 → release reset. */
    void     (*tpuart_reset_pin)(uint8_t level);

    /** Control the programming-mode LED.
     *  on=1 → LED on, on=0 → LED off. */
    void     (*prog_led_set)(uint8_t on);

    /** Read the programming-mode button.
     *  Returns 1 if pressed, 0 if released. */
    uint8_t  (*prog_button_read)(void);

    /** Return a monotonically increasing millisecond tick counter.
     *  May wrap around at 32-bit boundary. */
    uint32_t (*get_tick_ms)(void);

    /** Busy-wait for the given number of microseconds. */
    void     (*delay_us)(uint32_t us);
} knx_hal_t;

#endif /* KNX_HAL_H */
