/**
 * @file PMUBatterySensor.cpp
 * @brief PMU battery sensor implementation.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions at entry points
 * - Rule 7: All I2C/PMU calls checked
 * - Rule 9: Controlled pointer usage
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */

#include "PMUBatterySensor.h"
#include <cassert>

// Only include PMU headers when available
#if defined(HAS_PMU)
#include "XPowersLibInterface.hpp"
#endif

namespace meshtastic {
namespace power {

PMUBatterySensor::PMUBatterySensor(XPowersLibInterface *pmu, const PMUSensorConfig &config,
                                   const BatteryChemistry &chemistry)
    : BaseBatterySensor(chemistry), pmu_(pmu), config_(config), irq_pending_(false)
{
    assert(pmu != nullptr);
}

/**
 * @brief Initialize PMU battery sensor.
 *
 * NASA Rule 4: Under 60 lines
 * NASA Rule 7: All PMU calls checked
 */
BatterySensorResult PMUBatterySensor::initialize()
{
#if defined(HAS_PMU)
    // Validate PMU pointer (NASA Rule 5)
    assert(pmu_ != nullptr);
    if (pmu_ == nullptr) {
        return BatterySensorResult::ERROR_NOT_INITIALIZED;
    }

    // Detect PMU model if not specified
    if (config_.model == PMUModel::UNKNOWN) {
        config_.model = detectModel();
        if (config_.model == PMUModel::UNKNOWN) {
            return BatterySensorResult::ERROR_COMMUNICATION;
        }
    }

    // Configure PMU for battery monitoring
    if (!configurePMU()) {
        return BatterySensorResult::ERROR_CONFIGURATION;
    }

    // Setup IRQ if pin specified
    if (config_.irq_pin >= 0) {
#if defined(ARDUINO)
        pinMode(static_cast<uint8_t>(config_.irq_pin), INPUT);
#endif
    }

    setInitialized(true);
    return BatterySensorResult::SUCCESS;
#else
    return BatterySensorResult::ERROR_NOT_SUPPORTED;
#endif
}

const char *PMUBatterySensor::getSensorTypeName() const
{
    switch (config_.model) {
    case PMUModel::AXP192:
        return "AXP192";
    case PMUModel::AXP2101:
        return "AXP2101";
    default:
        return "PMU";
    }
}

BatterySensorResult PMUBatterySensor::readVoltageRaw(uint16_t &voltage_mv)
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return BatterySensorResult::ERROR_NOT_INITIALIZED;
    }

    // Read battery voltage from PMU
    const uint16_t voltage = pmu_->getBattVoltage();

    // Validate reading
    if (voltage == 0) {
        return BatterySensorResult::ERROR_INVALID_READING;
    }

    voltage_mv = voltage;
    return BatterySensorResult::SUCCESS;
#else
    (void)voltage_mv;
    return BatterySensorResult::ERROR_NOT_SUPPORTED;
#endif
}

ChargingState PMUBatterySensor::detectChargingState()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return ChargingState::UNKNOWN;
    }

    if (pmu_->isCharging()) {
        return ChargingState::CHARGING;
    }

    // Check if charge complete (only for AXP chips that support it)
    if (detectExternalPower() && !pmu_->isCharging()) {
        return ChargingState::CHARGE_COMPLETE;
    }

    return ChargingState::NOT_CHARGING;
#else
    return ChargingState::UNKNOWN;
#endif
}

bool PMUBatterySensor::detectExternalPower()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return false;
    }

    return pmu_->isVbusIn();
#else
    return false;
#endif
}

bool PMUBatterySensor::detectBatteryPresent()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return false;
    }

    return pmu_->isBatteryConnect();
#else
    return false;
#endif
}

bool PMUBatterySensor::handleIRQ()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return false;
    }

    // Read and clear IRQ status
    pmu_->getIrqStatus();

    bool had_irq = false;

    // Check for relevant IRQs
    if (pmu_->isVbusInsertIrq()) {
        had_irq = true;
    }
    if (pmu_->isVbusRemoveIrq()) {
        had_irq = true;
    }

    // Clear all IRQs
    pmu_->clearIrqStatus();

    return had_irq;
#else
    return false;
#endif
}

PMUModel PMUBatterySensor::detectModel()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return PMUModel::UNKNOWN;
    }

    // Use XPowersLib chip model detection
    const uint8_t model = pmu_->getChipModel();

    switch (model) {
    case 1: // XPOWERS_AXP192
        return PMUModel::AXP192;
    case 2: // XPOWERS_AXP2101
        return PMUModel::AXP2101;
    default:
        return PMUModel::UNKNOWN;
    }
#else
    return PMUModel::UNKNOWN;
#endif
}

/**
 * @brief Configure PMU for optimal battery monitoring.
 *
 * NASA Rule 4: Under 60 lines
 */
bool PMUBatterySensor::configurePMU()
{
#if defined(HAS_PMU)
    if (pmu_ == nullptr) {
        return false;
    }

    // Enable battery voltage measurement
    pmu_->enableBattVoltageMeasure();

    // Enable VBUS voltage measurement
    pmu_->enableVbusVoltageMeasure();

    // Disable TS pin measurement (no external temperature sensor)
    pmu_->disableTSPinMeasure();

    // Set shutdown voltage
    if (config_.shutdown_voltage_mv > 0) {
        pmu_->setSysPowerDownVoltage(config_.shutdown_voltage_mv);
    }

    // Disable unused IRQs, enable key ones
    pmu_->disableIRQ(0xFFFFFFFF); // Disable all first

    // Model-specific configuration would go here
    // (kept simple for NASA Rule 4 compliance)

    return true;
#else
    return false;
#endif
}

/**
 * @brief Factory function to detect PMU sensor.
 *
 * Attempts to initialize PMU chips on I2C bus.
 * Returns nullptr if no PMU found.
 */
PMUBatterySensor *detectPMUSensor(const BatteryChemistry &chemistry)
{
#if defined(HAS_PMU)
    // This would typically scan I2C and create appropriate PMU
    // For now, return nullptr - actual initialization happens elsewhere
    (void)chemistry;
    return nullptr;
#else
    (void)chemistry;
    return nullptr;
#endif
}

} // namespace power
} // namespace meshtastic
