/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico.h"
#include "pico/sleep.h"
#include "pico/stdlib.h"

#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/xosc.h"
// For __wfi
#include "hardware/sync.h"
// For scb_hw so we can enable deep sleep
#include "hardware/structs/scb.h"

#ifdef __PLAT_RP2040__
#include "hardware/rosc.h"
#include "hardware/rtc.h"
#endif

#ifdef __PLAT_RP2350__
#include "hardware/powman.h"
#include "hardware/ticks.h"
#include "hardware/structs/powman.h"
#include "hardware/rosc.h"    // For rosc_set_dormant() in ROSC dormant mode
#include "hardware/watchdog.h" // For watchdog_reboot() in shutdown functions
#endif

// when using old SDK this macro is not defined
#ifndef XOSC_HZ
#define XOSC_HZ 12000000u
#endif

// The difference between sleep and dormant is that ALL clocks are stopped in dormant mode,
// until the source (either xosc or rosc) is started again by an external event.
// In sleep mode some clocks can be left running controlled by the SLEEP_EN registers in the clocks
// block. For example you could keep clk_rtc running. Some destinations (proc0 and proc1 wakeup logic)
// can't be stopped in sleep mode otherwise there wouldn't be enough logic to wake up again.

static dormant_source_t _dormant_source = DORMANT_SOURCE_NONE;

#ifdef __PLAT_RP2350__
// LPOSC frequency is approximately 32kHz
#define LPOSC_HZ 32768u
// Track if we need to restore XOSC timebase after wake
static bool _was_using_xosc = false;
// Wakeup callback for RP2350
static void (*_rp2350_wakeup_callback)(void) = NULL;
// Flag for wakeup
static volatile bool _rp2350_awake = false;
#endif

bool dormant_source_valid(dormant_source_t dormant_source)
{
#ifdef __PLAT_RP2350__
    return (dormant_source == DORMANT_SOURCE_XOSC) ||
           (dormant_source == DORMANT_SOURCE_ROSC) ||
           (dormant_source == DORMANT_SOURCE_LPOSC);
#else
    return (dormant_source == DORMANT_SOURCE_XOSC) || (dormant_source == DORMANT_SOURCE_ROSC);
#endif
}

#ifdef __PLAT_RP2040__
// RP2040 implementation - uses RTC and ROSC/XOSC

// In order to go into dormant mode we need to be running from a stoppable clock source:
// either the xosc or rosc with no PLLs running. This means we disable the USB and ADC clocks
// and all PLLs
void sleep_run_from_dormant_source(dormant_source_t dormant_source)
{
    assert(dormant_source_valid(dormant_source));
    _dormant_source = dormant_source;

    // FIXME: Just defining average rosc freq here.
    uint src_hz = (dormant_source == DORMANT_SOURCE_XOSC) ? XOSC_HZ : 6.5 * MHZ;
    uint clk_ref_src = (dormant_source == DORMANT_SOURCE_XOSC) ? CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC
                                                               : CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH;

    // CLK_REF = XOSC or ROSC
    clock_configure(clk_ref, clk_ref_src,
                    0, // No aux mux
                    src_hz, src_hz);

    // CLK SYS = CLK_REF
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,
                    0, // Using glitchless mux
                    src_hz, src_hz);

    // CLK USB = 0MHz
    clock_stop(clk_usb);

    // CLK ADC = 0MHz
    clock_stop(clk_adc);

    // CLK RTC = ideally XOSC (12MHz) / 256 = 46875Hz but could be rosc
    uint clk_rtc_src = (dormant_source == DORMANT_SOURCE_XOSC) ? CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC
                                                               : CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_ROSC_CLKSRC_PH;

    clock_configure(clk_rtc,
                    0, // No GLMUX
                    clk_rtc_src, src_hz, 46875);

    // CLK PERI = clk_sys. Used as reference clock for Peripherals. No dividers so just select and enable
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, src_hz, src_hz);

    pll_deinit(pll_sys);
    pll_deinit(pll_usb);

    // Assuming both xosc and rosc are running at the moment
    if (dormant_source == DORMANT_SOURCE_XOSC) {
        // Can disable rosc
        rosc_disable();
    } else {
        // Can disable xosc
        xosc_disable();
    }
}

// Go to sleep until woken up by the RTC
void sleep_goto_sleep_until(datetime_t *t, rtc_callback_t callback)
{
    // We should have already called the sleep_run_from_dormant_source function
    assert(dormant_source_valid(_dormant_source));

    // Turn off all clocks when in sleep mode except for RTC
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS;
    clocks_hw->sleep_en1 = 0x0;

    rtc_set_alarm(t, callback);

    uint save = scb_hw->scr;
    // Enable deep sleep at the proc
    scb_hw->scr = save | M0PLUS_SCR_SLEEPDEEP_BITS;

    // Go to sleep
    __wfi();
}

