/**
 * @file knx_dpt.h
 * @brief KNX Datapoint Types (DPT) definitions and encoding/decoding API
 */
#ifndef KNX_DPT_H
#define KNX_DPT_H

#include <stdint.h>

/** Supported DPT types */
typedef enum {
    KNX_DPT1 = 1,  /**< 1-bit value (boolean) */
    KNX_DPT2 = 2,  /**< 2-bit value (controlled) */
    KNX_DPT5 = 5,  /**< 8-bit unsigned value (0..255) */
    KNX_DPT9 = 9,  /**< 2-byte float (KNX F16) */
} knx_dpt_type_t;

/** Union holding the native value for any supported DPT */
typedef union {
    uint8_t dpt1;                              /**< DPT-1: 0 or 1 */
    struct {
        uint8_t control : 1;                   /**< DPT-2: control bit */
        uint8_t value   : 1;                   /**< DPT-2: value bit */
    } dpt2;
    uint8_t dpt5;                              /**< DPT-5: 0..255 */
    float   dpt9;                              /**< DPT-9: float (-671088.64..+670760.96) */
} knx_dpt_value_t;

int knx_dpt_encode(knx_dpt_type_t type, knx_dpt_value_t value,
                   uint8_t *buf, uint8_t *len);

int knx_dpt_decode(knx_dpt_type_t type, const uint8_t *buf, uint8_t len,
                   knx_dpt_value_t *value);

uint16_t knx_dpt9_encode_f16(float f);
float knx_dpt9_decode_f16(uint16_t raw);

#endif /* KNX_DPT_H */
