/**
 * @file BaseBatterySensor.cpp
 * @brief Base battery sensor implementation.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 2: All loops have fixed upper bounds
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions at function entry
 * - Rule 7: All return values checked
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */

#include "BaseBatterySensor.h"
#include <cassert>

// Platform-specific time function
#if defined(ARDUINO)
#include <Arduino.h>
#define GET_TIME_MS() millis()
#else
#include <chrono>
#define GET_TIME_MS()                                                                                                            \
    static_cast<uint32_t>(                                                                                                       \
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count())
#endif

namespace meshtastic {
namespace power {

BaseBatterySensor::BaseBatterySensor(const BatteryChemistry &chemistry)
    : chemistry_(chemistry), cached_reading_(), filtered_voltage_mv_(0), last_read_time_ms_(0), initialized_(false),
      filter_initialized_(false)
{
    // Initialize cached reading to safe defaults
    cached_reading_.voltage_mv = -1;
    cached_reading_.charge_percent = -1;
    cached_reading_.charging = ChargingState::UNKNOWN;
    cached_reading_.source = PowerSource::UNKNOWN;
    cached_reading_.battery_present = false;
    cached_reading_.valid = false;
}

/**
 * @brief Read current battery state with caching.
 *
 * NASA Rule 4: Under 60 lines
 * NASA Rule 7: All results checked
 */
BatterySensorResult BaseBatterySensor::read(BatteryReading &reading)
{
    // Check initialization (NASA Rule 5)
    assert(initialized_);
    if (!initialized_) {
        reading = BatteryReading(); // Return safe defaults
        return BatterySensorResult::ERROR_NOT_INITIALIZED;
    }

    // Check if we should read now or use cached value
    if (!shouldReadNow()) {
        reading = cached_reading_;
        return cached_reading_.valid ? BatterySensorResult::SUCCESS : BatterySensorResult::ERROR_INVALID_READING;
    }

    // Perform hardware read
    uint16_t raw_voltage_mv = 0;
    BatterySensorResult result = readVoltageRaw(raw_voltage_mv);

    if (result != BatterySensorResult::SUCCESS) {
        // Keep last valid reading on error
        reading = cached_reading_;
        return result;
    }

    // Apply low-pass filter
    const uint16_t filtered_mv = applyFilter(raw_voltage_mv);

    // Update cached reading
    cached_reading_.voltage_mv = static_cast<int16_t>(filtered_mv);
    cached_reading_.battery_present = detectBatteryPresent();
    cached_reading_.charging = detectChargingState();
    cached_reading_.source = detectExternalPower() ? PowerSource::USB : PowerSource::BATTERY;

    // Calculate SoC if battery present
    if (cached_reading_.battery_present) {
        cached_reading_.charge_percent = chemistry_.calculateSoC(filtered_mv);
    } else {
        cached_reading_.charge_percent = -1;
    }

    cached_reading_.valid = true;
    updateReadTime();

    // Copy to output
    reading = cached_reading_;
    return BatterySensorResult::SUCCESS;
}

int16_t BaseBatterySensor::getVoltage_mV()
{
    if (!initialized_) {
        return -1;
    }

    BatteryReading reading;
    const BatterySensorResult result = read(reading);

    if (result == BatterySensorResult::SUCCESS && reading.valid) {
        return reading.voltage_mv;
    }
    return -1;
}

int8_t BaseBatterySensor::getChargePercent()
{
    if (!initialized_) {
        return -1;
    }

    BatteryReading reading;
    const BatterySensorResult result = read(reading);

    if (result == BatterySensorResult::SUCCESS && reading.valid) {
        return reading.charge_percent;
    }
    return -1;
}

bool BaseBatterySensor::isBatteryPresent()
{
    if (!initialized_) {
        return false;
    }

    BatteryReading reading;
    const BatterySensorResult result = read(reading);

    return (result == BatterySensorResult::SUCCESS) && reading.battery_present;
}

bool BaseBatterySensor::isExternalPower()
{
    if (!initialized_) {
        return false;
    }

    BatteryReading reading;
    const BatterySensorResult result = read(reading);

    if (result == BatterySensorResult::SUCCESS && reading.valid) {
        return (reading.source == PowerSource::USB || reading.source == PowerSource::EXTERNAL);
    }
    return false;
}

ChargingState BaseBatterySensor::getChargingState()
{
    if (!initialized_) {
        return ChargingState::UNKNOWN;
    }

    BatteryReading reading;
    const BatterySensorResult result = read(reading);

    if (result == BatterySensorResult::SUCCESS && reading.valid) {
        return reading.charging;
    }
    return ChargingState::UNKNOWN;
}

/**
 * @brief Default charging state detection using voltage.
 */
ChargingState BaseBatterySensor::detectChargingState()
{
    // Default: use voltage-based detection
    if (filtered_voltage_mv_ == 0) {
        return ChargingState::UNKNOWN;
    }

    if (chemistry_.isChargingVoltage(filtered_voltage_mv_)) {
        return ChargingState::CHARGING;
    }

    return ChargingState::NOT_CHARGING;
}

/**
 * @brief Default external power detection.
 */
bool BaseBatterySensor::detectExternalPower()
{
    // Default: assume external power if voltage above charging threshold
    return chemistry_.isChargingVoltage(filtered_voltage_mv_);
}

/**
 * @brief Default battery presence detection.
 */
bool BaseBatterySensor::detectBatteryPresent()
{
    // Check against no-battery threshold
    return !chemistry_.isNoBatteryVoltage(filtered_voltage_mv_);
}

/**
 * @brief Apply exponential moving average filter.
 *
 * NASA Rule 4: Under 60 lines
 */
uint16_t BaseBatterySensor::applyFilter(uint16_t raw_mv)
{
    if (!filter_initialized_) {
        // Initialize filter with first reading
        filtered_voltage_mv_ = raw_mv;
        filter_initialized_ = true;
        return raw_mv;
    }

    // Exponential moving average
    // filtered = filtered + coefficient * (raw - filtered)
    const float delta = static_cast<float>(raw_mv) - static_cast<float>(filtered_voltage_mv_);
    filtered_voltage_mv_ = static_cast<uint16_t>(static_cast<float>(filtered_voltage_mv_) + (LPF_COEFFICIENT * delta));

    return filtered_voltage_mv_;
}

bool BaseBatterySensor::shouldReadNow() const
{
    const uint32_t now = getTimeMs();
    const uint32_t elapsed = now - last_read_time_ms_;

    // Handle wrap-around and first read
    if (last_read_time_ms_ == 0 || elapsed >= MIN_READ_INTERVAL_MS) {
        return true;
    }
    return false;
}

void BaseBatterySensor::updateReadTime()
{
    last_read_time_ms_ = getTimeMs();
}

uint32_t BaseBatterySensor::getTimeMs() const
{
    return GET_TIME_MS();
}

} // namespace power
} // namespace meshtastic
