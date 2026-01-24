/**
 * @file AnalogBatterySensor.h
 * @brief Analog ADC-based battery voltage sensor.
 *
 * Supports battery voltage measurement via voltage divider connected
 * to an analog input pin. Works across ESP32, NRF52, and RP2040 platforms.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 2: Fixed sample count for averaging
 * - Rule 3: No dynamic allocation
 * - Rule 4: Functions under 60 lines
 * - Rule 6: Minimal variable scope
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "BaseBatterySensor.h"

namespace meshtastic {
namespace power {

/**
 * @brief Configuration for analog battery sensor.
 *
 * Encapsulates all pin and calibration settings to eliminate
 * #ifdef directives at the usage site.
 */
struct AnalogSensorConfig {
    int8_t battery_pin;           ///< ADC pin for battery voltage (-1 if not used)
    int8_t adc_ctrl_pin;          ///< ADC control/enable pin (-1 if not used)
    int8_t ext_pwr_detect_pin;    ///< External power detect pin (-1 if not used)
    int8_t ext_chrg_detect_pin;   ///< Charge detect pin (-1 if not used)
    float adc_multiplier;         ///< Voltage divider multiplier
    float reference_voltage;      ///< ADC reference voltage
    uint8_t resolution_bits;      ///< ADC resolution in bits
    bool adc_ctrl_use_pullup;     ///< Use pullup for ADC control
    bool adc_ctrl_active_high;    ///< ADC control active state
    bool ext_pwr_active_high;     ///< External power detect active state
    bool ext_chrg_active_high;    ///< Charge detect active state
    bool battery_immutable;       ///< Battery always considered present
    uint8_t adc_unit;             ///< ADC unit (0=ADC1, 1=ADC2 for ESP32)

    /**
     * @brief Default configuration constructor.
     */
    AnalogSensorConfig()
        : battery_pin(-1), adc_ctrl_pin(-1), ext_pwr_detect_pin(-1), ext_chrg_detect_pin(-1), adc_multiplier(2.0f),
          reference_voltage(3.3f), resolution_bits(12), adc_ctrl_use_pullup(false), adc_ctrl_active_high(true),
          ext_pwr_active_high(true), ext_chrg_active_high(true), battery_immutable(false), adc_unit(0)
    {
    }
};

/**
 * @brief Analog battery voltage sensor implementation.
 *
 * Reads battery voltage through a voltage divider connected to
 * an analog input pin. Supports various platform-specific ADC
 * configurations through the config structure.
 */
class AnalogBatterySensor : public BaseBatterySensor {
  public:
    /**
     * @brief Construct with configuration and chemistry.
     *
     * @param config Analog sensor configuration
     * @param chemistry Battery chemistry for SoC calculation
     */
    AnalogBatterySensor(const AnalogSensorConfig &config, const BatteryChemistry &chemistry);

    // IBatterySensor interface
    BatterySensorResult initialize() override;
    const char *getSensorTypeName() const override { return "AnalogADC"; }
    uint8_t getPriority() const override { return 10; } // Lowest priority (fallback)

  protected:
    BatterySensorResult readVoltageRaw(uint16_t &voltage_mv) override;
    ChargingState detectChargingState() override;
    bool detectExternalPower() override;
    bool detectBatteryPresent() override;

  private:
    AnalogSensorConfig config_;
    bool adc_enabled_;

    /**
     * @brief Enable ADC voltage divider.
     *
     * Activates any control GPIO before ADC read.
     */
    void enableADC();

    /**
     * @brief Disable ADC voltage divider.
     *
     * Deactivates control GPIO to save power.
     */
    void disableADC();

    /**
     * @brief Read raw ADC value with averaging.
     *
     * NASA Rule 2: Fixed sample count loop.
     *
     * @return Averaged raw ADC value
     */
    uint32_t readADCRaw();

    /**
     * @brief Convert raw ADC to millivolts.
     *
     * @param raw Raw ADC value
     * @return Voltage in millivolts
     */
    uint16_t convertToMillivolts(uint32_t raw);

    /**
     * @brief Platform-specific ADC initialization.
     *
     * @return true if successful
     */
    bool initializePlatformADC();
};

/**
 * @brief Create analog sensor with platform defaults.
 *
 * Factory function that creates sensor with appropriate
 * configuration for the current platform.
 *
 * @param chemistry Battery chemistry to use
 * @return Configured AnalogBatterySensor
 */
AnalogBatterySensor createAnalogSensor(const BatteryChemistry &chemistry);

} // namespace power
} // namespace meshtastic
