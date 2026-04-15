/**
 * @file knx_group.h
 * @brief KNX Group Object layer (internal)
 */
#ifndef KNX_GROUP_H
#define KNX_GROUP_H

#include <stdint.h>
#include "knx_flash.h"
#include "knx_transport.h"
#include "knx_dpt.h"

void knx_group_init(const knx_flash_config_t *cfg);
const knx_object_config_t *knx_group_find(uint16_t ga);
void knx_group_on_telegram(const knx_telegram_t *tg);
void knx_group_set_rx_callback(void (*cb)(uint16_t ga, knx_dpt_type_t dpt,
                                          knx_dpt_value_t value));
int  knx_group_set_object(const knx_object_config_t *obj);
int  knx_group_del_object(uint8_t object_index);
uint8_t knx_group_get_count(void);
const knx_object_config_t *knx_group_get_by_pos(uint8_t pos);
void knx_group_export_to_config(knx_flash_config_t *cfg);

#endif /* KNX_GROUP_H */
