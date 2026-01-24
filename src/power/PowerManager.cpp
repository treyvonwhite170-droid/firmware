/**
 * @file PowerManager.cpp
 * @brief Unified power manager implementation.
 *
 * Provides platform-agnostic power management by delegating
 * to platform-specific implementations through interfaces.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 3: Sensors allocated statically or at init only
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions for null checks
 * - Rule 7: All results checked and returned
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */

// Include configuration first to get variant defines
#include "configuration.h"

#include "PowerManager.h"
#include "VariantPowerConfig.h"
#include "sensors/AnalogBatterySensor.h"
#include "sensors/PMUBatterySensor.h"
#include <cassert>

// Platform-specific sleep manager includes
#if defined(ARCH_ESP32)
#include "sleep/ESP32SleepManager.h"
#endif

// PMU header if available
#if defined(HAS_PMU)
#include "XPowersLibInterface.hpp"
extern XPowersLibInterface *PMU;
#endif

namespace meshtastic {
namespace power {

// ============================================================================
// Static storage for sensors (avoids dynamic allocation per NASA Rule 3)
// ============================================================================

// Static sensor instances - only one active at a time
static AnalogBatterySensor *s_analog_sensor = nullptr;
static PMUBatterySensor *s_pmu_sensor = nullptr;

// Static sleep manager instance
#if defined(ARCH_ESP32)
static ESP32SleepManager *s_esp32_sleep_manager = nullptr;
#endif

// ============================================================================
// PowerManager Implementation
// ============================================================================

PowerManager::PowerManager()
    : config_(), chemistry_(BatteryType::LION, 1), battery_sensor_(&null_sensor_), sleep_manager_(nullptr),
      peripheral_power_(&null_peripheral_), null_sensor_(), null_peripheral_(), initialized_(false)
{
}

PowerManager::~PowerManager()
{
    // Static storage - no cleanup needed
}

PowerManager &PowerManager::getInstance()
{
    static PowerManager instance;
    return instance;
}

/**
 * @brief Initialize power management subsystem.
 *
 * NASA Rule 4: Under 60 lines
 */
PowerManagerResult PowerManager::initialize(const PowerManagerConfig &config)
{
    if (initialized_) {
        return PowerManagerResult::SUCCESS;
    }

    config_ = config;

    // Initialize battery chemistry from variant or config
    if (config.auto_detect_sensors) {
        // Use variant-specific chemistry (respects OCV_ARRAY, CELL_TYPE_*, etc.)
        chemistry_ = createVariantChemistry();
    } else {
        chemistry_ = BatteryChemistry(config.battery_type, config.num_cells);
    }

    // Initialize battery sensor
    if (!initializeBatterySensor()) {
        // Not fatal - use null sensor
        battery_sensor_ = &null_sensor_;
    }

    // Initialize sleep manager
    if (!initializeSleepManager()) {
        // Not fatal - sleep functions will return errors
        sleep_manager_ = nullptr;
    }

    // Initialize peripheral power
    if (!initializePeripheralPower()) {
        // Not fatal - use null implementation
        peripheral_power_ = &null_peripheral_;
    }

    initialized_ = true;
    return PowerManagerResult::SUCCESS;
}

// ============================================================================
// Battery Sensor Interface
// ============================================================================

BatterySensorResult PowerManager::getBatteryReading(BatteryReading &reading)
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->read(reading);
}

int16_t PowerManager::getBatteryVoltage_mV()
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->getVoltage_mV();
}

int8_t PowerManager::getBatteryPercent()
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->getChargePercent();
}

bool PowerManager::hasBattery()
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->isBatteryPresent();
}

bool PowerManager::hasExternalPower()
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->isExternalPower();
}

bool PowerManager::isCharging()
{
    assert(battery_sensor_ != nullptr);
    ChargingState state = battery_sensor_->getChargingState();
    return (state == ChargingState::CHARGING);
}

const char *PowerManager::getSensorTypeName() const
{
    assert(battery_sensor_ != nullptr);
    return battery_sensor_->getSensorTypeName();
}

// ============================================================================
// Sleep Manager Interface
// ============================================================================

WakeCause PowerManager::lightSleep(uint32_t duration_ms)
{
    if (sleep_manager_ == nullptr) {
        return WakeCause::UNKNOWN;
    }
    return sleep_manager_->lightSleep(duration_ms);
}

