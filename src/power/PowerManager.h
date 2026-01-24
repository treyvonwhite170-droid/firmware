/**
 * @file PowerManager.h
 * @brief Unified power management interface and factory.
 *
 * PowerManager serves as the single entry point for all power management
 * operations, abstracting platform-specific implementations behind
 * clean interfaces.
 *
 * Design Goals:
 * - Eliminate #ifdef clusters from calling code
 * - Provide consistent API across all platforms
 * - Follow NASA safety-critical coding standards
 * - Enable easy testing through dependency injection
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple, clear control flow
 * - Rule 3: No dynamic allocation after init
 * - Rule 4: Functions under 60 lines
 * - Rule 6: Minimal scope for all data
 * - Rule 8: Preprocessor limited to includes
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "IBatterySensor.h"
#include "IBatteryChemistry.h"
#include "ISleepManager.h"
#include "IPeripheralPower.h"
#include "sensors/NullBatterySensor.h"

namespace meshtastic {
namespace power {

/**
 * @brief Power manager initialization result.
 */
enum class PowerManagerResult : uint8_t {
    SUCCESS = 0,
    ERROR_NO_SENSOR,      ///< No battery sensor found
    ERROR_SENSOR_INIT,    ///< Sensor initialization failed
    ERROR_SLEEP_INIT,     ///< Sleep manager init failed
    ERROR_PERIPHERAL_INIT ///< Peripheral power init failed
};

/**
 * @brief Power manager configuration.
 *
 * Allows customization of power management behavior
 * without modifying code.
 */
struct PowerManagerConfig {
    BatteryType battery_type;         ///< Battery chemistry type
    uint8_t num_cells;                ///< Number of cells in series
    uint8_t low_voltage_count;        ///< Readings before low battery action
    uint16_t status_interval_ms;      ///< Status update interval
    bool auto_detect_sensors;         ///< Auto-detect battery sensors
    bool enable_power_saving;         ///< Enable power saving modes

    PowerManagerConfig()
        : battery_type(BatteryType::LION), num_cells(1), low_voltage_count(10),
          status_interval_ms(20000), auto_detect_sensors(true), enable_power_saving(true)
    {
    }
};

/**
 * @brief Unified power management interface.
 *
 * Provides a single point of access for:
 * - Battery status monitoring
 * - Sleep mode management
 * - Peripheral power control
 * - Power state notifications
 *
 * Usage:
 * @code
 *   PowerManager& pm = PowerManager::getInstance();
 *   pm.initialize(config);
 *
 *   BatteryReading reading;
 *   pm.getBatteryReading(reading);
 *
 *   if (reading.charge_percent < 5) {
 *       pm.enterDeepSleep(SLEEP_FOREVER);
 *   }
 * @endcode
 */
class PowerManager {
  public:
    /**
     * @brief Get singleton instance.
     *
     * @return Reference to PowerManager instance
     */
    static PowerManager &getInstance();

    /**
     * @brief Initialize power management subsystem.
     *
     * Detects and initializes battery sensors, sleep manager,
     * and peripheral power control for the current platform.
     *
     * @param config Power manager configuration
     * @return PowerManagerResult indicating success/failure
     */
    PowerManagerResult initialize(const PowerManagerConfig &config = PowerManagerConfig());

    /**
     * @brief Check if power manager is initialized.
     *
     * @return true if initialized successfully
     */
    bool isInitialized() const { return initialized_; }

    // =========================================================================
    // Battery Sensor Interface
    // =========================================================================

    /**
     * @brief Get current battery reading.
     *
     * @param[out] reading Battery state structure
     * @return BatterySensorResult indicating success/failure
     */
    BatterySensorResult getBatteryReading(BatteryReading &reading);

    /**
     * @brief Get battery voltage in millivolts.
     *
     * @return Voltage in mV, or -1 if unknown
     */
    int16_t getBatteryVoltage_mV();

    /**
     * @brief Get battery charge percentage.
     *
     * @return Charge 0-100%, or -1 if unknown
     */
    int8_t getBatteryPercent();

    /**
     * @brief Check if battery is present.
     *
     * @return true if battery detected
     */
    bool hasBattery();

    /**
     * @brief Check if external power connected.
     *
     * @return true if USB/external power present
     */
    bool hasExternalPower();

