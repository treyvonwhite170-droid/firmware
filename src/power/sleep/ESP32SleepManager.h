/**
 * @file ESP32SleepManager.h
 * @brief ESP32-specific sleep manager implementation.
 *
 * Provides light sleep, deep sleep, and modem sleep support
 * for ESP32/ESP32-S3/ESP32-C3 platforms.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Clear sleep state machine
 * - Rule 4: Functions under 60 lines
 * - Rule 7: All esp_ API calls checked
 * - Rule 8: Platform code isolated to this file
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "BaseSleepManager.h"

#if defined(ARCH_ESP32)
#include <esp_sleep.h>
#endif

namespace meshtastic {
namespace power {

/**
 * @brief ESP32 wake source configuration.
 */
struct ESP32WakeConfig {
    int8_t button_pin;           ///< Button wake pin (-1 if not used)
    int8_t lora_dio1_pin;        ///< LoRa DIO1 interrupt pin
    int8_t pmu_irq_pin;          ///< PMU IRQ pin (-1 if not used)
    int8_t rotary_press_pin;     ///< Rotary encoder press pin
    int8_t keyboard_int_pin;     ///< Keyboard interrupt pin
    int8_t touch_int_pin;        ///< Touch screen interrupt pin
    bool button_need_pullup;     ///< Button needs internal pullup
    bool use_ext0_wakeup;        ///< Use EXT0 for single GPIO wake
    bool use_ext1_wakeup;        ///< Use EXT1 for multiple GPIO wake

    ESP32WakeConfig()
        : button_pin(-1), lora_dio1_pin(-1), pmu_irq_pin(-1), rotary_press_pin(-1), keyboard_int_pin(-1), touch_int_pin(-1),
          button_need_pullup(false), use_ext0_wakeup(true), use_ext1_wakeup(false)
    {
    }
};

/**
 * @brief ESP32 sleep manager implementation.
 *
 * Supports:
 * - Modem sleep (WiFi/BT power management)
 * - Light sleep (fast wake, RAM preserved)
 * - Deep sleep (slow wake, RTC memory only)
 */
class ESP32SleepManager : public BaseSleepManager {
  public:
    /**
     * @brief Construct with wake configuration.
     *
     * @param config Wake source configuration
     */
    explicit ESP32SleepManager(const ESP32WakeConfig &config);

    // ISleepManager interface
    SleepResult initialize() override;
    bool isModeSupported(SleepMode mode) const override;
    SleepMode getDeepestMode() const override;
    SleepResult sleep(const SleepConfig &config, SleepStatus &status) override;
    WakeCause lightSleep(uint32_t duration_ms) override;
    SleepResult deepSleep(uint32_t duration_ms, bool skip_preflight, bool skip_save) override;
    void setCpuSpeed(bool fast) override;
    SleepResult enableModemSleep() override;
    const char *getPlatformName() const override { return "ESP32"; }

    /**
     * @brief Get the light sleep observable.
     *
     * ESP32-specific: notified before entering light sleep.
     *
     * @return Reference to light sleep observable
     */
    Observable<void *> &getLightSleepObservable() { return notify_light_sleep_; }

#if defined(ARCH_ESP32)
    /**
     * @brief Get the light sleep end observable.
     *
     * ESP32-specific: notified after waking from light sleep
     * with the wake cause.
     *
     * @return Reference to light sleep end observable
     */
    Observable<esp_sleep_wakeup_cause_t> &getLightSleepEndObservable() { return notify_light_sleep_end_; }
#endif

  private:
    ESP32WakeConfig wake_config_;              ///< Wake source configuration
    Observable<void *> notify_light_sleep_;    ///< Light sleep notification
#if defined(ARCH_ESP32)
    Observable<esp_sleep_wakeup_cause_t> notify_light_sleep_end_; ///< Light sleep end notification
#endif
    bool modem_sleep_enabled_;                 ///< Modem sleep state

    /**
     * @brief Configure GPIO wake sources for light sleep.
     */
    void configureGPIOWake();

    /**
     * @brief Configure GPIO wake sources for deep sleep.
     */
    void configureDeepSleepWake();

    /**
     * @brief Enable LoRa interrupt for wake.
     */
    void enableLoRaWake();

    /**
     * @brief Convert ESP wake cause to our enum.
     *
     * @param esp_cause ESP-IDF wake cause
     * @return Platform-agnostic WakeCause
     */
    WakeCause convertWakeCause(int esp_cause);

    /**
     * @brief Prepare hardware for deep sleep.
     *
     * Holds GPIO states, disables peripherals, etc.
     */
    void prepareHardwareForDeepSleep();

    /**
     * @brief Restore hardware after light sleep.
     */
    void restoreHardwareAfterLightSleep();
};

/**
 * @brief Create ESP32 sleep manager with default configuration.
 *
 * @return Configured ESP32SleepManager
 */
ESP32SleepManager createESP32SleepManager();

} // namespace power
} // namespace meshtastic
