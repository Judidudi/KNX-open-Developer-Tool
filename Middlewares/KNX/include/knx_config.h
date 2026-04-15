/**
 * @file knx_config.h
 * @brief KNX Configuration Protocol handler — firmware side (internal)
 */
#ifndef KNX_CONFIG_H
#define KNX_CONFIG_H

#include <stdint.h>
#include "knx_transport.h"

#define KNX_CFG_MAGIC    0xCFU
#define KNX_CFG_VERSION  0x01U

#define KNX_CFG_CMD_DISCOVER           0x01U
#define KNX_CFG_CMD_DISCOVER_RESP      0x02U
#define KNX_CFG_CMD_SET_PA             0x10U
#define KNX_CFG_CMD_SET_PA_ACK         0x11U
#define KNX_CFG_CMD_SET_NAME           0x12U
#define KNX_CFG_CMD_SET_NAME_ACK       0x13U
#define KNX_CFG_CMD_GET_OBJ_COUNT      0x20U
#define KNX_CFG_CMD_GET_OBJ_COUNT_RESP 0x21U
#define KNX_CFG_CMD_GET_OBJ            0x22U
#define KNX_CFG_CMD_GET_OBJ_RESP       0x23U
#define KNX_CFG_CMD_SET_OBJ            0x30U
#define KNX_CFG_CMD_SET_OBJ_ACK        0x31U
#define KNX_CFG_CMD_DEL_OBJ            0x32U
#define KNX_CFG_CMD_DEL_OBJ_ACK        0x33U
#define KNX_CFG_CMD_COMMIT             0x40U
#define KNX_CFG_CMD_COMMIT_ACK         0x41U
#define KNX_CFG_CMD_GET_INFO           0x50U
#define KNX_CFG_CMD_GET_INFO_RESP      0x51U
#define KNX_CFG_CMD_FACTORY_RESET      0x60U
#define KNX_CFG_CMD_FACTORY_RESET_ACK  0x61U
#define KNX_CFG_CMD_ERROR              0xFFU

#define KNX_CFG_ERR_UNKNOWN_CMD        0x01U
#define KNX_CFG_ERR_INVALID_INDEX      0x02U
#define KNX_CFG_ERR_FLASH              0x03U
#define KNX_CFG_ERR_NOT_PROG_MODE      0x04U
#define KNX_CFG_ERR_VERSION            0x05U

#define KNX_CFG_DISCOVERY_GA  0x0001U
#define KNX_FIRMWARE_VERSION_MAJOR  1U
#define KNX_FIRMWARE_VERSION_MINOR  0U

void knx_config_init(uint16_t device_pa);
void knx_config_set_pa(uint16_t pa);
void knx_config_on_telegram(const knx_telegram_t *tg,
                             const uint8_t *raw, uint8_t len);

#endif /* KNX_CONFIG_H */
