#include "configuration.h"
#include "hardware/xosc.h"
#include <hardware/clocks.h>
#include <hardware/pll.h>
#include <pico/stdlib.h>
#include <pico/unique_id.h>
#include <pico/sleep.h>

#ifdef __PLAT_RP2040__

static bool awake;

static void sleep_callback(void)
{
    awake = true;
}

void epoch_to_datetime(time_t epoch, datetime_t *dt)
{
    struct tm *tm_info;

    tm_info = gmtime(&epoch);
    dt->year = tm_info->tm_year;
    dt->month = tm_info->tm_mon + 1;
    dt->day = tm_info->tm_mday;
    dt->dotw = tm_info->tm_wday;
    dt->hour = tm_info->tm_hour;
    dt->min = tm_info->tm_min;
    dt->sec = tm_info->tm_sec;
}

void debug_date(datetime_t t)
{
    LOG_DEBUG("%d %d %d %d %d %d %d", t.year, t.month, t.day, t.hour, t.min, t.sec, t.dotw);
    uart_default_tx_wait_blocking();
}

void cpuDeepSleep(uint32_t msecs)
{

    time_t seconds = (time_t)(msecs / 1000);
    datetime_t t_init, t_alarm;

    awake = false;
    // Start the RTC
    rtc_init();
    epoch_to_datetime(0, &t_init);
    rtc_set_datetime(&t_init);
    epoch_to_datetime(seconds, &t_alarm);
    // debug_date(t_init);
    // debug_date(t_alarm);
    uart_default_tx_wait_blocking();
    sleep_run_from_dormant_source(DORMANT_SOURCE_ROSC);
    sleep_goto_sleep_until(&t_alarm, &sleep_callback);

    // Make sure we don't wake
    while (!awake) {
        delay(1);
    }

    /* For now, I don't know how to revert this state
        We just reboot in order to get back operational */
    rp2040.reboot();

    /* Set RP2040 in dormant mode. Will not wake up. */
    // xosc_dormant();
}

#else // RP2350

static volatile bool rp2350_awake = false;

static void rp2350_sleep_callback(void)
{
    rp2350_awake = true;
}

void cpuDeepSleep(uint32_t msecs)
{
    rp2350_awake = false;

    // Wait for any pending UART transmissions
    uart_default_tx_wait_blocking();

    // Configure for low-power operation using LPOSC
    // LPOSC provides ~32kHz timing with very low power consumption
    sleep_run_from_lposc();

    // Enter dormant mode with timer wakeup
    // This uses the POWMAN AON timer which continues running from LPOSC
    sleep_goto_dormant_for_ms(msecs, &rp2350_sleep_callback);

    // Wait for wakeup confirmation
    while (!rp2350_awake) {
        tight_loop_contents();
    }

    // Restore full clock configuration
    sleep_power_up();

    // Note: Unlike RP2040, RP2350 can recover from dormant mode
    // without a full reboot in many cases. However, for consistency
    // with the existing firmware behavior and to ensure all peripherals
    // are properly reinitialized, we still reboot.
    // If you want to avoid reboot, comment out the line below and
    // ensure all peripherals are properly reinitialized.
    rp2040.reboot();
}

// Alternative sleep function that doesn't reboot (experimental)
void cpuLightSleep(uint32_t msecs)
{
    rp2350_awake = false;

    uart_default_tx_wait_blocking();

    // Use XOSC for more accurate timing but higher power
    sleep_run_from_xosc();

    // Enter sleep mode (not full dormant)
    sleep_goto_sleep_for_ms(msecs, &rp2350_sleep_callback);

    while (!rp2350_awake) {
        tight_loop_contents();
    }

    // Restore clocks
    sleep_power_up();

    // No reboot needed for light sleep
}

// GPIO-triggered dormant mode for RP2350
void cpuDormantUntilPin(uint gpio_pin, bool edge, bool high)
{
    uart_default_tx_wait_blocking();

    // Configure for dormant mode
    sleep_run_from_xosc();

    // Enter dormant until GPIO triggers wakeup
    sleep_goto_dormant_until_pin(gpio_pin, edge, high);

    // Restore clocks
    sleep_power_up();
}

/**
 * @brief Shutdown the board until USB power is connected
 *
 * This function puts the RP2350 into the lowest possible power state.
 * The board will wake and reboot when:
 * - USB-C is plugged in (VBUS detected)
 * - 5V is applied to the 5V input pin
 *
 * The VBUS sense pin (default GPIO24) monitors USB power status.
 * When VBUS goes high, the board wakes from dormant mode and reboots
 * to ensure all peripherals are properly initialized.
 *
 * @param vbus_gpio GPIO pin for VBUS detection (default: VBUS_SENSE_PIN or 24)
 * @return true if shutdown was entered, false if USB already connected
 *
 * Power consumption in shutdown: ~0.6-1.2mA (varies by board)
 *
 * Usage:
 *   if (!cpuShutdown()) {
 *       // USB was already connected, handle accordingly
 *   }
 *   // Will only reach here if wake happened and reboot is disabled
 */
