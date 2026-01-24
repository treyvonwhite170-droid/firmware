/**
 * @file IBatterySensor.h
 * @brief Abstract interface for battery sensing hardware.
 *
 * This interface defines the contract for all battery sensor implementations,
 * abstracting away hardware-specific details and eliminating #ifdef clusters.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple control flow (virtual interface pattern)
 * - Rule 4: Functions kept under 60 lines
 * - Rule 6: Minimal scope for data objects
 * - Rule 7: Return value checking enforced by interface design
 * - Rule 8: Minimal preprocessor usage (include guards only)
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include <cstdint>

namespace meshtastic {
namespace power {

/**
 * @brief Result codes for battery sensor operations.
 *
 * Using explicit result codes instead of exceptions for
 * deterministic error handling per NASA Rule 1.
 */
enum class BatterySensorResult : uint8_t {
    SUCCESS = 0,           ///< Operation completed successfully
    ERROR_NOT_INITIALIZED, ///< Sensor not yet initialized
    ERROR_COMMUNICATION,   ///< Communication failure (I2C, SPI, etc.)
    ERROR_INVALID_READING, ///< Reading outside valid range
    ERROR_NO_BATTERY,      ///< No battery detected
    ERROR_TIMEOUT,         ///< Operation timed out
    ERROR_NOT_SUPPORTED    ///< Operation not supported on this platform
};

/**
 * @brief Battery charging state enumeration.
 *
 * Provides explicit charging states rather than boolean
 * to support complex charging scenarios.
 */
enum class ChargingState : uint8_t {
    UNKNOWN = 0,     ///< Charging state cannot be determined
    NOT_CHARGING,    ///< Battery not charging (discharging or full)
    CHARGING,        ///< Battery actively charging
    CHARGE_COMPLETE, ///< Charging complete, battery full
    FAULT            ///< Charging fault detected
};

/**
 * @brief Power source enumeration.
 */
enum class PowerSource : uint8_t {
    UNKNOWN = 0, ///< Power source unknown
    BATTERY,     ///< Running on battery power
    USB,         ///< Running on USB power
    EXTERNAL     ///< Running on external DC power
};

/**
 * @brief Battery sensor reading structure.
 *
 * Encapsulates all battery state in a single structure
 * to ensure atomic reads and minimize interface complexity.
 */
struct BatteryReading {
    int16_t voltage_mv;     ///< Battery voltage in millivolts (-1 if unknown)
    int8_t charge_percent;  ///< State of charge 0-100% (-1 if unknown)
    ChargingState charging; ///< Current charging state
    PowerSource source;     ///< Current power source
    bool battery_present;   ///< True if battery is connected
    bool valid;             ///< True if reading is valid

    /**
     * @brief Default constructor initializes to safe unknown state.
     */
    BatteryReading()
        : voltage_mv(-1), charge_percent(-1), charging(ChargingState::UNKNOWN), source(PowerSource::UNKNOWN),
          battery_present(false), valid(false)
    {
    }
};

/**
 * @brief Abstract interface for battery sensing hardware.
 *
 * All battery sensor implementations must derive from this interface.
 * This design eliminates #ifdef clusters by using polymorphism.
 *
 * Implementation Requirements (NASA Compliance):
 * - All implementations must validate parameters (Rule 7)
 * - No dynamic allocation in sensor methods (Rule 3)
 * - All loops must have fixed bounds (Rule 2)
 * - Implementation functions must be under 60 lines (Rule 4)
 */
class IBatterySensor {
  public:
    /**
     * @brief Virtual destructor for proper cleanup.
     */
    virtual ~IBatterySensor() = default;

    /**
     * @brief Initialize the battery sensor hardware.
     *
     * Must be called before any other sensor operations.
     * Should configure GPIO, I2C, ADC, or other hardware as needed.
     *
     * @return BatterySensorResult::SUCCESS if initialization successful
     */
    virtual BatterySensorResult initialize() = 0;

    /**
     * @brief Check if the sensor is initialized and ready.
     *
     * @return true if sensor is initialized and operational
     */
    virtual bool isInitialized() const = 0;

    /**
     * @brief Read current battery state.
     *
     * Performs a complete battery state read including voltage,
     * charge percentage, and charging status.
     *
     * @param[out] reading Structure to receive battery state
     * @return BatterySensorResult indicating success or failure type
     */
    virtual BatterySensorResult read(BatteryReading &reading) = 0;

    /**
     * @brief Get battery voltage in millivolts.
     *
     * Convenience method for voltage-only reads.
     *
     * @return Battery voltage in mV, or -1 if unknown/error
     */
    virtual int16_t getVoltage_mV() = 0;

    /**
     * @brief Get battery charge percentage.
     *
     * @return Charge percentage 0-100, or -1 if unknown/error
     */
    virtual int8_t getChargePercent() = 0;

    /**
     * @brief Check if battery is connected.
     *
     * @return true if battery is detected
     */
    virtual bool isBatteryPresent() = 0;

    /**
     * @brief Check if external power is connected.
     *
     * @return true if USB or external DC power detected
     */
    virtual bool isExternalPower() = 0;

    /**
     * @brief Get current charging state.
     *
     * @return ChargingState enumeration value
     */
    virtual ChargingState getChargingState() = 0;

    /**
     * @brief Get sensor type name for debugging/logging.
     *
     * @return Constant string identifying sensor type
     */
    virtual const char *getSensorTypeName() const = 0;

    /**
     * @brief Get the sensor priority for auto-detection.
     *
     * Higher priority sensors are tried first during initialization.
     * PMU sensors typically have highest priority (100),
     * fuel gauges medium (50), analog sensors lowest (10).
     *
     * @return Priority value (higher = try first)
     */
    virtual uint8_t getPriority() const = 0;

  protected:
    /**
     * @brief Protected default constructor.
     *
     * Only derived classes can be instantiated.
     */
    IBatterySensor() = default;

    // Prevent copying (Rule 9: restrict pointer/reference complexity)
    IBatterySensor(const IBatterySensor &) = delete;
    IBatterySensor &operator=(const IBatterySensor &) = delete;
};

} // namespace power
} // namespace meshtastic
