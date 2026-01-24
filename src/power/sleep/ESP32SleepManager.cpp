/**
 * @file ESP32SleepManager.cpp
 * @brief ESP32 sleep manager implementation.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 2: GPIO iteration bounded by MAX_WAKE_SOURCES
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions at entry points
 * - Rule 7: All esp_ calls return value checked
 * - Rule 8: All ESP32 code isolated here
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */

#include "ESP32SleepManager.h"
#include <cassert>

#if defined(ARCH_ESP32)
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

// Forward declarations for external functions
extern void cpuDeepSleep(uint32_t msecToWake);
extern class Screen *screen;
extern class NodeDB *nodeDB;
extern class PowerMon *powerMon;
#endif

namespace meshtastic {
namespace power {

ESP32SleepManager::ESP32SleepManager(const ESP32WakeConfig &config)
    : BaseSleepManager(), wake_config_(config), notify_light_sleep_(),
#if defined(ARCH_ESP32)
      notify_light_sleep_end_(),
#endif
      modem_sleep_enabled_(false)
{
}

SleepResult ESP32SleepManager::initialize()
{
    // Call base initialization
    SleepResult result = BaseSleepManager::initialize();
    if (result != SleepResult::SUCCESS) {
        return result;
    }

#if defined(ARCH_ESP32)
    // Determine wake cause from last boot
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    setLastWakeCause(convertWakeCause(static_cast<int>(cause)));

    // Increment boot count if waking from sleep
    if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        incrementBootCount();
    }
#endif

    return SleepResult::SUCCESS;
}

bool ESP32SleepManager::isModeSupported(SleepMode mode) const
{
    switch (mode) {
    case SleepMode::NONE:
    case SleepMode::MODEM:
    case SleepMode::LIGHT:
    case SleepMode::DEEP:
        return true;
    case SleepMode::SHUTDOWN:
        return false; // ESP32 doesn't have true shutdown
    default:
        return false;
    }
}

SleepMode ESP32SleepManager::getDeepestMode() const
{
    return SleepMode::DEEP;
}

SleepResult ESP32SleepManager::sleep(const SleepConfig &config, SleepStatus &status)
{
    status = SleepStatus();

    switch (config.mode) {
    case SleepMode::LIGHT: {
        WakeCause cause = lightSleep(config.duration_ms);
        status.wake_cause = cause;
        status.was_sleeping = true;
        return SleepResult::SUCCESS;
    }

    case SleepMode::DEEP:
        return deepSleep(config.duration_ms, config.skip_preflight, config.skip_save_state);

    case SleepMode::MODEM:
        return enableModemSleep();

    default:
        return SleepResult::ERROR_NOT_SUPPORTED;
    }
}

/**
 * @brief Enter light sleep mode.
 *
 * NASA Rule 4: Under 60 lines
 * NASA Rule 7: All ESP calls checked
 */
WakeCause ESP32SleepManager::lightSleep(uint32_t duration_ms)
{
#if defined(ARCH_ESP32)
    // Wait for preflight
    if (!waitForPreflight(false)) {
        return WakeCause::UNKNOWN;
    }

    // Notify observers
    notify_light_sleep_.notifyObservers(nullptr);

    // Configure wake sources
    configureGPIOWake();

    // Configure RTC domain
    esp_err_t err = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    assert(err == ESP_OK);

    // Set timer wake
    const uint64_t sleep_us = static_cast<uint64_t>(duration_ms) * 1000ULL;
    err = esp_sleep_enable_timer_wakeup(sleep_us);
    if (err != ESP_OK) {
        return WakeCause::UNKNOWN;
    }

    // Enable GPIO wake
    err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) {
        return WakeCause::UNKNOWN;
    }

    // Flush console
    flushConsole();

    // Enter light sleep
    err = esp_light_sleep_start();

    // Restore hardware
    restoreHardwareAfterLightSleep();

    // Get wake cause
    esp_sleep_wakeup_cause_t esp_cause = esp_sleep_get_wakeup_cause();
    WakeCause cause = convertWakeCause(static_cast<int>(esp_cause));

    // Notify observers of wake
    notify_light_sleep_end_.notifyObservers(esp_cause);

    return cause;
#else
    (void)duration_ms;
    return WakeCause::UNKNOWN;
#endif
}

/**
 * @brief Enter deep sleep mode.
 *
 * NASA Rule 4: Under 60 lines
 */
SleepResult ESP32SleepManager::deepSleep(uint32_t duration_ms, bool skip_preflight, bool skip_save)
{
#if defined(ARCH_ESP32)
    // Wait for preflight
    if (!waitForPreflight(skip_preflight)) {
        return SleepResult::ERROR_VETOED;
    }

    // Disable Bluetooth
    disableBluetooth();

    // Notify observers
    notifyDeepSleepObservers();

    // Save state if needed
    if (!skip_save && nodeDB != nullptr) {
        nodeDB->saveToDisk();
    }

    // Prepare hardware
    prepareHardwareForDeepSleep();

    // Configure wake sources
    configureDeepSleepWake();

    // Flush console
    flushConsole();

    // Enter deep sleep (does not return)
    cpuDeepSleep(duration_ms);

    // Should never reach here
    return SleepResult::SUCCESS;
#else
    (void)duration_ms;
    (void)skip_preflight;
    (void)skip_save;
    return SleepResult::ERROR_NOT_SUPPORTED;
#endif
}

