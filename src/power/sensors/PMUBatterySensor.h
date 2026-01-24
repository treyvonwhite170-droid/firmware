/**
 * @file PMUBatterySensor.h
 * @brief PMU-based battery sensor for AXP192/AXP2101 chips.
 *
 * Provides battery sensing through XPowers library PMU chips,
 * common on LilyGo T-Beam and similar ESP32 boards.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 3: PMU pointer passed at construction, no dynamic allocation
 * - Rule 4: Functions under 60 lines
 * - Rule 7: All I2C operations checked
 * - Rule 9: Single PMU pointer, well-controlled lifetime
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "BaseBatterySensor.h"

// Forward declaration to avoid including XPowers headers here
class XPowersLibInterface;

namespace meshtastic {
namespace power {

/**
 * @brief PMU chip model enumeration.
 */
enum class PMUModel : uint8_t {
    UNKNOWN = 0,
    AXP192,   ///< AXP192 (T-Beam v1.0/v1.1)
    AXP2101   ///< AXP2101 (T-Beam v1.2, T-Beam S3)
};

/**
 * @brief Configuration for PMU battery sensor.
 */
struct PMUSensorConfig {
    PMUModel model;              ///< PMU chip model
    uint8_t i2c_address;         ///< I2C address (typically 0x34)
    int8_t irq_pin;              ///< IRQ pin (-1 if not used)
    bool use_wire1;              ///< Use Wire1 instead of Wire
    uint16_t shutdown_voltage_mv; ///< Low voltage shutdown threshold

    PMUSensorConfig()
        : model(PMUModel::UNKNOWN), i2c_address(0x34), irq_pin(-1),
          use_wire1(false), shutdown_voltage_mv(2600)
    {
    }
};

/**
 * @brief PMU-based battery sensor implementation.
 *
 * Interfaces with AXP192/AXP2101 power management units
 * for accurate battery monitoring including:
 * - Calibrated voltage measurement
 * - Hardware charge detection
 * - USB/VBUS detection
 * - Battery presence detection
 */
class PMUBatterySensor : public BaseBatterySensor {
  public:
    /**
     * @brief Construct with PMU interface and configuration.
     *
     * @param pmu Pointer to initialized XPowersLibInterface
     * @param config PMU sensor configuration
     * @param chemistry Battery chemistry for SoC calculation
     */
    PMUBatterySensor(XPowersLibInterface *pmu, const PMUSensorConfig &config,
                     const BatteryChemistry &chemistry);

    // IBatterySensor interface
    BatterySensorResult initialize() override;
    const char *getSensorTypeName() const override;
    uint8_t getPriority() const override { return 100; } // Highest priority

    /**
     * @brief Get the PMU interface pointer.
     *
     * Allows access to additional PMU functions not in IBatterySensor.
     *
     * @return Pointer to XPowersLibInterface
     */
    XPowersLibInterface *getPMU() { return pmu_; }

    /**
     * @brief Check and clear PMU IRQ status.
     *
     * Should be called periodically to handle PMU events.
     *
     * @return true if any relevant IRQ was pending
     */
    bool handleIRQ();

    /**
     * @brief Get PMU model.
     *
     * @return Detected or configured PMU model
     */
    PMUModel getModel() const { return config_.model; }

  protected:
    BatterySensorResult readVoltageRaw(uint16_t &voltage_mv) override;
    ChargingState detectChargingState() override;
    bool detectExternalPower() override;
    bool detectBatteryPresent() override;

  private:
    XPowersLibInterface *pmu_;    ///< PMU interface (not owned)
    PMUSensorConfig config_;       ///< Sensor configuration
    bool irq_pending_;             ///< IRQ flag

    /**
     * @brief Detect PMU model from chip ID.
     *
     * @return Detected model or UNKNOWN
     */
    PMUModel detectModel();

    /**
     * @brief Configure PMU for battery monitoring.
     *
     * Sets up ADC channels, charge parameters, etc.
     *
     * @return true if successful
     */
    bool configurePMU();
};

/**
 * @brief Attempt to detect and create PMU sensor.
 *
 * Scans I2C for PMU chips and creates appropriate sensor
 * if found. Returns nullptr if no PMU detected.
 *
 * @param chemistry Battery chemistry to use
 * @return Pointer to PMUBatterySensor or nullptr
 */
PMUBatterySensor *detectPMUSensor(const BatteryChemistry &chemistry);

} // namespace power
} // namespace meshtastic
