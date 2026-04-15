/**
 * @file knx_transport.h
 * @brief KNX Transport / Network Layer (internal)
 */
#ifndef KNX_TRANSPORT_H
#define KNX_TRANSPORT_H

#include <stdint.h>
#include "knx_dl.h"
#include "knx_dpt.h"
#include "knx_hal.h"

#define KNX_APCI_GROUP_READ     0x0000U
#define KNX_APCI_GROUP_RESP     0x0040U
#define KNX_APCI_GROUP_WRITE    0x0080U
#define KNX_ACK_TIMEOUT_MS      300U

typedef struct {
    uint16_t        ga;
    uint16_t        src_addr;
    uint16_t        apci;
    knx_dpt_type_t  dpt;
    knx_dpt_value_t value;
    uint8_t         raw_apdu[14];
    uint8_t         raw_apdu_len;
} knx_telegram_t;

typedef void (*knx_transport_rx_cb_t)(const knx_telegram_t *tg);

void knx_transport_init(const knx_hal_t *hal, knx_transport_rx_cb_t cb);
void knx_transport_process(void);
int  knx_transport_send_write(uint16_t ga, uint16_t src,
                              knx_dpt_type_t dpt, knx_dpt_value_t value);
int  knx_transport_send_read(uint16_t ga, uint16_t src);
int  knx_transport_send_response(uint16_t ga, uint16_t src,
                                 knx_dpt_type_t dpt, knx_dpt_value_t value);
void knx_transport_set_pa(uint16_t pa);
void knx_transport_on_frame(const knx_frame_t *frame);

#endif /* KNX_TRANSPORT_H */
