/**
 * @file BaseBatterySensor.h
 * @brief Base class for battery sensor implementations.
 *
 * Provides common functionality for all battery sensor implementations
 * including reading caching, filtering, and chemistry-based SoC calculation.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 3: No dynamic allocation after initialization
 * - Rule 4: Functions kept under 60 lines
 * - Rule 5: Assertions for parameter validation
 * - Rule 6: Minimal variable scope
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "../IBatterySensor.h"
#include "../IBatteryChemistry.h"
#include <cstdint>

namespace meshtastic {
namespace power {

/**
 * @brief Minimum interval between ADC reads in milliseconds.
 */
static constexpr uint32_t MIN_READ_INTERVAL_MS = 5000;

/**
 * @brief Number of samples for ADC averaging.
 *
 * Fixed constant per NASA Rule 2.
 */
static constexpr uint8_t ADC_SAMPLE_COUNT = 15;

/**
 * @brief Low-pass filter coefficient (0.0 - 1.0).
 *
 * Higher values = faster response, more noise.
 * Lower values = smoother output, slower response.
 */
static constexpr float LPF_COEFFICIENT = 0.5f;

/**
 * @brief Base class for battery sensor implementations.
 *
 * Provides common caching, filtering, and SoC calculation logic.
 * Derived classes implement hardware-specific read operations.
 */
class BaseBatterySensor : public IBatterySensor {
  public:
    /**
     * @brief Construct with battery chemistry.
     *
     * @param chemistry Battery chemistry for SoC calculation
     */
    explicit BaseBatterySensor(const BatteryChemistry &chemistry);

    virtual ~BaseBatterySensor() = default;

    // IBatterySensor interface implementation
    bool isInitialized() const override { return initialized_; }
    BatterySensorResult read(BatteryReading &reading) override;
    int16_t getVoltage_mV() override;
    int8_t getChargePercent() override;
    bool isBatteryPresent() override;
    bool isExternalPower() override;
    ChargingState getChargingState() override;

  protected:
    /**
     * @brief Hardware-specific voltage read.
     *
     * Must be implemented by derived classes.
     *
     * @param[out] voltage_mv Raw voltage in millivolts
     * @return BatterySensorResult indicating success/failure
     */
    virtual BatterySensorResult readVoltageRaw(uint16_t &voltage_mv) = 0;

    /**
     * @brief Hardware-specific charging state detection.
     *
     * Default implementation uses voltage-based detection.
     * Override for hardware with dedicated charging detection.
     *
     * @return ChargingState enumeration
     */
    virtual ChargingState detectChargingState();

    /**
     * @brief Hardware-specific external power detection.
     *
     * Default implementation uses voltage threshold.
     * Override for hardware with USB/external power detection.
     *
     * @return true if external power detected
     */
    virtual bool detectExternalPower();

    /**
     * @brief Hardware-specific battery presence detection.
     *
     * Default uses chemistry no-battery threshold.
     *
     * @return true if battery is present
     */
    virtual bool detectBatteryPresent();

    /**
     * @brief Apply low-pass filter to voltage reading.
     *
     * @param raw_mv Raw voltage reading
     * @return Filtered voltage value
     */
    uint16_t applyFilter(uint16_t raw_mv);

    /**
     * @brief Check if enough time has passed for new read.
     *
     * @return true if should perform new read
     */
    bool shouldReadNow() const;

    /**
     * @brief Update last read timestamp.
     */
    void updateReadTime();

    /**
     * @brief Get the battery chemistry reference.
     *
     * @return Reference to chemistry configuration
     */
    const BatteryChemistry &getChemistry() const { return chemistry_; }

    /**
     * @brief Set initialized flag.
     *
     * @param init Initialization state
     */
    void setInitialized(bool init) { initialized_ = init; }

    /**
     * @brief Get cached reading for derived class access.
     *
     * @return Reference to cached reading
     */
    BatteryReading &getCachedReading() { return cached_reading_; }

  private:
    BatteryChemistry chemistry_;          ///< Battery chemistry for SoC
    BatteryReading cached_reading_;       ///< Cached battery reading
    uint16_t filtered_voltage_mv_;        ///< Filtered voltage value
    uint32_t last_read_time_ms_;          ///< Last read timestamp
    bool initialized_;                     ///< Initialization state
    bool filter_initialized_;              ///< Filter state initialized

    /**
     * @brief Get current time in milliseconds.
     *
     * Abstracted for testability.
     *
     * @return Current time in milliseconds
     */
    virtual uint32_t getTimeMs() const;
};

} // namespace power
} // namespace meshtastic