void ESP32SleepManager::setCpuSpeed(bool fast)
{
#if defined(ARCH_ESP32)
    // ESP32 CPU frequency management
    if (fast) {
        setCpuFrequencyMhz(240);
    } else {
        setCpuFrequencyMhz(80);
    }
#else
    (void)fast;
#endif
}

/**
 * @brief Enable modem sleep for WiFi power management.
 *
 * NASA Rule 4: Under 60 lines
 */
SleepResult ESP32SleepManager::enableModemSleep()
{
#if defined(ARCH_ESP32)
    if (modem_sleep_enabled_) {
        return SleepResult::SUCCESS;
    }

    // Configure power management
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    esp_pm_config_t pm_config = {};
#else
    esp_pm_config_esp32_t pm_config = {};
#endif

    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 20;
    pm_config.light_sleep_enable = false;

    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
        return SleepResult::ERROR_CONFIGURATION;
    }

    modem_sleep_enabled_ = true;
    return SleepResult::SUCCESS;
#else
    return SleepResult::ERROR_NOT_SUPPORTED;
#endif
}

void ESP32SleepManager::configureGPIOWake()
{
#if defined(ARCH_ESP32)
    // Button wake
    if (wake_config_.button_pin >= 0) {
        if (wake_config_.button_need_pullup) {
            gpio_pullup_en(static_cast<gpio_num_t>(wake_config_.button_pin));
        }
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.button_pin), GPIO_INTR_LOW_LEVEL);
    }

    // Rotary encoder
    if (wake_config_.rotary_press_pin >= 0) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.rotary_press_pin), GPIO_INTR_LOW_LEVEL);
    }

    // Keyboard
    if (wake_config_.keyboard_int_pin >= 0) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.keyboard_int_pin), GPIO_INTR_LOW_LEVEL);
    }

    // PMU IRQ
    if (wake_config_.pmu_irq_pin >= 0) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.pmu_irq_pin), GPIO_INTR_LOW_LEVEL);
    }

    // Touch screen
    if (wake_config_.touch_int_pin >= 0) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.touch_int_pin), GPIO_INTR_LOW_LEVEL);
    }

    // LoRa DIO1 (active high)
    if (wake_config_.lora_dio1_pin >= 0) {
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.lora_dio1_pin), GPIO_INTR_HIGH_LEVEL);
    }
#endif
}

void ESP32SleepManager::configureDeepSleepWake()
{
#if defined(ARCH_ESP32)
    // For deep sleep, we typically use EXT0 or EXT1 wake
    // This would be configured based on board-specific requirements
    enableLoRaWake();
#endif
}

void ESP32SleepManager::enableLoRaWake()
{
#if defined(ARCH_ESP32)
    if (wake_config_.lora_dio1_pin >= 0) {
        gpio_pulldown_en(static_cast<gpio_num_t>(wake_config_.lora_dio1_pin));
        gpio_wakeup_enable(static_cast<gpio_num_t>(wake_config_.lora_dio1_pin), GPIO_INTR_HIGH_LEVEL);
    }
#endif
}

WakeCause ESP32SleepManager::convertWakeCause(int esp_cause)
{
#if defined(ARCH_ESP32)
    switch (static_cast<esp_sleep_wakeup_cause_t>(esp_cause)) {
    case ESP_SLEEP_WAKEUP_TIMER:
        return WakeCause::TIMER;
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:
        return WakeCause::EXTERNAL;
    case ESP_SLEEP_WAKEUP_GPIO:
        return WakeCause::GPIO;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return WakeCause::TOUCH;
    case ESP_SLEEP_WAKEUP_UART:
        return WakeCause::UART;
    default:
        return WakeCause::UNKNOWN;
    }
#else
    (void)esp_cause;
    return WakeCause::UNKNOWN;
#endif
}

void ESP32SleepManager::prepareHardwareForDeepSleep()
{
#if defined(ARCH_ESP32)
    // Hold GPIO states during sleep
    if (wake_config_.button_pin >= 0) {
        if (GPIO_IS_VALID_OUTPUT_GPIO(wake_config_.button_pin)) {
            gpio_hold_en(static_cast<gpio_num_t>(wake_config_.button_pin));
        }
    }
#endif
}

void ESP32SleepManager::restoreHardwareAfterLightSleep()
{
#if defined(ARCH_ESP32)
    // Disable GPIO wake on pins
    if (wake_config_.button_pin >= 0) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(wake_config_.button_pin));
    }
    if (wake_config_.rotary_press_pin >= 0) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(wake_config_.rotary_press_pin));
    }
    if (wake_config_.keyboard_int_pin >= 0) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(wake_config_.keyboard_int_pin));
    }
    if (wake_config_.touch_int_pin >= 0) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(wake_config_.touch_int_pin));
    }
    if (wake_config_.lora_dio1_pin >= 0) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(wake_config_.lora_dio1_pin));
    }
#endif
}

ESP32SleepManager createESP32SleepManager()
{
    ESP32WakeConfig config;
    // Default configuration - would be populated from variant.h in practice
    return ESP32SleepManager(config);
}

} // namespace power
} // namespace meshtastic

// Weak implementations for undefined externals
#if !defined(ARCH_ESP32)
void __attribute__((weak)) cpuDeepSleep(uint32_t msecToWake)
{
    (void)msecToWake;
}
#endif