SleepResult PowerManager::deepSleep(uint32_t duration_ms, bool skip_preflight)
{
    if (sleep_manager_ == nullptr) {
        return SleepResult::ERROR_NOT_SUPPORTED;
    }

    // Prepare peripherals for sleep
    preparePeripheralsForSleep();

    return sleep_manager_->deepSleep(duration_ms, skip_preflight, false);
}

bool PowerManager::isSleepModeSupported(SleepMode mode) const
{
    if (sleep_manager_ == nullptr) {
        return false;
    }
    return sleep_manager_->isModeSupported(mode);
}

void PowerManager::setCpuSpeed(bool fast)
{
    if (sleep_manager_ != nullptr) {
        sleep_manager_->setCpuSpeed(fast);
    }
}

SleepResult PowerManager::enableModemSleep()
{
    if (sleep_manager_ == nullptr) {
        return SleepResult::ERROR_NOT_SUPPORTED;
    }
    return sleep_manager_->enableModemSleep();
}

WakeCause PowerManager::getLastWakeCause() const
{
    if (sleep_manager_ == nullptr) {
        return WakeCause::UNKNOWN;
    }
    return sleep_manager_->getLastWakeCause();
}

int32_t PowerManager::getBootCount() const
{
    if (sleep_manager_ == nullptr) {
        return 0;
    }
    return sleep_manager_->getBootCount();
}

// ============================================================================
// Peripheral Power Interface
// ============================================================================

void PowerManager::preparePeripheralsForSleep()
{
    assert(peripheral_power_ != nullptr);
    peripheral_power_->prepareForDeepSleep();
}

void PowerManager::restorePeripheralsAfterWake()
{
    assert(peripheral_power_ != nullptr);
    peripheral_power_->restoreAfterWake();
}

// ============================================================================
// Private Initialization Methods
// ============================================================================

/**
 * @brief Initialize battery sensor with auto-detection.
 *
 * Tries sensors in priority order: PMU > Fuel Gauge > Analog.
 * Uses variant-specific configuration from VariantPowerConfig.h.
 *
 * NASA Rule 4: Under 60 lines
 */
bool PowerManager::initializeBatterySensor()
{
#if defined(HAS_PMU)
    // Try PMU sensor first (highest priority)
    if (PMU != nullptr) {
        static PMUSensorConfig pmu_config;
        // Configure PMU from variant
#if defined(PMU_IRQ)
        pmu_config.irq_pin = PMU_IRQ;
#endif
#if defined(PMU_USE_WIRE1)
        pmu_config.use_wire1 = true;
#endif
        static PMUBatterySensor pmu_sensor(PMU, pmu_config, chemistry_);
        s_pmu_sensor = &pmu_sensor;

        if (s_pmu_sensor->initialize() == BatterySensorResult::SUCCESS) {
            battery_sensor_ = s_pmu_sensor;
            return true;
        }
    }
#endif

    // Try analog sensor if variant supports it
    if (hasVariantAnalogBattery()) {
        // Use variant configuration helper for full define coverage
        static AnalogSensorConfig analog_config = createVariantAnalogConfig();
        static AnalogBatterySensor analog_sensor(analog_config, chemistry_);
        s_analog_sensor = &analog_sensor;

        if (s_analog_sensor->initialize() == BatterySensorResult::SUCCESS) {
            battery_sensor_ = s_analog_sensor;
            return true;
        }
    }

    // No sensor available - use null
    return false;
}

/**
 * @brief Initialize sleep manager for current platform.
 *
 * Uses variant-specific wake configuration.
 *
 * NASA Rule 4: Under 60 lines
 */
bool PowerManager::initializeSleepManager()
{
#if defined(ARCH_ESP32)
    // Use variant configuration helper for ESP32
    static ESP32WakeConfig wake_config = createVariantESP32WakeConfig();
    static ESP32SleepManager esp32_sleep(wake_config);
    s_esp32_sleep_manager = &esp32_sleep;

    if (s_esp32_sleep_manager->initialize() == SleepResult::SUCCESS) {
        sleep_manager_ = s_esp32_sleep_manager;
        return true;
    }
#endif

    // TODO: Add NRF52, RP2040, STM32WL sleep managers

    // No sleep manager for this platform
    return false;
}

bool PowerManager::initializePeripheralPower()
{
    // TODO: Implement variant-specific peripheral power control
    // using hasVariantPeripheralPower(), getVariantPowerEnablePin(), etc.
    peripheral_power_ = &null_peripheral_;
    return true;
}

} // namespace power
} // namespace meshtastic
