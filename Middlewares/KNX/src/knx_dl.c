/**
 * @file knx_dl.c
 * @brief KNX Data Link Layer — TPUART2 driver
 */
#include "knx_dl.h"
#include <string.h>

#define TPUART_U_RESET_REQ          0x01U
#define TPUART_U_RESET_IND          0x03U
#define TPUART_U_ACK_REQ            0x11U
#define KNX_CTRL_STD_DATA           0xBCU
#define KNX_FRAME_MIN_BYTES         9U
#define TPUART_RESET_TIMEOUT_MS     500U

static uint8_t  s_rx_buf[KNX_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static inline void rb_push(uint8_t byte)
{
    uint16_t next = (s_rx_head + 1U) & (KNX_RX_BUF_SIZE - 1U);
    if (next != s_rx_tail) {
        s_rx_buf[s_rx_head] = byte;
        s_rx_head = next;
    }
}

static inline int rb_pop(uint8_t *byte)
{
    if (s_rx_head == s_rx_tail) { return -1; }
    *byte = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1U) & (KNX_RX_BUF_SIZE - 1U);
    return 0;
}

static const knx_hal_t      *s_hal     = 0;
static knx_dl_frame_cb_t     s_frame_cb = 0;
static uint8_t  s_asm_buf[KNX_MAX_FRAME_SIZE];
static uint8_t  s_asm_len = 0U;
static uint8_t  s_asm_expected = 0U;
static uint8_t  s_asm_active = 0U;

static uint8_t calc_fcs(const uint8_t *data, uint8_t len)
{
    uint8_t fcs = 0xFFU; uint8_t i;
    for (i = 0U; i < len; i++) { fcs ^= data[i]; }
    return fcs;
}

static void dispatch_frame(void)
{
    if (s_asm_len < KNX_FRAME_MIN_BYTES) {
        s_asm_active = 0U; s_asm_len = 0U; return;
    }
    uint8_t fcs = calc_fcs(s_asm_buf, s_asm_len);
    if (fcs != 0xFFU) {
        s_asm_active = 0U; s_asm_len = 0U; return;
    }
    knx_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.ctrl      = s_asm_buf[0];
    frame.src_addr  = ((uint16_t)s_asm_buf[1] << 8) | s_asm_buf[2];
    frame.dst_addr  = ((uint16_t)s_asm_buf[3] << 8) | s_asm_buf[4];
    frame.addr_type = (s_asm_buf[5] >> 7) & 0x1U;
    frame.hop_count = (s_asm_buf[5] >> 4) & 0x7U;
    uint8_t data_len = s_asm_buf[5] & 0x0FU;
    frame.apdu_len = data_len + 1U;
    if (frame.apdu_len > 14U) { frame.apdu_len = 14U; }
    uint8_t apdu_offset = 6U;
    if ((apdu_offset + frame.apdu_len) >= s_asm_len) {
        s_asm_active = 0U; s_asm_len = 0U; return;
    }
    memcpy(frame.apdu, &s_asm_buf[apdu_offset], frame.apdu_len);
    if (frame.addr_type == 1U) {
        s_hal->uart_send_byte(TPUART_U_ACK_REQ);
    }
    if (s_frame_cb) { s_frame_cb(&frame); }
    s_asm_active = 0U; s_asm_len = 0U;
}

static void process_rx_byte(uint8_t byte)
{
    if (!s_asm_active) {
        if ((byte & 0xD3U) == 0x90U) {
            s_asm_active = 1U; s_asm_len = 0U; s_asm_expected = 0U;
            s_asm_buf[s_asm_len++] = byte;
        }
        return;
    }
    s_asm_buf[s_asm_len++] = byte;
    if (s_asm_len == 6U) {
        uint8_t data_len = s_asm_buf[5] & 0x0FU;
        s_asm_expected = (uint8_t)(6U + data_len + 1U + 1U);
    }
    if (s_asm_expected > 0U && s_asm_len >= s_asm_expected) {
        dispatch_frame();
    }
    if (s_asm_len >= KNX_MAX_FRAME_SIZE) {
        s_asm_active = 0U; s_asm_len = 0U;
    }
}

void knx_dl_rx_byte(uint8_t byte) { rb_push(byte); }

void knx_dl_init(const knx_hal_t *hal, knx_dl_frame_cb_t cb)
{
    s_hal = hal; s_frame_cb = cb;
    s_rx_head = 0U; s_rx_tail = 0U;
    s_asm_active = 0U; s_asm_len = 0U;
    hal->uart_set_rx_callback(knx_dl_rx_byte);
    hal->tpuart_reset_pin(0U);
    hal->delay_us(20U);
    hal->tpuart_reset_pin(1U);
    uint32_t t_start = hal->get_tick_ms();
    while ((hal->get_tick_ms() - t_start) < TPUART_RESET_TIMEOUT_MS) {
        uint8_t b;
        if (rb_pop(&b) == 0) {
            if (b == TPUART_U_RESET_IND) { break; }
        }
    }
    hal->uart_send_byte(TPUART_U_RESET_REQ);
}

void knx_dl_process(void)
{
    uint8_t byte;
    while (rb_pop(&byte) == 0) { process_rx_byte(byte); }
}

int knx_dl_send(const knx_frame_t *frame)
{
    if (!frame || !s_hal) { return -1; }
    uint8_t raw[KNX_MAX_FRAME_SIZE];
    uint8_t raw_len = 0U;
    raw[raw_len++] = frame->ctrl;
    raw[raw_len++] = (uint8_t)(frame->src_addr >> 8);
    raw[raw_len++] = (uint8_t)(frame->src_addr & 0xFFU);
    raw[raw_len++] = (uint8_t)(frame->dst_addr >> 8);
    raw[raw_len++] = (uint8_t)(frame->dst_addr & 0xFFU);
    uint8_t data_len = (frame->apdu_len > 0U) ? (frame->apdu_len - 1U) : 0U;
    raw[raw_len++] = (uint8_t)((frame->addr_type << 7) |
                               ((frame->hop_count & 0x7U) << 4) |
                               (data_len & 0x0FU));
    uint8_t i;
    for (i = 0U; i < frame->apdu_len && raw_len < (KNX_MAX_FRAME_SIZE - 1U); i++) {
        raw[raw_len++] = frame->apdu[i];
    }
    raw[raw_len] = calc_fcs(raw, raw_len);
    raw_len++;
    for (i = 0U; i < raw_len; i++) {
        uint8_t service = (i == raw_len - 1U)
            ? (uint8_t)(0x40U | i)
            : (uint8_t)(0x80U | i);
        s_hal->uart_send_byte(service);
        s_hal->uart_send_byte(raw[i]);
    }
    return 0;
}
