/**
 * @file knx.c
 * @brief KNX stack public API implementation — wires all layers together
 */
#include "knx.h"
#include "knx_dl.h"
#include "knx_transport.h"
#include "knx_group.h"
#include "knx_config.h"
#include "knx_flash.h"
#include "knx_prog.h"
#include <string.h>

static uint8_t           s_initialised = 0U;
static const knx_hal_t  *s_hal         = 0;
static knx_flash_config_t s_cfg;

static void (*s_app_rx_cb)(uint16_t ga, knx_dpt_type_t dpt,
                            knx_dpt_value_t value) = 0;

static void on_dl_frame(const knx_frame_t *frame)
{
    knx_transport_on_frame(frame);
}

static void on_transport_rx(const knx_telegram_t *tg)
{
    if (tg->apci == KNX_APCI_GROUP_WRITE &&
        tg->raw_apdu_len >= 6U &&
        tg->raw_apdu[2] == KNX_CFG_MAGIC)
    {
        knx_config_on_telegram(tg, &tg->raw_apdu[2],
                               (uint8_t)(tg->raw_apdu_len - 2U));
        return;
    }
    knx_group_on_telegram(tg);
}

static void on_group_rx(uint16_t ga, knx_dpt_type_t dpt,
                         knx_dpt_value_t value)
{
    if (s_app_rx_cb) {
        s_app_rx_cb(ga, dpt, value);
    }
}

knx_status_t knx_init(const knx_hal_t *hal)
{
    if (!hal)                      { return KNX_ERR_INVALID_ARG; }
    if (!hal->uart_send_byte)      { return KNX_ERR_INVALID_ARG; }
    if (!hal->uart_set_rx_callback){ return KNX_ERR_INVALID_ARG; }
    if (!hal->tpuart_reset_pin)    { return KNX_ERR_INVALID_ARG; }
    if (!hal->prog_led_set)        { return KNX_ERR_INVALID_ARG; }
    if (!hal->prog_button_read)    { return KNX_ERR_INVALID_ARG; }
    if (!hal->get_tick_ms)         { return KNX_ERR_INVALID_ARG; }
    if (!hal->delay_us)            { return KNX_ERR_INVALID_ARG; }

    s_hal = hal;
    knx_flash_read(&s_cfg);
    knx_group_init(&s_cfg);
    knx_group_set_rx_callback(on_group_rx);
    knx_config_init(s_cfg.physical_address);
    knx_prog_init(hal);
    knx_dl_init(hal, on_dl_frame);
    knx_transport_init(hal, on_transport_rx);
    knx_transport_set_pa(s_cfg.physical_address);

    s_initialised = 1U;
    return KNX_OK;
}

void knx_process(void)
{
    if (!s_initialised) { return; }
    knx_dl_process();
    knx_transport_process();
    knx_prog_process();
}

knx_status_t knx_write(uint16_t ga, knx_dpt_type_t dpt, knx_dpt_value_t value)
{
    if (!s_initialised)          { return KNX_ERR_NOT_INIT; }
    if (ga == 0U)                { return KNX_ERR_INVALID_ARG; }
    int rc = knx_transport_send_write(ga, s_cfg.physical_address, dpt, value);
    if (rc != 0)                 { return KNX_ERR_BUSY; }
    return KNX_OK;
}

knx_status_t knx_read_request(uint16_t ga)
{
    if (!s_initialised) { return KNX_ERR_NOT_INIT; }
    if (ga == 0U)       { return KNX_ERR_INVALID_ARG; }
    int rc = knx_transport_send_read(ga, s_cfg.physical_address);
    if (rc != 0)        { return KNX_ERR_BUSY; }
    return KNX_OK;
}

void knx_on_receive(void (*cb)(uint16_t ga, knx_dpt_type_t dpt,
                               knx_dpt_value_t value))
{
    s_app_rx_cb = cb;
}