static void _go_dormant(void)
{
    assert(dormant_source_valid(_dormant_source));

    if (_dormant_source == DORMANT_SOURCE_XOSC) {
        xosc_dormant();
    } else {
        rosc_set_dormant();
    }
}

void sleep_goto_dormant_until_pin(uint gpio_pin, bool edge, bool high)
{
    bool low = !high;
    bool level = !edge;

    // Configure the appropriate IRQ at IO bank 0
    assert(gpio_pin < NUM_BANK0_GPIOS);

    uint32_t event = 0;

    if (level && low)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_LEVEL_LOW_BITS;
    if (level && high)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_LEVEL_HIGH_BITS;
    if (edge && high)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS;
    if (edge && low)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_LOW_BITS;

    gpio_set_dormant_irq_enabled(gpio_pin, event, true);

    _go_dormant();
    // Execution stops here until woken up

    // Clear the irq so we can go back to dormant mode again if we want
    gpio_acknowledge_irq(gpio_pin, event);
}

void sleep_power_up(void)
{
    // Re-enable the ring oscillator, which will bring back all clocks
    rosc_enable();

    // Reset clocks to default state
    clocks_init();
}

#else // RP2350 implementation

// RP2350 uses POWMAN (Power Manager) and AON Timer instead of RTC
// The LPOSC (Low Power Oscillator) provides timing during dormant mode

void sleep_run_from_dormant_source(dormant_source_t dormant_source)
{
    assert(dormant_source_valid(dormant_source));
    _dormant_source = dormant_source;

    // Determine source frequency
    uint src_hz;
    if (dormant_source == DORMANT_SOURCE_LPOSC) {
        src_hz = LPOSC_HZ;
        _was_using_xosc = true;
    } else if (dormant_source == DORMANT_SOURCE_XOSC) {
        src_hz = XOSC_HZ;
        _was_using_xosc = false;
    } else {
        // ROSC - approximate frequency
        src_hz = 6500000u; // ~6.5 MHz
        _was_using_xosc = false;
    }

    // For LPOSC, we configure POWMAN to use the low-power oscillator
    if (dormant_source == DORMANT_SOURCE_LPOSC) {
        // Switch POWMAN timer to use LPOSC
        // The LPOSC is always running on RP2350
        powman_timer_set_1khz_tick_source_lposc();
    } else {
        // Use XOSC for POWMAN timer (more accurate but more power)
        powman_timer_set_1khz_tick_source_xosc();
    }

    // Stop unnecessary clocks for low power
    clock_stop(clk_usb);
    clock_stop(clk_adc);
    clock_stop(clk_hstx); // RP2350 has HSTX clock

    // Configure reference clock based on source
    if (dormant_source == DORMANT_SOURCE_XOSC || dormant_source == DORMANT_SOURCE_LPOSC) {
        // Use XOSC as reference (or prepare for LPOSC dormant)
        clock_configure(clk_ref,
                        CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                        0,
                        XOSC_HZ,
                        XOSC_HZ);
    } else {
        // Use ROSC as reference
        clock_configure(clk_ref,
                        CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH,
                        0,
                        src_hz,
                        src_hz);
    }

    // CLK_SYS from CLK_REF
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,
                    0,
                    (dormant_source == DORMANT_SOURCE_ROSC) ? src_hz : XOSC_HZ,
                    (dormant_source == DORMANT_SOURCE_ROSC) ? src_hz : XOSC_HZ);

    // Disable PLLs to save power
    pll_deinit(pll_sys);
    pll_deinit(pll_usb);

    // If using XOSC/LPOSC, we can disable ROSC
    // Note: On RP2350, be careful about disabling oscillators
    if (dormant_source == DORMANT_SOURCE_XOSC || dormant_source == DORMANT_SOURCE_LPOSC) {
        // ROSC can be disabled when using XOSC
        // But keep it running for now as some peripherals might need it
    }
}

