/**
 * @file knx_dpt.c
 * @brief KNX Datapoint Type encoding/decoding implementation
 */
#include "knx_dpt.h"

uint16_t knx_dpt9_encode_f16(float f)
{
    float scaled = f * 100.0f;
    int32_t sign = 0;
    if (scaled < 0.0f) { sign = 1; scaled = -scaled; }
    int32_t exp = 0;
    int32_t mant = (int32_t)(scaled + 0.5f);
    while (mant > 2047 && exp < 15) { mant = (mant + 1) >> 1; exp++; }
    if (mant > 2047) { mant = 2047; }
    if (sign) { mant = -mant; mant &= 0x07FF; }
    return (uint16_t)(((sign & 0x1) << 15) | ((exp & 0xF) << 11) | (mant & 0x07FF));
}

float knx_dpt9_decode_f16(uint16_t raw)
{
    int32_t sign = (raw >> 15) & 0x1;
    int32_t exp  = (raw >> 11) & 0xF;
    int32_t mant = (int32_t)(raw & 0x07FF);
    if (sign) { mant |= ~0x07FF; }
    float result = 0.01f * (float)mant;
    int32_t i;
    for (i = 0; i < exp; i++) { result *= 2.0f; }
    return result;
}

int knx_dpt_encode(knx_dpt_type_t type, knx_dpt_value_t value,
                   uint8_t *buf, uint8_t *len)
{
    if (!buf || !len) { return -1; }
    switch (type) {
    case KNX_DPT1:
        buf[0] = (uint8_t)(value.dpt1 & 0x01U); *len = 1U; break;
    case KNX_DPT2:
        buf[0] = (uint8_t)(((value.dpt2.control & 0x1U) << 1) | (value.dpt2.value & 0x1U));
        *len = 1U; break;
    case KNX_DPT5:
        buf[0] = 0x00U; buf[1] = value.dpt5; *len = 2U; break;
    case KNX_DPT9: {
        uint16_t f16 = knx_dpt9_encode_f16(value.dpt9);
        buf[0] = 0x00U; buf[1] = (uint8_t)(f16 >> 8); buf[2] = (uint8_t)(f16 & 0xFFU);
        *len = 3U; break;
    }
    default: return -1;
    }
    return 0;
}

int knx_dpt_decode(knx_dpt_type_t type, const uint8_t *buf, uint8_t len,
                   knx_dpt_value_t *value)
{
    if (!buf || !value) { return -1; }
    switch (type) {
    case KNX_DPT1:
        if (len < 1U) { return -1; }
        value->dpt1 = buf[0] & 0x01U; break;
    case KNX_DPT2:
        if (len < 1U) { return -1; }
        value->dpt2.control = (buf[0] >> 1) & 0x1U;
        value->dpt2.value   =  buf[0]       & 0x1U; break;
    case KNX_DPT5:
        if (len < 2U) { return -1; }
        value->dpt5 = buf[1]; break;
    case KNX_DPT9:
        if (len < 3U) { return -1; }
        value->dpt9 = knx_dpt9_decode_f16((uint16_t)((buf[1] << 8) | buf[2])); break;
    default: return -1;
    }
    return 0;
}
