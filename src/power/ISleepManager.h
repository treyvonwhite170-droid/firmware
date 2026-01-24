/**
 * @file ISleepManager.h
 * @brief Abstract interface for platform-specific sleep management.
 *
 * This interface abstracts sleep/wake operations across different
 * microcontroller architectures (ESP32, NRF52, RP2040, STM32WL),
 * eliminating architecture-specific #ifdef clusters.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple control flow (state machine pattern)
 * - Rule 3: No dynamic allocation in sleep paths
 * - Rule 4: Functions kept under 60 lines
 * - Rule 7: Explicit result codes for all operations
 * - Rule 8: Minimal preprocessor usage
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include <cstdint>

namespace meshtastic {
namespace power {

/**
 * @brief Maximum number of wake sources that can be configured.
 *
 * Fixed constant per NASA Rule 2 (fixed upper-bound loops).
 */
static constexpr uint8_t MAX_WAKE_SOURCES = 16;

/**
 * @brief Maximum sleep duration in milliseconds.
 *
 * Using UINT32_MAX - 1 to allow for "forever" sleep.
 */
static constexpr uint32_t MAX_SLEEP_MS = 0xFFFFFFFE;

/**
 * @brief Special value indicating sleep forever.
 */
static constexpr uint32_t SLEEP_FOREVER = 0xFFFFFFFF;

/**
 * @brief Result codes for sleep operations.
 */
enum class SleepResult : uint8_t {
    SUCCESS = 0,           ///< Operation completed successfully
    ERROR_NOT_SUPPORTED,   ///< Sleep mode not supported on this platform
    ERROR_BUSY,            ///< Cannot sleep - system busy
    ERROR_VETOED,          ///< Sleep vetoed by observer
    ERROR_CONFIGURATION,   ///< Wake source configuration error
    ERROR_HARDWARE,        ///< Hardware error during sleep
    ERROR_TIMEOUT          ///< Operation timed out
};

/**
 * @brief Wake cause enumeration.
 *
 * Platform-agnostic wake source identification.
 */
enum class WakeCause : uint8_t {
    UNKNOWN = 0,   ///< Wake cause unknown or reset
    TIMER,         ///< Timer/RTC wakeup
    GPIO,          ///< GPIO interrupt (button, etc.)
    UART,          ///< UART activity
    RADIO,         ///< LoRa radio interrupt
    USB,           ///< USB connection/activity
    PMU,           ///< Power management unit interrupt
    TOUCH,         ///< Touch screen interrupt
    ACCELEROMETER, ///< Motion sensor interrupt
    EXTERNAL,      ///< External interrupt (EXT0/EXT1)
    CAUSE_COUNT    ///< Number of wake causes (for bounds)
};

/**
 * @brief Sleep mode enumeration.
 *
 * Defines sleep depth levels available across platforms.
 */
enum class SleepMode : uint8_t {
    NONE = 0,    ///< No sleep (normal operation)
    MODEM,       ///< Modem sleep (WiFi/BT off, CPU running)
    LIGHT,       ///< Light sleep (fast wake, RAM preserved)
    DEEP,        ///< Deep sleep (slow wake, partial RAM)
    SHUTDOWN,    ///< Full shutdown (requires reset/button)
    MODE_COUNT   ///< Number of sleep modes (for bounds)
};

/**
 * @brief Wake source configuration.
 */
struct WakeSourceConfig {
    uint8_t gpio_pin;          ///< GPIO pin number (0xFF if not GPIO)
    bool active_low;           ///< True if wake on low level
    bool pullup_enable;        ///< Enable internal pullup
    bool pulldown_enable;      ///< Enable internal pulldown
    WakeCause cause;           ///< Associated wake cause
    bool enabled;              ///< True if this source is enabled

    WakeSourceConfig() : gpio_pin(0xFF), active_low(true), pullup_enable(false), pulldown_enable(false),
                         cause(WakeCause::UNKNOWN), enabled(false) {}
};

/**
 * @brief Sleep configuration structure.
 */
struct SleepConfig {
    SleepMode mode;                              ///< Requested sleep mode
    uint32_t duration_ms;                        ///< Sleep duration (SLEEP_FOREVER for indefinite)
    uint8_t wake_source_count;                   ///< Number of configured wake sources
    WakeSourceConfig wake_sources[MAX_WAKE_SOURCES]; ///< Wake source configurations
    bool skip_preflight;                         ///< Skip preflight checks
    bool skip_save_state;                        ///< Skip saving state to flash