void sleep_goto_sleep_for_ms(uint32_t sleep_ms, void (*callback)(void))
{
    assert(dormant_source_valid(_dormant_source));

    _rp2350_awake = false;
    _rp2350_wakeup_callback = callback;

    // Get current POWMAN time
    uint64_t now_ms = powman_timer_get_ms();

    // Set alarm for wake time
    uint64_t wake_time_ms = now_ms + sleep_ms;
    powman_timer_enable_alarm_at_ms(wake_time_ms);

    // Configure sleep - keep some clocks running for faster wake
    // Enable clocks needed for POWMAN timer
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_SYS_TIMER0_BITS |
                           CLOCKS_SLEEP_EN0_CLK_SYS_TIMER1_BITS;
    clocks_hw->sleep_en1 = 0;

    // Enable deep sleep
    uint save = scb_hw->scr;
    scb_hw->scr = save | M33_SCR_SLEEPDEEP_BITS;

    // Wait for interrupt (sleep)
    __wfi();

    // Restore SCR
    scb_hw->scr = save;

    // Disable the alarm
    powman_timer_disable_alarm();
}

void sleep_goto_dormant_for_ms(uint32_t dormant_ms, void (*callback)(void))
{
    assert(dormant_source_valid(_dormant_source));

    _rp2350_awake = false;
    _rp2350_wakeup_callback = callback;

    // For dormant mode, we need to use LPOSC as it keeps running
    // Make sure POWMAN timer uses LPOSC
    powman_timer_set_1khz_tick_source_lposc();

    // Get current time and set alarm
    uint64_t now_ms = powman_timer_get_ms();
    uint64_t wake_time_ms = now_ms + dormant_ms;
    powman_timer_enable_alarm_at_ms(wake_time_ms);

    // Disable all clocks for dormant - POWMAN will wake us
    clocks_hw->sleep_en0 = 0;
    clocks_hw->sleep_en1 = 0;

    // Enable deep sleep
    uint save = scb_hw->scr;
    scb_hw->scr = save | M33_SCR_SLEEPDEEP_BITS;

    // Enter dormant - XOSC will stop
    xosc_dormant();

    // We wake up here
    // Restore SCR
    scb_hw->scr = save;

    // Disable the alarm
    powman_timer_disable_alarm();

    // Call callback if set
    if (_rp2350_wakeup_callback) {
        _rp2350_wakeup_callback();
    }
}

void sleep_goto_dormant_until_pin(uint gpio_pin, bool edge, bool high)
{
    bool low = !high;
    bool level = !edge;

    assert(gpio_pin < NUM_BANK0_GPIOS);

    uint32_t event = 0;

    if (level && low)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_LEVEL_LOW_BITS;
    if (level && high)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_LEVEL_HIGH_BITS;
    if (edge && high)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_HIGH_BITS;
    if (edge && low)
        event = IO_BANK0_DORMANT_WAKE_INTE0_GPIO0_EDGE_LOW_BITS;

    // Enable dormant wake interrupt for the GPIO
    gpio_set_dormant_irq_enabled(gpio_pin, event, true);

    // On RP2350, we can also configure POWMAN GPIO wakeup for lower power
    // Note: RP2350 supports max 4 GPIO wakeup sources via POWMAN
    // For simplicity, use the standard dormant wake mechanism here

    // Enter dormant mode based on configured source
    if (_dormant_source == DORMANT_SOURCE_ROSC) {
        // Use ROSC dormant mode
        rosc_set_dormant();
    } else {
        // XOSC or LPOSC: use XOSC dormant
        // LPOSC continues running during XOSC dormant for POWMAN timer
        xosc_dormant();
    }

    // Clear the irq so we can go back to dormant mode again if we want
    gpio_acknowledge_irq(gpio_pin, event);
}

void sleep_power_up(void)
{
    // Restore XOSC timebase for POWMAN if we were using it
    if (_was_using_xosc) {
        powman_timer_set_1khz_tick_source_xosc();
    }

    // Reset clocks to default state - this re-enables PLLs
    clocks_init();

    // Re-initialize ticks for proper timing
    tick_init();

    _dormant_source = DORMANT_SOURCE_NONE;
}

// ============================================================
// FreeRTOS and Dual-Core Support
// ============================================================

#include "pico/multicore.h"

// Track if core1 was running before sleep
// Use volatile for multicore-safe access
static volatile bool _core1_is_active = false;
static volatile bool _core1_was_stopped_for_sleep = false;
static void (*_core1_entry_func)(void) = NULL;

// Check if we're running on core0
static inline bool _is_core0(void) {
    return get_core_num() == 0;
}

void sleep_core1_stop(void)
{
    // Only core0 should call this
    if (!_is_core0()) {
        return;
    }

    // Only stop if core1 is actually active
    if (!_core1_is_active) {
        return;
    }

    // Mark that we stopped core1 for sleep (to know we should resume it)
    _core1_was_stopped_for_sleep = true;
    _core1_is_active = false;

    // Reset core1 - this stops it safely
    multicore_reset_core1();

    // Small delay required after reset before any relaunch
    // See: https://github.com/raspberrypi/pico-sdk/issues/1977
    sleep_ms(10);
}

