/**
 * @file IBatteryChemistry.h
 * @brief Abstract interface for battery chemistry definitions.
 *
 * This interface defines battery chemistry characteristics including
 * Open Circuit Voltage (OCV) curves for accurate State of Charge (SoC)
 * estimation across different battery types.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 2: Fixed upper-bound loops (NUM_OCV_POINTS constant)
 * - Rule 4: Functions kept under 60 lines
 * - Rule 6: Minimal scope for data objects
 * - Rule 8: Minimal preprocessor usage
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include <cstdint>

namespace meshtastic {
namespace power {

/**
 * @brief Maximum number of OCV data points.
 *
 * Fixed constant per NASA Rule 2 (fixed upper-bound loops).
 */
static constexpr uint8_t MAX_OCV_POINTS = 11;

/**
 * @brief Maximum number of battery cells in series.
 *
 * Fixed constant for loop bounds.
 */
static constexpr uint8_t MAX_CELLS = 6;

/**
 * @brief Battery chemistry type enumeration.
 */
enum class BatteryType : uint8_t {
    LION = 0,    ///< Lithium-Ion (default)
    LIFEPO4,     ///< Lithium Iron Phosphate
    LEAD_ACID,   ///< Lead-Acid (e.g., 12V systems)
    ALKALINE,    ///< Alkaline (non-rechargeable)
    NIMH,        ///< Nickel-Metal Hydride
    LTO,         ///< Lithium Titanate
    CUSTOM,      ///< Custom user-defined curve
    TYPE_COUNT   ///< Number of battery types (for iteration bounds)
};

/**
 * @brief Battery chemistry configuration structure.
 *
 * Contains all parameters needed to characterize a battery
 * chemistry type for accurate SoC estimation.
 */
struct BatteryChemistryConfig {
    BatteryType type;                     ///< Battery chemistry type
    uint8_t num_cells;                    ///< Number of cells in series
    uint8_t num_ocv_points;               ///< Number of valid OCV points
    uint16_t ocv_mv[MAX_OCV_POINTS];      ///< OCV curve (100% to 0%)
    uint16_t charging_threshold_mv;       ///< Voltage above which charging detected
    uint16_t empty_threshold_mv;          ///< Voltage below which battery empty
    uint16_t no_battery_threshold_mv;     ///< Voltage below which no battery
    const char *name;                     ///< Human-readable chemistry name

    /**
     * @brief Default constructor with LiIon defaults.
     */
    BatteryChemistryConfig()
        : type(BatteryType::LION), num_cells(1), num_ocv_points(MAX_OCV_POINTS), ocv_mv{4190, 4050, 3990, 3890, 3800,
                                                                                        3720, 3630, 3530, 3420, 3300, 3100},
          charging_threshold_mv(4200), empty_threshold_mv(3100), no_battery_threshold_mv(2600), name("LiIon")
    {
    }
};

/**
 * @brief Abstract interface for battery chemistry.
 *
 * Provides methods to calculate State of Charge from voltage
 * and access chemistry-specific parameters.
 */
class IBatteryChemistry {
  public:
    virtual ~IBatteryChemistry() = default;

    /**
     * @brief Get the battery chemistry type.
     *
     * @return BatteryType enumeration value
     */
    virtual BatteryType getType() const = 0;

    /**
     * @brief Get the chemistry configuration.
     *
     * @return Reference to chemistry configuration
     */
    virtual const BatteryChemistryConfig &getConfig() const = 0;

    /**
     * @brief Calculate State of Charge from voltage.
     *
     * Uses OCV curve interpolation to estimate battery
     * charge percentage from measured voltage.
     *
     * @param voltage_mv Battery pack voltage in millivolts
     * @return Charge percentage 0-100, or -1 if invalid
     */
    virtual int8_t calculateSoC(uint16_t voltage_mv) const = 0;

    /**
     * @brief Check if voltage indicates charging.
     *
     * @param voltage_mv Battery pack voltage in millivolts
     * @return true if voltage suggests battery is charging
     */
    virtual bool isChargingVoltage(uint16_t voltage_mv) const = 0;

    /**
     * @brief Check if voltage indicates empty battery.
     *
     * @param voltage_mv Battery pack voltage in millivolts
     * @return true if voltage indicates battery is empty
     */
    virtual bool isEmptyVoltage(uint16_t voltage_mv) const = 0;

    /**
     * @brief Check if voltage indicates no battery present.
     *
     * @param voltage_mv Measured voltage in millivolts
     * @return true if voltage suggests no battery connected
     */
    virtual bool isNoBatteryVoltage(uint16_t voltage_mv) const = 0;

    /**
     * @brief Get chemistry name for logging.
     *
     * @return Constant string with chemistry name
     */
    virtual const char *getName() const = 0;

  protected:
    IBatteryChemistry() = default;
    IBatteryChemistry(const IBatteryChemistry &) = default;
    IBatteryChemistry &operator=(const IBatteryChemistry &) = default;
    IBatteryChemistry(IBatteryChemistry &&) = default;
    IBatteryChemistry &operator=(IBatteryChemistry &&) = default;
};

/**
 * @brief Concrete implementation of battery chemistry.
 *
 * Template-free implementation to avoid code bloat and
 * ensure deterministic behavior.
 */
class BatteryChemistry : public IBatteryChemistry {
  public:
    /**
     * @brief Construct with specific configuration.
     *
     * @param config Chemistry configuration
     */
    explicit BatteryChemistry(const BatteryChemistryConfig &config) : config_(config) {}

    /**
     * @brief Construct with battery type using built-in defaults.
     *
     * @param type Battery type to use
     * @param num_cells Number of cells in series (default 1)
     */
    explicit BatteryChemistry(BatteryType type = BatteryType::LION, uint8_t num_cells = 1);

    // Allow copy and move semantics for assignment in PowerManager
    BatteryChemistry(const BatteryChemistry &) = default;
    BatteryChemistry &operator=(const BatteryChemistry &) = default;
    BatteryChemistry(BatteryChemistry &&) = default;
    BatteryChemistry &operator=(BatteryChemistry &&) = default;

    BatteryType getType() const override { return config_.type; }

    const BatteryChemistryConfig &getConfig() const override { return config_; }

    /**
     * @brief Calculate SoC using linear interpolation on OCV curve.
     *
     * NASA Compliance:
     * - Rule 2: Loop bound is MAX_OCV_POINTS constant
     * - Rule 4: Function under 60 lines
     */
    int8_t calculateSoC(uint16_t voltage_mv) const override;

    bool isChargingVoltage(uint16_t voltage_mv) const override
    {
        return voltage_mv > (config_.charging_threshold_mv * config_.num_cells);
    }

    bool isEmptyVoltage(uint16_t voltage_mv) const override
    {
        return voltage_mv < (config_.empty_threshold_mv * config_.num_cells);
    }

    bool isNoBatteryVoltage(uint16_t voltage_mv) const override
    {
        return voltage_mv < (config_.no_battery_threshold_mv * config_.num_cells);
    }

    const char *getName() const override { return config_.name; }

  private:
    BatteryChemistryConfig config_;
};

/**
 * @brief Factory function to create chemistry for a battery type.
 *
 * @param type Battery chemistry type
 * @param num_cells Number of cells in series
 * @return Configured BatteryChemistry instance
 */
BatteryChemistry createChemistry(BatteryType type, uint8_t num_cells = 1);

} // namespace power
} // namespace meshtastic
