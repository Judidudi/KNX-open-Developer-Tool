/**
 * @file knx_prog.h
 * @brief KNX Programming Mode state machine (internal)
 */
#ifndef KNX_PROG_H
#define KNX_PROG_H

#include <stdint.h>
#include "knx_hal.h"

#define KNX_PROG_LONG_PRESS_MS  3000U
#define KNX_PROG_DEBOUNCE_MS    50U

typedef enum {
    KNX_PROG_STATE_NORMAL       = 0,
    KNX_PROG_STATE_PROGRAMMING  = 1,
    KNX_PROG_STATE_FACTORY_RESET = 2,
} knx_prog_state_t;

void knx_prog_init(const knx_hal_t *hal);
void knx_prog_process(void);
knx_prog_state_t knx_prog_get_state(void);
uint8_t knx_prog_is_active(void);
void knx_prog_on_pa_assigned(void);

#endif /* KNX_PROG_H */
