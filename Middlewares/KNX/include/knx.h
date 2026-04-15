/**
 * @file knx.h
 * @brief KNX stack public API
 */
#ifndef KNX_H
#define KNX_H

#include <stdint.h>
#include "knx_hal.h"
#include "knx_dpt.h"

typedef enum {
    KNX_OK             = 0,
    KNX_ERR_NOT_INIT   = 1,
    KNX_ERR_BUSY       = 2,
    KNX_ERR_TIMEOUT    = 3,
    KNX_ERR_INVALID_ARG = 4,
    KNX_ERR_FLASH      = 5,
} knx_status_t;

#define KNX_GA(main, middle, sub)  \
    ((uint16_t)(((main) << 11) | ((middle) << 8) | (sub)))

knx_status_t knx_init(const knx_hal_t *hal);
void knx_process(void);
knx_status_t knx_write(uint16_t ga, knx_dpt_type_t dpt, knx_dpt_value_t value);
knx_status_t knx_read_request(uint16_t ga);
void knx_on_receive(void (*cb)(uint16_t ga, knx_dpt_type_t dpt,
                               knx_dpt_value_t value));

#endif /* KNX_H */
