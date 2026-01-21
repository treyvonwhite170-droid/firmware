/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_SLEEP_H_
#define _PICO_SLEEP_H_

#include "pico.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \file sleep.h
 *  \defgroup hardware_sleep hardware_sleep
 *
 * Lower Power Sleep API
 *
 * The difference between sleep and dormant is that ALL clocks are stopped in dormant mode,
 * until the source (either xosc or rosc) is started again by an external event.
 * In sleep mode some clocks can be left running controlled by the SLEEP_EN registers in the clocks
 * block. For example you could keep clk_rtc running. Some destinations (proc0 and proc1 wakeup logic)
 * can't be stopped in sleep mode otherwise there wouldn't be enough logic to wake up again.
 *
 * RP2350 Note: RP2350 uses POWMAN (Power Manager) and AON Timer instead of RTC for sleep/wake.
 * The LPOSC (Low Power Oscillator) is used for dormant mode timing.
 *
 * \subsection sleep_example Example
 * \addtogroup hardware_sleep
 * \include hello_sleep.c
 */

typedef enum {
    DORMANT_SOURCE_NONE,
    DORMANT_SOURCE_XOSC,
    DORMANT_SOURCE_ROSC,
#ifdef __PLAT_RP2350__
    DORMANT_SOURCE_LPOSC  // RP2350 low-power oscillator
#endif
} dormant_source_t;

/*! \brief Check if dormant source is valid
 *  \ingroup hardware_sleep
 *
 * \param dormant_source The dormant clock source to check
 * \return true if valid
 */
bool dormant_source_valid(dormant_source_t dormant_source);

/*! \brief Set all clock sources to the dormant clock source to prepare for sleep.
 *  \ingroup hardware_sleep
 *
 * \param dormant_source The dormant clock source to use
 */
void sleep_run_from_dormant_source(dormant_source_t dormant_source);

/*! \brief Set the dormant clock source to be the crystal oscillator
 *  \ingroup hardware_sleep
 */
static inline void sleep_run_from_xosc(void)
{
    sleep_run_from_dormant_source(DORMANT_SOURCE_XOSC);
}

/*! \brief Set the dormant clock source to be the ring oscillator
 *  \ingroup hardware_sleep
 */
static inline void sleep_run_from_rosc(void)
{
    sleep_run_from_dormant_source(DORMANT_SOURCE_ROSC);
}

#ifdef __PLAT_RP2350__
/*! \brief Set the dormant clock source to be the low-power oscillator (RP2350 only)
 *  \ingroup hardware_sleep
 *
 * The LPOSC runs at approximately 32kHz and consumes very little power.
 * This is the recommended source for extended dormant periods on RP2350.
 */
static inline void sleep_run_from_lposc(void)
{
    sleep_run_from_dormant_source(DORMANT_SOURCE_LPOSC);
}
#endif

#ifdef __PLAT_RP2040__
#include "hardware/rtc.h"

/*! \brief Send system to sleep until the specified time (RP2040)
 *  \ingroup hardware_sleep
 *
 * One of the sleep_run_* functions must be called prior to this call
 *
 * \param t The time to wake up (datetime_t format)
 * \param callback Function to call on wakeup.
 */
void sleep_goto_sleep_until(datetime_t *t, rtc_callback_t callback);

#else // RP2350

/*! \brief Send system to sleep for specified milliseconds (RP2350)
 *  \ingroup hardware_sleep
 *
 * One of the sleep_run_* functions must be called prior to this call.
 * Uses AON timer for wakeup.
 *
 * \param sleep_ms Duration to sleep in milliseconds
 * \param callback Function to call on wakeup (can be NULL)
 */
void sleep_goto_sleep_for_ms(uint32_t sleep_ms, void (*callback)(void));

/*! \brief Send system to dormant until specified milliseconds (RP2350)
 *  \ingroup hardware_sleep
 *
 * All clocks are stopped. Uses LPOSC + AON timer for wakeup.
 * Lower power than sleep but less accurate timing.
 *
 * \param dormant_ms Duration to stay dormant in milliseconds
 * \param callback Function to call on wakeup (can be NULL)
 */
void sleep_goto_dormant_for_ms(uint32_t dormant_ms, void (*callback)(void));

#endif

/*! \brief Send system to sleep until the specified GPIO changes
 *  \ingroup hardware_sleep
 *
 * One of the sleep_run_* functions must be called prior to this call
 *
 * \param gpio_pin The pin to provide the wake up
 * \param edge true for leading edge, false for trailing edge
 * \param high true for active high, false for active low
 */
void sleep_goto_dormant_until_pin(uint gpio_pin, bool edge, bool high);

/*! \brief Send system to sleep until a leading high edge is detected on GPIO
 *  \ingroup hardware_sleep
 *
 * One of the sleep_run_* functions must be called prior to this call
 *
 * \param gpio_pin The pin to provide the wake up
 */
static inline void sleep_goto_dormant_until_edge_high(uint gpio_pin)
{
    sleep_goto_dormant_until_pin(gpio_pin, true, true);
}

