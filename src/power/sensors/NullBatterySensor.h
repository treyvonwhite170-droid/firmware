/**
 * @file NullBatterySensor.h
 * @brief Null battery sensor for platforms without battery sensing.
 *
 * Provides safe default behavior when no battery sensor is available.
 * Follows Null Object pattern to eliminate null pointer checks.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple, deterministic behavior
 * - Rule 3: No dynamic allocation
 * - Rule 4: All functions trivially short
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "../IBatterySensor.h"

namespace meshtastic {
namespace power {

/**
 * @brief Null Object pattern implementation for battery sensor.
 *
 * Returns safe default values when no battery sensor is available.
 * Eliminates need for null checks throughout the codebase.
 */
class NullBatterySensor : public IBatterySensor {
  public:
    NullBatterySensor() = default;
    virtual ~NullBatterySensor() = default;

    BatterySensorResult initialize() override
    {
        // Always succeeds - nothing to initialize
        return BatterySensorResult::SUCCESS;
    }

    bool isInitialized() const override
    {
        return true; // Always "initialized"
    }

    BatterySensorResult read(BatteryReading &reading) override
    {
        // Return unknown/default values
        reading = BatteryReading();
        reading.valid = true; // Valid, just unknown
        return BatterySensorResult::SUCCESS;
    }

    int16_t getVoltage_mV() override
    {
        return -1; // Unknown
    }

    int8_t getChargePercent() override
    {
        return -1; // Unknown
    }

    bool isBatteryPresent() override
    {
        return false; // Assume no battery
    }

    bool isExternalPower() override
    {
        return true; // Assume powered (device is running)
    }

    ChargingState getChargingState() override
    {
        return ChargingState::UNKNOWN;
    }

    const char *getSensorTypeName() const override
    {
        return "Null";
    }

    uint8_t getPriority() const override
    {
        return 0; // Lowest priority - only used as fallback
    }
};

} // namespace power
} // namespace meshtastic
