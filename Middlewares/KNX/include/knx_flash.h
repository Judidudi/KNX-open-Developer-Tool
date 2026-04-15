/**
 * @file knx_flash.h
 * @brief KNX Flash persistence interface
 */
#ifndef KNX_FLASH_H
#define KNX_FLASH_H

#include <stdint.h>
#include "knx_dpt.h"

#define KNX_FLASH_MAGIC        0x4B4E5801U
#define KNX_MAX_GROUP_OBJECTS  32U
#define KNX_FLAG_SEND      0x01U
#define KNX_FLAG_RECEIVE   0x02U
#define KNX_FLAG_TRANSMIT  0x04U

typedef struct {
    uint16_t       group_address;
    knx_dpt_type_t dpt;
    uint8_t        flags;
    uint8_t        object_index;
} knx_object_config_t;

typedef struct {
    uint32_t            magic;
    uint16_t            physical_address;
    uint8_t             device_name[32];
    uint8_t             num_objects;
    knx_object_config_t objects[KNX_MAX_GROUP_OBJECTS];
    uint32_t            crc32;
} knx_flash_config_t;

int knx_flash_read(knx_flash_config_t *cfg);
int knx_flash_write(knx_flash_config_t *cfg);
int knx_flash_erase(void);
uint32_t knx_crc32(const uint8_t *data, uint32_t len);

#endif /* KNX_FLASH_H */