/*! \brief Send system to sleep until a high level is detected on GPIO
 *  \ingroup hardware_sleep
 *
 * One of the sleep_run_* functions must be called prior to this call
 *
 * \param gpio_pin The pin to provide the wake up
 */
static inline void sleep_goto_dormant_until_level_high(uint gpio_pin)
{
    sleep_goto_dormant_until_pin(gpio_pin, false, true);
}

/*! \brief Restore clocks and peripherals after sleep/dormant
 *  \ingroup hardware_sleep
 *
 * Call this after waking from sleep or dormant mode to restore
 * normal clock configuration.
 */
void sleep_power_up(void);

#ifdef __PLAT_RP2350__

// ============================================================
// FreeRTOS and Dual-Core Support
// ============================================================

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)

/*! \brief Prepare for sleep by suspending FreeRTOS tasks
 *  \ingroup hardware_sleep
 *
 * Call this before entering any sleep mode when using FreeRTOS.
 * This function:
 * 1. Suspends the FreeRTOS scheduler
 * 2. Waits for core1 to be idle (if using dual-core)
 *
 * \note Must be paired with sleep_freertos_resume()
 */
void sleep_freertos_prepare(void);

/*! \brief Resume FreeRTOS after waking from sleep
 *  \ingroup hardware_sleep
 *
 * Call this after waking from sleep to resume normal operation.
 * This function resumes the FreeRTOS scheduler.
 */
void sleep_freertos_resume(void);

#endif // HAS_FREE_RTOS

/*! \brief Stop core1 before entering sleep/dormant mode
 *  \ingroup hardware_sleep
 *
 * When using dual-core operation, core1 must be stopped before
 * entering sleep or dormant modes to prevent undefined behavior.
 *
 * This function sends a signal to core1 to stop and waits for
 * confirmation. After waking, call sleep_core1_resume() to restart.
 */
void sleep_core1_stop(void);

/*! \brief Resume core1 after waking from sleep
 *  \ingroup hardware_sleep
 *
 * Restarts core1 execution after waking from sleep/dormant mode.
 * The core1 entry function must be set before calling this.
 *
 * \param entry Function pointer for core1 to execute
 */
void sleep_core1_resume(void (*entry)(void));

/*! \brief Check if core1 is currently running
 *  \ingroup hardware_sleep
 *
 * \return true if core1 is active, false if stopped
 */
bool sleep_core1_is_running(void);

// ============================================================
// Shutdown Functions
// ============================================================

/*! \brief Shutdown the board until USB power is connected (RP2350)
 *  \ingroup hardware_sleep
 *
 * This function puts the RP2350 into the lowest possible power state
 * (dormant mode with minimal power consumption). The board will wake
 * when USB power (VBUS) is detected or when the 5V pin receives power.
 *
 * On XIAO RP2350 and similar boards, VBUS detection is typically on GPIO24.
 * When VBUS goes high (USB connected), the board wakes from dormant mode.
 *
 * If VBUS is already present, this function will NOT enter shutdown mode
 * to prevent immediate wake loops. Instead, it will return false.
 *
 * FreeRTOS Note: This function automatically handles FreeRTOS task
 * suspension and core1 shutdown when HAS_FREE_RTOS is defined.
 *
 * Power consumption in shutdown: ~0.6-1.2mA (board dependent)
 * For true µA-level shutdown, external power switching is required.
 *
 * \param vbus_gpio The GPIO pin connected to VBUS sense (default: 24)
 * \return true if shutdown was entered and woke up, false if VBUS already present
 *
 * \note After waking, the function calls sleep_power_up() to restore clocks.
 *       A full reboot is recommended for reliable peripheral reinitialization.
 */
bool sleep_shutdown_until_usb(uint vbus_gpio);

/*! \brief Shutdown with default VBUS pin (GPIO24)
 *  \ingroup hardware_sleep
 */
static inline bool sleep_shutdown_until_usb_default(void)
{
    return sleep_shutdown_until_usb(24); // Default VBUS sense pin
}

/*! \brief Enter ultra-low-power shutdown mode (RP2350)
 *  \ingroup hardware_sleep
 *
 * This function enters the lowest possible power state by:
 * 1. Disabling all unnecessary clocks
 * 2. Entering dormant mode
 * 3. Waiting for the specified GPIO to go high
 *
 * Unlike sleep_shutdown_until_usb(), this function:
 * - Does NOT check if the wake pin is already high
 * - Will block until the pin goes high
 * - Is suitable for any GPIO wake source (button, external signal, etc.)
 *
 * FreeRTOS Note: This function automatically handles FreeRTOS task
 * suspension and core1 shutdown when HAS_FREE_RTOS is defined.
 *
 * \param wake_gpio The GPIO pin to wake on (level high)
 * \param reboot If true, reboot after wake; if false, just restore clocks
 */
void sleep_shutdown_until_gpio(uint wake_gpio, bool reboot);

#endif // __PLAT_RP2350__

#ifdef __cplusplus
}
#endif

#endif