    /**
     * @brief Check if battery is charging.
     *
     * @return true if actively charging
     */
    bool isCharging();

    /**
     * @brief Get battery sensor type name.
     *
     * @return Sensor type string
     */
    const char *getSensorTypeName() const;

    // =========================================================================
    // Sleep Manager Interface
    // =========================================================================

    /**
     * @brief Enter light sleep mode.
     *
     * Fast wake, RAM preserved. Returns after wake.
     *
     * @param duration_ms Sleep duration in milliseconds
     * @return Wake cause
     */
    WakeCause lightSleep(uint32_t duration_ms);

    /**
     * @brief Enter deep sleep mode.
     *
     * May not return - device reboots on wake.
     *
     * @param duration_ms Sleep duration (SLEEP_FOREVER for indefinite)
     * @param skip_preflight Skip preflight checks
     * @return SleepResult (only on failure)
     */
    SleepResult deepSleep(uint32_t duration_ms, bool skip_preflight = false);

    /**
     * @brief Check if sleep mode is supported.
     *
     * @param mode Sleep mode to check
     * @return true if supported on this platform
     */
    bool isSleepModeSupported(SleepMode mode) const;

    /**
     * @brief Set CPU speed.
     *
     * @param fast true for high speed, false for power saving
     */
    void setCpuSpeed(bool fast);

    /**
     * @brief Enable modem sleep.
     *
     * @return SleepResult
     */
    SleepResult enableModemSleep();

    /**
     * @brief Get last wake cause.
     *
     * @return WakeCause from most recent wake
     */
    WakeCause getLastWakeCause() const;

    /**
     * @brief Get boot count.
     *
     * @return Number of boots since power-on
     */
    int32_t getBootCount() const;

    // =========================================================================
    // Peripheral Power Interface
    // =========================================================================

    /**
     * @brief Prepare peripherals for deep sleep.
     */
    void preparePeripheralsForSleep();

    /**
     * @brief Restore peripherals after wake.
     */
    void restorePeripheralsAfterWake();

    // =========================================================================
    // Low-Level Access (for compatibility)
    // =========================================================================

    /**
     * @brief Get battery sensor interface.
     *
     * @return Pointer to IBatterySensor (never null)
     */
    IBatterySensor *getBatterySensor() { return battery_sensor_; }

    /**
     * @brief Get sleep manager interface.
     *
     * @return Pointer to ISleepManager (may be null)
     */
    ISleepManager *getSleepManager() { return sleep_manager_; }

    /**
     * @brief Get peripheral power interface.
     *
     * @return Pointer to IPeripheralPower (never null)
     */
    IPeripheralPower *getPeripheralPower() { return peripheral_power_; }

    /**
     * @brief Get battery chemistry.
     *
     * @return Pointer to IBatteryChemistry
     */
    IBatteryChemistry *getBatteryChemistry() { return &chemistry_; }

  private:
    // Private constructor for singleton
    PowerManager();
    ~PowerManager();

    // Prevent copying
    PowerManager(const PowerManager &) = delete;
    PowerManager &operator=(const PowerManager &) = delete;

    /**
     * @brief Detect and initialize battery sensor.
     *
     * Tries sensors in priority order.
     *
     * @return true if sensor initialized
     */
    bool initializeBatterySensor();

    /**
     * @brief Initialize sleep manager for platform.
     *
     * @return true if sleep manager initialized
     */
    bool initializeSleepManager();

    /**
     * @brief Initialize peripheral power control.
     *
     * @return true if peripheral control initialized
     */
    bool initializePeripheralPower();

    // Member variables
    PowerManagerConfig config_;           ///< Current configuration
    BatteryChemistry chemistry_;          ///< Battery chemistry
    IBatterySensor *battery_sensor_;      ///< Active battery sensor
    ISleepManager *sleep_manager_;        ///< Platform sleep manager
    IPeripheralPower *peripheral_power_;  ///< Peripheral power control
    NullBatterySensor null_sensor_;       ///< Fallback null sensor
    NullPeripheralPower null_peripheral_; ///< Fallback null peripheral
    bool initialized_;                     ///< Initialization state
};

} // namespace power
} // namespace meshtastic