    SleepConfig()
        : mode(SleepMode::DEEP), duration_ms(SLEEP_FOREVER), wake_source_count(0),
          skip_preflight(false), skip_save_state(false)
    {
        // Initialize wake sources array (NASA Rule 6: explicit initialization)
        for (uint8_t i = 0; i < MAX_WAKE_SOURCES; ++i) {
            wake_sources[i] = WakeSourceConfig();
        }
    }
};

/**
 * @brief Sleep status/result structure.
 */
struct SleepStatus {
    WakeCause wake_cause;     ///< What caused the wake
    uint32_t sleep_duration_actual_ms; ///< Actual sleep duration
    uint8_t wake_gpio;        ///< GPIO that triggered wake (if applicable)
    bool was_sleeping;        ///< True if device was in sleep state

    SleepStatus() : wake_cause(WakeCause::UNKNOWN), sleep_duration_actual_ms(0),
                    wake_gpio(0xFF), was_sleeping(false) {}
};

/**
 * @brief Abstract interface for sleep management.
 *
 * Implementations provide platform-specific sleep behavior
 * while maintaining a consistent interface.
 */
class ISleepManager {
  public:
    virtual ~ISleepManager() = default;

    /**
     * @brief Initialize sleep manager.
     *
     * Sets up wake sources, configures RTC, etc.
     *
     * @return SleepResult::SUCCESS if initialization successful
     */
    virtual SleepResult initialize() = 0;

    /**
     * @brief Check if a sleep mode is supported.
     *
     * @param mode Sleep mode to check
     * @return true if the mode is supported on this platform
     */
    virtual bool isModeSupported(SleepMode mode) const = 0;

    /**
     * @brief Get the deepest supported sleep mode.
     *
     * @return Deepest sleep mode available
     */
    virtual SleepMode getDeepestMode() const = 0;

    /**
     * @brief Enter sleep mode.
     *
     * Enters the specified sleep mode and returns when device wakes.
     * For deep sleep, this function may not return (reboot on wake).
     *
     * @param config Sleep configuration
     * @param[out] status Status after wake (if applicable)
     * @return SleepResult indicating success or failure type
     */
    virtual SleepResult sleep(const SleepConfig &config, SleepStatus &status) = 0;

    /**
     * @brief Convenience method for light sleep.
     *
     * @param duration_ms Sleep duration in milliseconds
     * @return Wake cause after returning from sleep
     */
    virtual WakeCause lightSleep(uint32_t duration_ms) = 0;

    /**
     * @brief Enter deep sleep mode.
     *
     * May not return - device reboots on wake.
     *
     * @param duration_ms Sleep duration (SLEEP_FOREVER for indefinite)
     * @param skip_preflight Skip preflight checks
     * @param skip_save Skip saving state to flash
     * @return SleepResult (only if sleep fails)
     */
    virtual SleepResult deepSleep(uint32_t duration_ms, bool skip_preflight = false,
                                  bool skip_save = false) = 0;

    /**
     * @brief Perform preflight checks before sleep.
     *
     * Notifies observers and checks if sleep can proceed.
     *
     * @return true if sleep is allowed
     */
    virtual bool doPreflightCheck() = 0;

    /**
     * @brief Configure CPU frequency.
     *
     * @param fast true for high speed, false for power saving
     */
    virtual void setCpuSpeed(bool fast) = 0;

    /**
     * @brief Enable modem sleep (WiFi/BT power management).
     *
     * @return SleepResult::SUCCESS if enabled
     */
    virtual SleepResult enableModemSleep() = 0;

    /**
     * @brief Get the last wake cause.
     *
     * @return WakeCause from most recent wake
     */
    virtual WakeCause getLastWakeCause() const = 0;

    /**
     * @brief Get boot count (incremented each deep sleep wake).
     *
     * @return Boot count value
     */
    virtual int32_t getBootCount() const = 0;

    /**
     * @brief Get platform name for logging.
     *
     * @return Constant string with platform name
     */
    virtual const char *getPlatformName() const = 0;

  protected:
    ISleepManager() = default;
    ISleepManager(const ISleepManager &) = delete;
    ISleepManager &operator=(const ISleepManager &) = delete;
};

} // namespace power
} // namespace meshtastic