bool cpuShutdown(uint vbus_gpio)
{
    // Log shutdown intent
    LOG_INFO("Entering shutdown mode, waiting for USB power...");
    uart_default_tx_wait_blocking();

    // Try to enter shutdown mode
    bool entered = sleep_shutdown_until_usb(vbus_gpio);

    if (!entered) {
        // USB was already connected
        LOG_INFO("USB already connected, cannot shutdown");
        return false;
    }

    // We've woken up from shutdown - USB is now connected
    LOG_INFO("Woke from shutdown, USB power detected. Rebooting...");
    uart_default_tx_wait_blocking();

    // Reboot for clean peripheral initialization
    rp2040.reboot();

    // Should not reach here
    return true;
}

/**
 * @brief Shutdown with default VBUS pin
 *
 * Uses the board-defined VBUS_SENSE_PIN or falls back to GPIO24
 * (standard Pico 2 VBUS sense pin).
 */
bool cpuShutdownDefault(void)
{
#ifdef VBUS_SENSE_PIN
    return cpuShutdown(VBUS_SENSE_PIN);
#else
    return cpuShutdown(24); // Default Pico 2 VBUS sense
#endif
}

/**
 * @brief Shutdown until a specific GPIO goes high
 *
 * More flexible version that can wake on any GPIO signal.
 * Useful for custom wake sources like external buttons or signals.
 *
 * @param wake_gpio GPIO pin to monitor for wake
 */
void cpuShutdownUntilGpio(uint wake_gpio)
{
    LOG_INFO("Entering shutdown mode, waiting for GPIO%d...", wake_gpio);
    uart_default_tx_wait_blocking();

    sleep_shutdown_until_gpio(wake_gpio, false);

    LOG_INFO("Woke from shutdown. Rebooting...");
    uart_default_tx_wait_blocking();

    rp2040.reboot();
}

// ============================================================
// Core1 Compute Task Utilities
// ============================================================
// These functions help run compute-heavy tasks on core1 while
// keeping core0 free for I/O, networking, and other tasks.
//
// Usage with FreeRTOS SMP:
//   Tasks are automatically distributed across cores by the scheduler.
//   Use xTaskCreatePinnedToCore() to pin compute tasks to core1.
//
// Usage without FreeRTOS (bare metal multicore):
//   Define setup1() and loop1() in your sketch for core1 code.
//   Use rp2040.fifo for inter-core communication.

#include "pico/multicore.h"

// Core1 task function pointer (for bare metal mode)
static void (*_core1_task_func)(void) = NULL;
static volatile bool _core1_task_running = false;

/**
 * @brief Internal core1 entry wrapper
 */
static void _core1_task_wrapper(void)
{
    _core1_task_running = true;
    if (_core1_task_func != NULL) {
        _core1_task_func();
    }
    _core1_task_running = false;

    // Core1 will idle here until reset
    while (1) {
        __wfi(); // Wait for interrupt (low power idle)
    }
}

/**
 * @brief Launch a compute task on core1
 *
 * This function starts a compute-heavy task on core1, leaving core0
 * free for I/O operations. The task runs until completion or until
 * stopCore1Task() is called.
 *
 * @param task_func Function to run on core1 (should be long-running)
 * @return true if task was launched, false if core1 is already busy
 *
 * Example:
 *   void heavyComputation() {
 *       while (!computationDone) {
 *           // Do heavy math, signal processing, etc.
 *           processData();
 *       }
 *   }
 *   launchCore1Task(heavyComputation);
 */
bool launchCore1Task(void (*task_func)(void))
{
    if (_core1_task_running) {
        return false; // Core1 already busy
    }

    _core1_task_func = task_func;

    // Launch core1 with our wrapper
    multicore_launch_core1(_core1_task_wrapper);

    return true;
}

/**
 * @brief Stop the current core1 task
 *
 * Resets core1 to stop any running task. After calling this,
 * a new task can be launched with launchCore1Task().
 */
void stopCore1Task(void)
{
    if (_core1_task_running) {
        multicore_reset_core1();
        _core1_task_running = false;
        _core1_task_func = NULL;
    }
}

/**
 * @brief Check if core1 is running a task
 *
 * @return true if a task is currently running on core1
 */
bool isCore1TaskRunning(void)
{
    return _core1_task_running;
}

/**
 * @brief Get the core number we're currently running on
 *
 * @return 0 for core0, 1 for core1
 */
uint8_t getCurrentCore(void)
{
    return get_core_num();
}