void sleep_core1_resume(void (*entry)(void))
{
    if (!_is_core0()) {
        return;
    }

    if (entry != NULL) {
        _core1_entry_func = entry;
    }

    // Only resume if we stopped it for sleep and have an entry function
    if (_core1_entry_func != NULL && _core1_was_stopped_for_sleep) {
        // Relaunch core1 with the entry function
        multicore_launch_core1(_core1_entry_func);
        _core1_is_active = true;
    }

    _core1_was_stopped_for_sleep = false;
}

bool sleep_core1_is_running(void)
{
    // Return whether core1 is currently active
    // This flag is set when core1 is launched and cleared when stopped
    return _core1_is_active;
}

void sleep_core1_set_active(bool active, void (*entry)(void))
{
    // Called to mark core1 as active after launching it externally
    // This allows the sleep system to properly track and stop/resume core1
    _core1_is_active = active;
    if (entry != NULL) {
        _core1_entry_func = entry;
    }
}

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)

#include <FreeRTOS.h>
#include <task.h>

void sleep_freertos_prepare(void)
{
    // Suspend the FreeRTOS scheduler
    // This prevents task switches during sleep preparation
    vTaskSuspendAll();

    // Stop core1 if it's running
    // In FreeRTOS SMP mode, this ensures all tasks on core1 are stopped
    sleep_core1_stop();
}

void sleep_freertos_resume(void)
{
    // Resume the FreeRTOS scheduler
    xTaskResumeAll();

    // Note: Core1 resume is typically handled by reboot
    // If not rebooting, the caller should restart core1 tasks manually
}

#endif // HAS_FREE_RTOS || __FREERTOS

// ============================================================
// Shutdown Functions
// ============================================================

bool sleep_shutdown_until_usb(uint vbus_gpio)
{
    // Configure VBUS sense pin as input
    gpio_init(vbus_gpio);
    gpio_set_dir(vbus_gpio, GPIO_IN);
    gpio_pull_down(vbus_gpio); // Pull down when no USB

    // Check if VBUS is already present (USB connected)
    // If so, don't enter shutdown to prevent immediate wake loop
    if (gpio_get(vbus_gpio)) {
        return false; // VBUS already present, don't shutdown
    }

    // Stop core1 before entering dormant mode
    // This is critical for proper low-power operation
    sleep_core1_stop();

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)
    // Suspend FreeRTOS scheduler
    vTaskSuspendAll();
#endif

    // Prepare for lowest power dormant mode
    // Use XOSC for dormant - it will stop and restart on wake
    // Note: sleep_run_from_dormant_source() already stops clk_usb, clk_adc, clk_hstx
    sleep_run_from_dormant_source(DORMANT_SOURCE_XOSC);

    // Enter dormant mode, waiting for VBUS (level high)
    // When USB is plugged in, VBUS goes high and we wake
    sleep_goto_dormant_until_pin(vbus_gpio, false, true); // level, high

    // We've woken up - VBUS is now present
    // Restore all clocks
    sleep_power_up();

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)
    // Resume FreeRTOS scheduler
    xTaskResumeAll();
#endif

    return true;
}

void sleep_shutdown_until_gpio(uint wake_gpio, bool reboot)
{
    // Configure wake pin as input with pull-down
    gpio_init(wake_gpio);
    gpio_set_dir(wake_gpio, GPIO_IN);
    gpio_pull_down(wake_gpio);

    // Stop core1 before entering dormant mode
    sleep_core1_stop();

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)
    // Suspend FreeRTOS scheduler
    vTaskSuspendAll();
#endif

    // Prepare for lowest power dormant mode
    // Note: sleep_run_from_dormant_source() already stops clk_usb, clk_adc, clk_hstx
    sleep_run_from_dormant_source(DORMANT_SOURCE_XOSC);

    // Enter dormant mode, waiting for GPIO level high
    sleep_goto_dormant_until_pin(wake_gpio, false, true);

    // We've woken up
    // Restore clocks first
    sleep_power_up();

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)
    // Resume FreeRTOS scheduler
    xTaskResumeAll();
#endif

    // Note: Core1 is NOT automatically restarted
    // Caller should either reboot or call sleep_core1_resume()

    // If reboot requested, use watchdog to trigger a reset
    // This ensures a clean restart of all peripherals
    if (reboot) {
        watchdog_reboot(0, 0, 0);  // Immediate reboot
        while (1) tight_loop_contents();  // Wait for reset
    }
}

#endif // __PLAT_RP2350__
