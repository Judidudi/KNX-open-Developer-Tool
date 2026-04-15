/**
 * @file main.c
 * @brief STM32F446RE KNX firmware entry point
 *
 * This file is part of the STM32CubeIDE project skeleton.
 * Only knx_init() and knx_process() are added here; all other generated
 * code (system_stm32f4xx.c, startup, linker script) is left untouched.
 *
 * SystemClock_Config() is intentionally left empty — the stack runs on
 * the default 16 MHz HSI clock.
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "knx.h"
#include "knx_hal_stm32.h"

/* Private variables ---------------------------------------------------------*/

/** Example application: callback for received group telegrams */
static void app_on_receive(uint16_t ga, knx_dpt_type_t dpt,
                            knx_dpt_value_t value)
{
    /* Application-specific handling goes here.
     * Example: if GA matches a known object, act on the value. */
    (void)ga;
    (void)dpt;
    (void)value;
}

/* --------------------------------------------------------------------------
 * main()
 * -------------------------------------------------------------------------- */

int main(void)
{
    /* CubeIDE-generated HAL_Init() call would go here if using HAL.
     * We use bare-metal, so just initialise the KNX HAL port directly. */

    knx_hal_t hal;
    knx_hal_stm32_init(&hal);

    /* Initialise the KNX stack */
    knx_status_t status = knx_init(&hal);
    (void)status;  /* In production, signal error via LED pattern */

    /* Register application callback */
    knx_on_receive(app_on_receive);

    /* Main loop */
    while (1) {
        knx_process();

        /* Application code can go here between knx_process() calls.
         * Keep execution time short to not starve the KNX stack. */
    }
}

/* --------------------------------------------------------------------------
 * SystemClock_Config — intentionally empty (16 MHz HSI default)
 * -------------------------------------------------------------------------- */

void SystemClock_Config(void)
{
    /* No PLL, no HSE, no RCC manipulation.
     * The device runs at 16 MHz HSI after power-on reset. */
}

/* --------------------------------------------------------------------------
 * Error handler
 * -------------------------------------------------------------------------- */

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        /* Spin — in production add a watchdog reset here */
    }
}
