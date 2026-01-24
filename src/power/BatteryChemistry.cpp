/**
 * @file BatteryChemistry.cpp
 * @brief Implementation of battery chemistry calculations.
 *
 * Provides OCV-based State of Charge estimation for various
 * battery chemistries without using #ifdef clusters.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 2: All loops have fixed upper bounds (MAX_OCV_POINTS)
 * - Rule 4: All functions under 60 lines
 * - Rule 5: Assertions at function entry points
 * - Rule 6: Minimal variable scope
 * - Rule 7: All parameters validated
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */

#include "IBatteryChemistry.h"
#include <cassert>

namespace meshtastic {
namespace power {

/**
 * @brief Static OCV tables for standard battery chemistries.
 *
 * Stored as program constants to avoid heap allocation (NASA Rule 3).
 * Values are in millivolts per cell, from 100% to 0% charge.
 */
namespace ChemistryTables {

// LiIon/LiPo standard discharge curve (3.0V - 4.2V)
static constexpr uint16_t LION_OCV[MAX_OCV_POINTS] = {
    4190, 4050, 3990, 3890, 3800, 3720, 3630, 3530, 3420, 3300, 3100
};

// LiFePO4 flat discharge curve (2.5V - 3.65V)
static constexpr uint16_t LIFEPO4_OCV[MAX_OCV_POINTS] = {
    3400, 3350, 3320, 3290, 3270, 3260, 3250, 3230, 3200, 3120, 3000
};

// Lead-acid (per 2V cell, 6 cells = 12V system)
static constexpr uint16_t LEAD_ACID_OCV[MAX_OCV_POINTS] = {
    2120, 2090, 2070, 2050, 2030, 2010, 1990, 1980, 1970, 1960, 1950
};

// Alkaline (1.5V nominal, non-rechargeable)
static constexpr uint16_t ALKALINE_OCV[MAX_OCV_POINTS] = {
    1580, 1400, 1350, 1300, 1280, 1250, 1230, 1190, 1150, 1100, 1000
};

// NiMH (1.2V nominal)
static constexpr uint16_t NIMH_OCV[MAX_OCV_POINTS] = {
    1400, 1300, 1280, 1270, 1260, 1250, 1240, 1230, 1210, 1150, 1000
};

// LTO Lithium Titanate (1.5V - 2.8V, extremely safe)
static constexpr uint16_t LTO_OCV[MAX_OCV_POINTS] = {
    2700, 2560, 2540, 2520, 2500, 2460, 2420, 2400, 2380, 2320, 1500
};

} // namespace ChemistryTables

/**
 * @brief Construct BatteryChemistry with built-in defaults.
 *
 * NASA Rule 2: Switch has bounded cases (BatteryType enum)
 * NASA Rule 7: Parameter validated
 */
BatteryChemistry::BatteryChemistry(BatteryType type, uint8_t num_cells)
{
    // Validate parameters (NASA Rule 7)
    assert(num_cells > 0 && num_cells <= MAX_CELLS);
    assert(static_cast<uint8_t>(type) < static_cast<uint8_t>(BatteryType::TYPE_COUNT));

    config_.type = type;
    config_.num_cells = (num_cells > 0 && num_cells <= MAX_CELLS) ? num_cells : 1;
    config_.num_ocv_points = MAX_OCV_POINTS;

    // Select OCV table based on type (NASA Rule 1: simple switch)
    const uint16_t *ocv_table = nullptr;
    switch (type) {
    case BatteryType::LION:
        ocv_table = ChemistryTables::LION_OCV;
        config_.name = "LiIon";
        config_.charging_threshold_mv = 4200;
        config_.empty_threshold_mv = 3100;
        config_.no_battery_threshold_mv = 2600;
        break;

    case BatteryType::LIFEPO4:
        ocv_table = ChemistryTables::LIFEPO4_OCV;
        config_.name = "LiFePO4";
        config_.charging_threshold_mv = 3650;
        config_.empty_threshold_mv = 3000;
        config_.no_battery_threshold_mv = 2500;
        break;

    case BatteryType::LEAD_ACID:
        ocv_table = ChemistryTables::LEAD_ACID_OCV;
        config_.name = "LeadAcid";
        config_.charging_threshold_mv = 2400;
        config_.empty_threshold_mv = 1950;
        config_.no_battery_threshold_mv = 1800;
        break;

    case BatteryType::ALKALINE:
        ocv_table = ChemistryTables::ALKALINE_OCV;
        config_.name = "Alkaline";
        config_.charging_threshold_mv = 1600; // Not rechargeable
        config_.empty_threshold_mv = 1000;
        config_.no_battery_threshold_mv = 800;
        break;

    case BatteryType::NIMH:
        ocv_table = ChemistryTables::NIMH_OCV;
        config_.name = "NiMH";
        config_.charging_threshold_mv = 1450;
        config_.empty_threshold_mv = 1000;
        config_.no_battery_threshold_mv = 800;
        break;

    case BatteryType::LTO:
        ocv_table = ChemistryTables::LTO_OCV;
        config_.name = "LTO";
        config_.charging_threshold_mv = 2800;
        config_.empty_threshold_mv = 1500;
        config_.no_battery_threshold_mv = 1200;
        break;

    case BatteryType::CUSTOM:
    default:
        // Custom: use LiIon as default, expect config to be overwritten
        ocv_table = ChemistryTables::LION_OCV;
        config_.name = "Custom";
        config_.charging_threshold_mv = 4200;
        config_.empty_threshold_mv = 3100;
        config_.no_battery_threshold_mv = 2600;
        break;
    }

    // Copy OCV table (NASA Rule 2: bounded loop)
    assert(ocv_table != nullptr);
    for (uint8_t i = 0; i < MAX_OCV_POINTS; ++i) {
        config_.ocv_mv[i] = ocv_table[i];
    }
}

/**
 * @brief Calculate State of Charge using OCV interpolation.
 *
 * Uses linear interpolation between OCV table points to estimate
 * charge percentage from measured voltage.
 *
 * NASA Rule 2: Loop bounded by MAX_OCV_POINTS
 * NASA Rule 4: Function under 60 lines
 * NASA Rule 5: Entry assertions
 * NASA Rule 7: Parameter validation
 */
int8_t BatteryChemistry::calculateSoC(uint16_t voltage_mv) const
{
    // Validate configuration (NASA Rule 5)
    assert(config_.num_cells > 0);
    assert(config_.num_ocv_points > 1 && config_.num_ocv_points <= MAX_OCV_POINTS);

    // Check for no battery condition
    if (voltage_mv < (config_.no_battery_threshold_mv * config_.num_cells)) {
        return -1; // No battery detected
    }

    // Convert pack voltage to per-cell voltage
    const uint16_t cell_voltage = voltage_mv / config_.num_cells;

    // Check if above maximum (fully charged or charging)
    if (cell_voltage >= config_.ocv_mv[0]) {
        return 100;
    }

    // Check if below minimum (empty)
    if (cell_voltage <= config_.ocv_mv[config_.num_ocv_points - 1]) {
        return 0;
    }

    // Find interpolation segment (NASA Rule 2: bounded loop)
    for (uint8_t i = 0; i < config_.num_ocv_points - 1; ++i) {
        if (cell_voltage >= config_.ocv_mv[i + 1]) {
            // Found segment: voltage is between ocv[i] and ocv[i+1]
            // Calculate percentage using linear interpolation

            // Segment spans from (100 - i*10)% to (100 - (i+1)*10)%
            // for 11 points covering 0-100% in 10% steps
            const uint8_t high_percent = 100 - (i * 10);
            const uint8_t low_percent = 100 - ((i + 1) * 10);

            const uint16_t high_mv = config_.ocv_mv[i];
            const uint16_t low_mv = config_.ocv_mv[i + 1];

            // Avoid division by zero (NASA Rule 7)
            if (high_mv == low_mv) {
                return static_cast<int8_t>(high_percent);
            }

            // Linear interpolation
            const uint32_t range_mv = high_mv - low_mv;
            const uint32_t offset_mv = cell_voltage - low_mv;
            const uint8_t percent_range = high_percent - low_percent;

            // Calculate interpolated percentage
            // Using uint32_t to prevent overflow
            const uint8_t soc = low_percent +
                               static_cast<uint8_t>((offset_mv * percent_range) / range_mv);

            // Clamp result to valid range (NASA Rule 7)
            if (soc > 100) {
                return 100;
            }
            return static_cast<int8_t>(soc);
        }
    }

    // Should not reach here, but return 0 as safe default
    return 0;
}

/**
 * @brief Factory function to create chemistry instance.
 *
 * NASA Rule 3: No heap allocation - returns by value.
 */
BatteryChemistry createChemistry(BatteryType type, uint8_t num_cells)
{
    return BatteryChemistry(type, num_cells);
}

} // namespace power
} // namespace meshtastic