#if defined(HAS_FREE_RTOS) || defined(__FREERTOS)
#include <FreeRTOS.h>
#include <task.h>

/**
 * @brief Create a FreeRTOS task pinned to core1
 *
 * This is a convenience wrapper for xTaskCreatePinnedToCore that
 * always pins the task to core1 for compute-heavy operations.
 *
 * @param taskFunc Task function
 * @param name Task name (for debugging)
 * @param stackSize Stack size in words
 * @param params Parameters to pass to task
 * @param priority Task priority (0-7)
 * @param taskHandle Output handle (can be NULL)
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 *
 * Example:
 *   void computeTask(void *params) {
 *       while (1) {
 *           processData();
 *           vTaskDelay(pdMS_TO_TICKS(10));
 *       }
 *   }
 *   createCore1Task(computeTask, "compute", 4096, NULL, 3, NULL);
 */
BaseType_t createCore1Task(
    TaskFunction_t taskFunc,
    const char *name,
    uint32_t stackSize,
    void *params,
    UBaseType_t priority,
    TaskHandle_t *taskHandle)
{
    // In FreeRTOS SMP, use core affinity to pin to core1
    // Note: This requires FreeRTOS SMP configuration
#if configUSE_CORE_AFFINITY
    TaskHandle_t handle;
    BaseType_t result = xTaskCreate(taskFunc, name, stackSize, params, priority, &handle);
    if (result == pdPASS && handle != NULL) {
        // Set core affinity to core1 only (bitmask: 0b10 = core1)
        vTaskCoreAffinitySet(handle, (1 << 1));
        if (taskHandle != NULL) {
            *taskHandle = handle;
        }
    }
    return result;
#else
    // Fallback: just create the task normally
    return xTaskCreate(taskFunc, name, stackSize, params, priority, taskHandle);
#endif
}

#endif // HAS_FREE_RTOS

#endif // __PLAT_RP2350__

void setBluetoothEnable(bool enable)
{
    // not needed
}

void updateBatteryLevel(uint8_t level)
{
    // not needed
}

void getMacAddr(uint8_t *dmac)
{
    pico_unique_board_id_t src;
    pico_get_unique_board_id(&src);
    dmac[5] = src.id[7];
    dmac[4] = src.id[6];
    dmac[3] = src.id[5];
    dmac[2] = src.id[4];
    dmac[1] = src.id[3];
    dmac[0] = src.id[2];
}

void rp2040Setup()
{
    /* Sets a random seed to make sure we get different random numbers on each boot.
       Taken from CPU cycle counter and ROSC oscillator, so should be pretty random.
    */
    randomSeed(rp2040.hwrand32());

#ifdef RP2040_SLOW_CLOCK
    uint f_pll_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
    uint f_pll_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_USB_CLKSRC_PRIMARY);
    uint f_rosc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC);
    uint f_clk_sys = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
    uint f_clk_peri = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI);
    uint f_clk_usb = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB);
    uint f_clk_adc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC);
    uint f_clk_rtc = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_RTC);

    LOG_INFO("Clock speed:");
    LOG_INFO("pll_sys  = %dkHz", f_pll_sys);
    LOG_INFO("pll_usb  = %dkHz", f_pll_usb);
    LOG_INFO("rosc     = %dkHz", f_rosc);
    LOG_INFO("clk_sys  = %dkHz", f_clk_sys);
    LOG_INFO("clk_peri = %dkHz", f_clk_peri);
    LOG_INFO("clk_usb  = %dkHz", f_clk_usb);
    LOG_INFO("clk_adc  = %dkHz", f_clk_adc);
    LOG_INFO("clk_rtc  = %dkHz", f_clk_rtc);
#endif
}

void enterDfuMode()
{
    reset_usb_boot(0, 0);
}

/* Init in early boot state. */
#ifdef RP2040_SLOW_CLOCK
void initVariant()
{
    /* Set the system frequency to 18 MHz. */
    set_sys_clock_khz(18 * KHZ, false);
    /* The previous line automatically detached clk_peri from clk_sys, and
       attached it to pll_usb. We need to attach clk_peri back to system PLL to keep SPI
       working at this low speed.
       For details see https://github.com/jgromes/RadioLib/discussions/938
    */
    clock_configure(clk_peri,
                    0,                                                // No glitchless mux
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
                    18 * MHZ,                                         // Input frequency
                    18 * MHZ                                          // Output (must be same as no divider)
    );
    /* Run also ADC on lower clk_sys. */
    clock_configure(clk_adc, 0, CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, 18 * MHZ, 18 * MHZ);
    /* Run RTC from XOSC since USB clock is off */
    clock_configure(clk_rtc, 0, CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 12 * MHZ, 47 * KHZ);
    /* Turn off USB PLL */
    pll_deinit(pll_usb);
}
#endif
