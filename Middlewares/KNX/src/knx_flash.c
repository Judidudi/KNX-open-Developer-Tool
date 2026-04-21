/**
 * @file knx_flash.c
 * @brief Flash persistence for KNX configuration (STM32F446RE Sector 7)
 */
#include "knx_flash.h"
#include "stm32f446xx.h"
#include <string.h>

#define KNX_FLASH_BASE_ADDR    0x08060000UL
#define FLASH_SECTOR_7_NUM     7U
#define FLASH_KEY1             0x45670123UL
#define FLASH_KEY2             0xCDEF89ABUL
#define FLASH_TIMEOUT_CYCLES   0x00100000UL

uint32_t knx_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i; uint8_t j;
    for (i = 0U; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (j = 0U; j < 8U; j++) {
            if (crc & 1U) { crc = (crc >> 1) ^ 0xEDB88320UL; }
            else          { crc >>= 1; }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

static void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void) { FLASH->CR |= FLASH_CR_LOCK; }

static int flash_wait_ready(void)
{
    volatile uint32_t timeout = FLASH_TIMEOUT_CYCLES;
    while ((FLASH->SR & FLASH_SR_BSY) && --timeout) { }
    if (timeout == 0U) { return -1; }
    FLASH->SR = FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR |
                FLASH_SR_WRPERR | FLASH_SR_OPERR;
    return 0;
}

int knx_flash_erase(void)
{
    if (flash_wait_ready() != 0) { return -1; }
    flash_unlock();
    FLASH->CR &= ~(FLASH_CR_SNB | FLASH_CR_PSIZE);
    FLASH->CR |= (FLASH_SECTOR_7_NUM << FLASH_CR_SNB_Pos) |
                 FLASH_CR_PSIZE_1 | FLASH_CR_SER;
    FLASH->CR |= FLASH_CR_STRT;
    if (flash_wait_ready() != 0) {
        FLASH->CR &= ~FLASH_CR_SER; flash_lock(); return -1;
    }
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    flash_lock();
    return 0;
}

int knx_flash_write(knx_flash_config_t *cfg)
{
    if (!cfg) { return -1; }
    cfg->crc32 = knx_crc32((const uint8_t *)cfg,
                            sizeof(knx_flash_config_t) - sizeof(uint32_t));
    if (knx_flash_erase() != 0) { return -1; }
    if (flash_wait_ready() != 0) { return -1; }
    flash_unlock();
    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    const uint32_t *src = (const uint32_t *)(const void *)cfg;
    volatile uint32_t *dst = (volatile uint32_t *)KNX_FLASH_BASE_ADDR;
    uint32_t words = (sizeof(knx_flash_config_t) + 3U) / 4U;
    uint32_t i;
    for (i = 0U; i < words; i++) {
        dst[i] = src[i];
        if (flash_wait_ready() != 0) {
            FLASH->CR &= ~FLASH_CR_PG; flash_lock(); return -1;
        }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
    if (memcmp((const void *)KNX_FLASH_BASE_ADDR, cfg,
               sizeof(knx_flash_config_t)) != 0) { return -1; }
    return 0;
}

int knx_flash_read(knx_flash_config_t *cfg)
{
    if (!cfg) { return -1; }
    const knx_flash_config_t *flash_cfg =
        (const knx_flash_config_t *)KNX_FLASH_BASE_ADDR;
    if (flash_cfg->magic != KNX_FLASH_MAGIC) { goto load_defaults; }
    uint32_t expected_crc =
        knx_crc32((const uint8_t *)flash_cfg,
                  sizeof(knx_flash_config_t) - sizeof(uint32_t));
    if (flash_cfg->crc32 != expected_crc) { goto load_defaults; }
    memcpy(cfg, flash_cfg, sizeof(knx_flash_config_t));
    return 0;
load_defaults:
    memset(cfg, 0, sizeof(knx_flash_config_t));
    cfg->magic            = KNX_FLASH_MAGIC;
    cfg->physical_address = 0x0000U;
    cfg->num_objects      = 0U;
    return -1;
}
