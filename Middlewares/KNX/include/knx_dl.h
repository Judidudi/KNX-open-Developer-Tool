/**
 * @file knx_dl.h
 * @brief KNX Data Link Layer — TPUART2 driver (internal)
 */
#ifndef KNX_DL_H
#define KNX_DL_H

#include <stdint.h>
#include "knx_hal.h"

#define KNX_MAX_FRAME_SIZE  23U
#define KNX_RX_BUF_SIZE     256U

typedef struct {
    uint8_t  ctrl;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint8_t  addr_type;
    uint8_t  hop_count;
    uint8_t  apdu[14];
    uint8_t  apdu_len;
} knx_frame_t;

typedef void (*knx_dl_frame_cb_t)(const knx_frame_t *frame);

void knx_dl_init(const knx_hal_t *hal, knx_dl_frame_cb_t cb);
void knx_dl_process(void);
int  knx_dl_send(const knx_frame_t *frame);
void knx_dl_rx_byte(uint8_t byte);

#endif /* KNX_DL_H */
