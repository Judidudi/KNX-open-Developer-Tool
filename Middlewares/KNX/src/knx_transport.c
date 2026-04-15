/**
 * @file knx_transport.c
 * @brief KNX Transport / Network Layer implementation
 */
#include "knx_transport.h"
#include "knx_dl.h"
#include "knx_dpt.h"
#include <string.h>

#define KNX_CTRL_STD    0xBCU
#define KNX_HOP_COUNT   6U

static const knx_hal_t       *s_hal    = 0;
static knx_transport_rx_cb_t  s_rx_cb  = 0;
static uint16_t s_pa = 0x0000U;
static uint8_t  s_tx_pending  = 0U;
static uint32_t s_tx_start_ms = 0U;

static void build_frame(knx_frame_t *f, uint16_t ga, uint8_t addr_type,
                        const uint8_t *apdu, uint8_t apdu_len)
{
    f->ctrl = KNX_CTRL_STD; f->src_addr = s_pa; f->dst_addr = ga;
    f->addr_type = addr_type; f->hop_count = KNX_HOP_COUNT;
    f->apdu_len = apdu_len;
    if (apdu && apdu_len > 0U) { memcpy(f->apdu, apdu, apdu_len); }
}

void knx_transport_init(const knx_hal_t *hal, knx_transport_rx_cb_t cb)
{
    s_hal = hal; s_rx_cb = cb;
    s_tx_pending = 0U; s_tx_start_ms = 0U;
}

void knx_transport_set_pa(uint16_t pa) { s_pa = pa; }

void knx_transport_process(void)
{
    if (s_tx_pending && s_hal) {
        uint32_t elapsed = s_hal->get_tick_ms() - s_tx_start_ms;
        if (elapsed >= KNX_ACK_TIMEOUT_MS) { s_tx_pending = 0U; }
    }
}

int knx_transport_send_write(uint16_t ga, uint16_t src,
                             knx_dpt_type_t dpt, knx_dpt_value_t value)
{
    (void)src;
    if (s_tx_pending) { return -1; }
    uint8_t encoded[3]; uint8_t enc_len = 0U;
    if (knx_dpt_encode(dpt, value, encoded, &enc_len) != 0) { return -1; }
    uint8_t apdu[14]; uint8_t apdu_len = 0U;
    apdu[apdu_len++] = 0x00U;
    if (dpt == KNX_DPT1 || dpt == KNX_DPT2) {
        apdu[apdu_len++] = (uint8_t)(0x80U | (encoded[0] & 0x3FU));
    } else {
        apdu[apdu_len++] = 0x80U;
        uint8_t i;
        for (i = 1U; i < enc_len && apdu_len < 14U; i++) { apdu[apdu_len++] = encoded[i]; }
    }
    knx_frame_t frame;
    build_frame(&frame, ga, 1U, apdu, apdu_len);
    if (knx_dl_send(&frame) != 0) { return -1; }
    s_tx_pending = 1U; s_tx_start_ms = s_hal->get_tick_ms();
    return 0;
}

int knx_transport_send_read(uint16_t ga, uint16_t src)
{
    (void)src;
    if (s_tx_pending) { return -1; }
    uint8_t apdu[2] = { 0x00U, 0x00U };
    knx_frame_t frame;
    build_frame(&frame, ga, 1U, apdu, 2U);
    if (knx_dl_send(&frame) != 0) { return -1; }
    s_tx_pending = 1U; s_tx_start_ms = s_hal->get_tick_ms();
    return 0;
}

int knx_transport_send_response(uint16_t ga, uint16_t src,
                                knx_dpt_type_t dpt, knx_dpt_value_t value)
{
    (void)src;
    if (s_tx_pending) { return -1; }
    uint8_t encoded[3]; uint8_t enc_len = 0U;
    if (knx_dpt_encode(dpt, value, encoded, &enc_len) != 0) { return -1; }
    uint8_t apdu[14]; uint8_t apdu_len = 0U;
    apdu[apdu_len++] = 0x00U;
    if (dpt == KNX_DPT1 || dpt == KNX_DPT2) {
        apdu[apdu_len++] = (uint8_t)(0x40U | (encoded[0] & 0x3FU));
    } else {
        apdu[apdu_len++] = 0x40U;
        uint8_t i;
        for (i = 1U; i < enc_len && apdu_len < 14U; i++) { apdu[apdu_len++] = encoded[i]; }
    }
    knx_frame_t frame;
    build_frame(&frame, ga, 1U, apdu, apdu_len);
    if (knx_dl_send(&frame) != 0) { return -1; }
    s_tx_pending = 1U; s_tx_start_ms = s_hal->get_tick_ms();
    return 0;
}

void knx_transport_on_frame(const knx_frame_t *frame)
{
    if (!frame || !s_rx_cb) { return; }
    if (frame->addr_type != 1U) { return; }
    if (frame->apdu_len < 2U) { return; }
    uint16_t apci = (uint16_t)(((uint16_t)(frame->apdu[0] & 0x03U) << 8) |
                                (uint16_t)(frame->apdu[1] & 0xC0U));
    knx_telegram_t tg;
    memset(&tg, 0, sizeof(tg));
    tg.ga = frame->dst_addr; tg.src_addr = frame->src_addr;
    tg.apci = apci; tg.raw_apdu_len = frame->apdu_len;
    memcpy(tg.raw_apdu, frame->apdu, frame->apdu_len);
    switch (apci) {
    case KNX_APCI_GROUP_READ:
    case KNX_APCI_GROUP_RESP:
    case KNX_APCI_GROUP_WRITE:
        s_rx_cb(&tg); break;
    default: break;
    }
    s_tx_pending = 0U;
}
