/**
 * @file IPeripheralPower.h
 * @brief Abstract interface for peripheral power management.
 *
 * This interface abstracts control of peripheral power rails and
 * enable pins, eliminating scattered #ifdef directives for power
 * control across different board variants.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple control flow
 * - Rule 4: Functions kept under 60 lines
 * - Rule 6: Minimal scope, explicit state tracking
 * - Rule 7: Return value checking on all operations
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
 * @brief Maximum number of peripheral power channels.
 *
 * Fixed constant per NASA Rule 2 (fixed upper-bound loops).
 */
static constexpr uint8_t MAX_PERIPHERAL_CHANNELS = 16;

/**
 * @brief Peripheral power result codes.
 */
enum class PeripheralPowerResult : uint8_t {
    SUCCESS = 0,           ///< Operation successful
    ERROR_INVALID_CHANNEL, ///< Channel ID out of range
    ERROR_NOT_CONFIGURED,  ///< Channel not configured
    ERROR_PROTECTED,       ///< Channel is protected (cannot disable)
    ERROR_HARDWARE         ///< Hardware error
};

/**
 * @brief Peripheral type enumeration.
 *
 * Identifies the type of peripheral for power management policy.
 */
enum class PeripheralType : uint8_t {
    UNKNOWN = 0,
    SCREEN,      ///< OLED/TFT/E-ink display (avoid DISPLAY macro conflict)
    GPS,         ///< GPS/GNSS module
    LORA,        ///< LoRa radio
    LED,         ///< LED indicator(s)
    SDCARD,      ///< SD card
    SENSOR,      ///< General sensor
    BUZZER,      ///< Audio buzzer
    POWER_RAIL,  ///< General power rail (3.3V, 5V, etc.)
    WATCHDOG,    ///< Watchdog enable
    BLUETOOTH,   ///< Bluetooth module
    WIFI,        ///< WiFi module
    PERIPHERAL_TYPE_COUNT
};

/**
 * @brief Peripheral channel configuration.
 */
struct PeripheralChannel {
    uint8_t gpio_pin;           ///< GPIO pin number (0xFF if not GPIO-controlled)
    PeripheralType type;        ///< Peripheral type
    bool active_high;           ///< True if HIGH enables power
    bool protected_channel;     ///< True if cannot be disabled
    bool default_state;         ///< Default power state on init
    bool current_state;         ///< Current power state
    const char *name;           ///< Human-readable name

    PeripheralChannel()
        : gpio_pin(0xFF), type(PeripheralType::UNKNOWN), active_high(true),
          protected_channel(false), default_state(false), current_state(false),
          name("Unconfigured")
    {
    }
};

/**
 * @brief Abstract interface for peripheral power control.
 *
 * Manages power enable pins and rails for peripherals.
 */
class IPeripheralPower {
  public:
    virtual ~IPeripheralPower() = default;

    /**
     * @brief Initialize peripheral power management.
     *
     * Configures GPIO pins and sets default power states.
     *
     * @return PeripheralPowerResult::SUCCESS if successful
     */
    virtual PeripheralPowerResult initialize() = 0;

    /**
     * @brief Get number of configured channels.
     *
     * @return Number of peripheral channels
     */
    virtual uint8_t getChannelCount() const = 0;

    /**
     * @brief Get channel configuration by index.
     *
     * @param index Channel index (0 to getChannelCount()-1)
     * @param[out] channel Channel configuration
     * @return PeripheralPowerResult::SUCCESS if valid
     */
    virtual PeripheralPowerResult getChannel(uint8_t index,
                                             PeripheralChannel &channel) const = 0;

    /**
     * @brief Enable power to a peripheral channel.
     *
     * @param index Channel index
     * @return PeripheralPowerResult indicating success/failure
     */
    virtual PeripheralPowerResult enable(uint8_t index) = 0;

    /**
     * @brief Disable power to a peripheral channel.
     *
     * Protected channels cannot be disabled.
     *
     * @param index Channel index
     * @return PeripheralPowerResult indicating success/failure
     */
    virtual PeripheralPowerResult disable(uint8_t index) = 0;

    /**
     * @brief Set peripheral channel state.
     *
     * @param index Channel index
     * @param enabled true to enable, false to disable
     * @return PeripheralPowerResult indicating success/failure
     */
    virtual PeripheralPowerResult setState(uint8_t index, bool enabled) = 0;

    /**
     * @brief Check if a channel is currently enabled.
     *
     * @param index Channel index
     * @return true if channel is powered on
     */
    virtual bool isEnabled(uint8_t index) const = 0;

    /**
     * @brief Find channel by peripheral type.
     *
     * @param type Peripheral type to find
     * @return Channel index, or 0xFF if not found
     */
    virtual uint8_t findByType(PeripheralType type) const = 0;

    /**
     * @brief Enable all peripheral channels.
     *
     * Called during system startup.
     */
    virtual void enableAll() = 0;

    /**
     * @brief Disable all non-protected channels.
     *
     * Called before deep sleep.
     */
    virtual void disableAllNonProtected() = 0;

    /**
     * @brief Prepare peripherals for deep sleep.
     *
     * Puts peripherals into low-power state and
     * configures GPIO for minimal leakage.
     */
    virtual void prepareForDeepSleep() = 0;

    /**
     * @brief Restore peripherals after wake.
     *
     * Re-enables peripherals after deep sleep wake.
     */
    virtual void restoreAfterWake() = 0;

  protected:
    IPeripheralPower() = default;
    IPeripheralPower(const IPeripheralPower &) = delete;
    IPeripheralPower &operator=(const IPeripheralPower &) = delete;
};

/**
 * @brief Null implementation for platforms without peripheral power control.
 *
 * Provides safe default behavior when no peripheral power management
 * is needed.
 */
class NullPeripheralPower : public IPeripheralPower {
  public:
    PeripheralPowerResult initialize() override { return PeripheralPowerResult::SUCCESS; }
    uint8_t getChannelCount() const override { return 0; }
    PeripheralPowerResult getChannel(uint8_t, PeripheralChannel &) const override
    {
        return PeripheralPowerResult::ERROR_INVALID_CHANNEL;
    }
    PeripheralPowerResult enable(uint8_t) override
    {
        return PeripheralPowerResult::ERROR_INVALID_CHANNEL;
    }
    PeripheralPowerResult disable(uint8_t) override
    {
        return PeripheralPowerResult::ERROR_INVALID_CHANNEL;
    }
    PeripheralPowerResult setState(uint8_t, bool) override
    {
        return PeripheralPowerResult::ERROR_INVALID_CHANNEL;
    }
    bool isEnabled(uint8_t) const override { return false; }
    uint8_t findByType(PeripheralType) const override { return 0xFF; }
    void enableAll() override {}
    void disableAllNonProtected() override {}
    void prepareForDeepSleep() override {}
    void restoreAfterWake() override {}
};

} // namespace power
} // namespace meshtastic
