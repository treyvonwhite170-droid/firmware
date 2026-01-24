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

#include "PowerManager.h"
#include "sensors/AnalogBatterySensor.h"
#include "sensors/PMUBatterySensor.h"
#include <cassert>

// Platform-specific sleep manager includes
#if defined(ARCH_ESP32)
#include "sleep/ESP32SleepManager.h"
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

    // Initialize battery chemistry
    chemistry_ = BatteryChemistry(config.battery_type, config.num_cells);

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
 * Tries sensors in priority order: PMU > Fuel Gauge > Analog
 *
 * NASA Rule 4: Under 60 lines
 */
bool PowerManager::initializeBatterySensor()
{
#if defined(HAS_PMU)
    // Try PMU sensor first (highest priority)
    extern XPowersLibInterface *PMU;
    if (PMU != nullptr) {
        static PMUSensorConfig pmu_config;
        static PMUBatterySensor pmu_sensor(PMU, pmu_config, chemistry_);
        s_pmu_sensor = &pmu_sensor;

        if (s_pmu_sensor->initialize() == BatterySensorResult::SUCCESS) {
            battery_sensor_ = s_pmu_sensor;
            return true;
        }
    }
#endif

#if defined(BATTERY_PIN)
    // Try analog sensor
    static AnalogSensorConfig analog_config;
    analog_config.battery_pin = BATTERY_PIN;
#if defined(ADC_MULTIPLIER)
    analog_config.adc_multiplier = ADC_MULTIPLIER;
#endif
#if defined(EXT_PWR_DETECT)
    analog_config.ext_pwr_detect_pin = EXT_PWR_DETECT;
#endif
#if defined(EXT_CHRG_DETECT)
    analog_config.ext_chrg_detect_pin = EXT_CHRG_DETECT;
#endif
#if defined(ADC_CTRL)
    analog_config.adc_ctrl_pin = ADC_CTRL;
#endif
#if defined(BATTERY_IMMUTABLE)
    analog_config.battery_immutable = true;
#endif

    static AnalogBatterySensor analog_sensor(analog_config, chemistry_);
    s_analog_sensor = &analog_sensor;

    if (s_analog_sensor->initialize() == BatterySensorResult::SUCCESS) {
        battery_sensor_ = s_analog_sensor;
        return true;
    }
#endif

    // No sensor available - use null
    return false;
}

bool PowerManager::initializeSleepManager()
{
#if defined(ARCH_ESP32)
    static ESP32WakeConfig wake_config;

#if defined(BUTTON_PIN)
    wake_config.button_pin = BUTTON_PIN;
#if defined(BUTTON_NEED_PULLUP)
    wake_config.button_need_pullup = true;
#endif
#endif

#if defined(LORA_DIO1)
    wake_config.lora_dio1_pin = LORA_DIO1;
#endif

#if defined(PMU_IRQ)
    wake_config.pmu_irq_pin = PMU_IRQ;
#endif

    static ESP32SleepManager esp32_sleep(wake_config);
    s_esp32_sleep_manager = &esp32_sleep;

    if (s_esp32_sleep_manager->initialize() == SleepResult::SUCCESS) {
        sleep_manager_ = s_esp32_sleep_manager;
        return true;
    }
#endif

    // No sleep manager for this platform
    return false;
}

bool PowerManager::initializePeripheralPower()
{
    // Peripheral power would be configured here
    // For now, use null implementation
    peripheral_power_ = &null_peripheral_;
    return true;
}

} // namespace power
} // namespace meshtastic
