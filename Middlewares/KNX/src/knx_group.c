/**
 * @file knx_group.c
 * @brief KNX Group Object layer implementation
 */
#include "knx_group.h"
#include <string.h>

static knx_object_config_t s_objects[KNX_MAX_GROUP_OBJECTS];
static uint8_t             s_count = 0U;
static void (*s_rx_cb)(uint16_t ga, knx_dpt_type_t dpt,
                        knx_dpt_value_t value) = 0;

void knx_group_init(const knx_flash_config_t *cfg)
{
    if (!cfg) { return; }
    uint8_t n = cfg->num_objects;
    if (n > KNX_MAX_GROUP_OBJECTS) { n = KNX_MAX_GROUP_OBJECTS; }
    memcpy(s_objects, cfg->objects, n * sizeof(knx_object_config_t));
    s_count = n;
}

const knx_object_config_t *knx_group_find(uint16_t ga)
{
    uint8_t i;
    for (i = 0U; i < s_count; i++) {
        if (s_objects[i].group_address == ga) { return &s_objects[i]; }
    }
    return 0;
}

void knx_group_on_telegram(const knx_telegram_t *tg)
{
    if (!tg || !s_rx_cb) { return; }
    if (tg->apci == KNX_APCI_GROUP_READ) { return; }
    const knx_object_config_t *obj = knx_group_find(tg->ga);
    if (!obj) { return; }
    if (!(obj->flags & KNX_FLAG_RECEIVE)) { return; }
    knx_dpt_value_t value;
    memset(&value, 0, sizeof(value));
    if (knx_dpt_decode(obj->dpt, tg->raw_apdu, tg->raw_apdu_len, &value) != 0) { return; }
    s_rx_cb(tg->ga, obj->dpt, value);
}

void knx_group_set_rx_callback(void (*cb)(uint16_t ga, knx_dpt_type_t dpt,
                                          knx_dpt_value_t value))
{
    s_rx_cb = cb;
}

int knx_group_set_object(const knx_object_config_t *obj)
{
    if (!obj) { return -1; }
    uint8_t i;
    for (i = 0U; i < s_count; i++) {
        if (s_objects[i].object_index == obj->object_index) {
            s_objects[i] = *obj; return 0;
        }
    }
    if (s_count >= KNX_MAX_GROUP_OBJECTS) { return -1; }
    s_objects[s_count++] = *obj;
    return 0;
}

int knx_group_del_object(uint8_t object_index)
{
    uint8_t i;
    for (i = 0U; i < s_count; i++) {
        if (s_objects[i].object_index == object_index) {
            uint8_t j;
            for (j = i; j < s_count - 1U; j++) { s_objects[j] = s_objects[j + 1U]; }
            s_count--;
            return 0;
        }
    }
    return -1;
}

uint8_t knx_group_get_count(void) { return s_count; }

const knx_object_config_t *knx_group_get_by_pos(uint8_t pos)
{
    if (pos >= s_count) { return 0; }
    return &s_objects[pos];
}

void knx_group_export_to_config(knx_flash_config_t *cfg)
{
    if (!cfg) { return; }
    cfg->num_objects = s_count;
    memcpy(cfg->objects, s_objects, s_count * sizeof(knx_object_config_t));
}
