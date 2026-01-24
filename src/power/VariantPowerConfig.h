/**
 * @file VariantPowerConfig.h
 * @brief Variant-specific power configuration adapter.
 *
 * This file bridges variant.h defines to the power management abstractions,
 * ensuring all board-specific power configurations are properly translated
 * into the sensor and sleep manager configurations.
 *
 * IMPORTANT: This file must be included AFTER configuration.h to ensure
 * all variant defines are available.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 8: Preprocessor limited to configuration adaptation
 * - Rule 6: Minimal scope - configuration only
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "configuration.h"  // Must be first - brings in variant.h
#include "sensors/AnalogBatterySensor.h"
#include "sleep/ESP32SleepManager.h"
#include "IBatteryChemistry.h"

namespace meshtastic {
namespace power {

// ============================================================================
// Battery Chemistry Configuration from Variant
// ============================================================================

/**
 * @brief Get battery type from variant defines.
 *
 * Translates CELL_TYPE_* defines to BatteryType enum.
 */
inline BatteryType getVariantBatteryType()
{
#if defined(CELL_TYPE_LIFEPO4)
    return BatteryType::LIFEPO4;
#elif defined(CELL_TYPE_LEADACID)
    return BatteryType::LEAD_ACID;
#elif defined(CELL_TYPE_ALKALINE)
    return BatteryType::ALKALINE;
#elif defined(CELL_TYPE_NIMH)
    return BatteryType::NIMH;
#elif defined(CELL_TYPE_LTO)
    return BatteryType::LTO;
#else
    return BatteryType::LION;  // Default
#endif
}

/**
 * @brief Get number of battery cells from variant.
 */
inline uint8_t getVariantCellCount()
{
#if defined(NUM_CELLS)
    return NUM_CELLS;
#else
    return 1;
#endif
}

/**
 * @brief Check if variant defines custom OCV array.
 */
inline bool hasVariantOCVArray()
{
#if defined(OCV_ARRAY)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Create battery chemistry with variant-specific OCV if defined.
 */
inline BatteryChemistry createVariantChemistry()
{
    BatteryType type = getVariantBatteryType();
    uint8_t cells = getVariantCellCount();

#if defined(OCV_ARRAY)
    // Use variant-specific OCV array
    BatteryChemistryConfig config;
    config.type = type;
    config.num_cells = cells;
    config.num_ocv_points = MAX_OCV_POINTS;

    // Copy variant OCV array
    static const uint16_t variant_ocv[MAX_OCV_POINTS] = { OCV_ARRAY };
    for (uint8_t i = 0; i < MAX_OCV_POINTS; ++i) {
        config.ocv_mv[i] = variant_ocv[i];
    }

    // Set thresholds based on OCV
    config.charging_threshold_mv = variant_ocv[0] + 10;
    config.empty_threshold_mv = variant_ocv[MAX_OCV_POINTS - 1];
    config.no_battery_threshold_mv = variant_ocv[MAX_OCV_POINTS - 1] - 500;
    config.name = "Variant";

    return BatteryChemistry(config);
#else
    return BatteryChemistry(type, cells);
#endif
}

// ============================================================================
// Analog Sensor Configuration from Variant
// ============================================================================

/**
 * @brief Create analog sensor configuration from variant defines.
 *
 * Translates all variant.h ADC and power detection defines
 * into AnalogSensorConfig structure.
 */
inline AnalogSensorConfig createVariantAnalogConfig()
{
    AnalogSensorConfig config;

    // Battery ADC pin
#if defined(BATTERY_PIN)
    config.battery_pin = BATTERY_PIN;
#else
    config.battery_pin = -1;
#endif

    // ADC control pin
#if defined(ADC_CTRL)
    config.adc_ctrl_pin = ADC_CTRL;
#if defined(ADC_CTRL_ENABLED)
    // ADC_CTRL_ENABLED can be HIGH, LOW, 1, or 0
    config.adc_ctrl_active_high = (ADC_CTRL_ENABLED == HIGH || ADC_CTRL_ENABLED == 1);
#else
    config.adc_ctrl_active_high = true;
#endif
#if defined(ADC_USE_PULLUP)
    config.adc_ctrl_use_pullup = true;
#endif
#endif

    // ADC multiplier (voltage divider ratio)
#if defined(ADC_MULTIPLIER)
    config.adc_multiplier = ADC_MULTIPLIER;
#else
    config.adc_multiplier = 2.0f;
#endif

    // Reference voltage
#if defined(AREF_VOLTAGE)
    config.reference_voltage = AREF_VOLTAGE;
#elif defined(ARCH_NRF52)
    config.reference_voltage = 3.6f;  // NRF52 internal reference
#else
    config.reference_voltage = 3.3f;
#endif

    // ADC resolution
#if defined(BATTERY_SENSE_RESOLUTION_BITS)
    config.resolution_bits = BATTERY_SENSE_RESOLUTION_BITS;
#elif defined(ADC_RESOLUTION)
    config.resolution_bits = ADC_RESOLUTION;
#else
    config.resolution_bits = 12;
#endif

    // ADC unit (ESP32 specific)
#if defined(BAT_MEASURE_ADC_UNIT)
    config.adc_unit = BAT_MEASURE_ADC_UNIT;
#else
    config.adc_unit = 0;  // ADC1 by default
#endif

    // External power detection
#if defined(EXT_PWR_DETECT)
    config.ext_pwr_detect_pin = EXT_PWR_DETECT;
#if defined(HELTEC_CAPSULE_SENSOR_V3) || defined(HELTEC_SENSOR_HUB)
    config.ext_pwr_active_high = false;  // These boards are active LOW
#else
    config.ext_pwr_active_high = true;
#endif
#endif

    // Charge detection
#if defined(EXT_CHRG_DETECT)
    config.ext_chrg_detect_pin = EXT_CHRG_DETECT;
#if defined(EXT_CHRG_DETECT_VALUE)
    config.ext_chrg_active_high = (EXT_CHRG_DETECT_VALUE == HIGH);
#else
    config.ext_chrg_active_high = true;
#endif
#endif

    // Battery immutable flag (integrated battery)
#if defined(BATTERY_IMMUTABLE)
    config.battery_immutable = true;
#endif

    return config;
}

/**
 * @brief Check if variant has analog battery sensing.
 */
inline bool hasVariantAnalogBattery()
{
#if defined(BATTERY_PIN)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if variant has PMU.
 */
inline bool hasVariantPMU()
{
#if defined(HAS_PMU) || defined(HAS_AXP192) || defined(HAS_AXP2101)
    return true;
#else
    return false;
#endif
}

// ============================================================================
// Sleep Manager Configuration from Variant
// ============================================================================

#if defined(ARCH_ESP32)
/**
 * @brief Create ESP32 wake configuration from variant defines.
 */
inline ESP32WakeConfig createVariantESP32WakeConfig()
{
    ESP32WakeConfig config;

    // Button wake pin
#if defined(BUTTON_PIN)
    config.button_pin = BUTTON_PIN;
#if defined(BUTTON_NEED_PULLUP)
    config.button_need_pullup = true;
#endif
#endif

    // LoRa interrupt pin
#if defined(LORA_DIO1) && (LORA_DIO1 != -1)
    config.lora_dio1_pin = LORA_DIO1;
#elif defined(SX126X_DIO1)
    config.lora_dio1_pin = SX126X_DIO1;
#endif

    // PMU interrupt pin (often disabled due to spurious wakes)
#if defined(PMU_IRQ)
    config.pmu_irq_pin = PMU_IRQ;
#endif

    // Rotary encoder
#if defined(ROTARY_PRESS)
    config.rotary_press_pin = ROTARY_PRESS;
#endif

    // Keyboard interrupt
#if defined(KB_INT)
    config.keyboard_int_pin = KB_INT;
#endif

    // Touch screen
#if defined(SCREEN_TOUCH_INT) && defined(WAKE_ON_TOUCH)
    config.touch_int_pin = SCREEN_TOUCH_INT;
#endif

    return config;
}
#endif

// ============================================================================
// Peripheral Power Configuration from Variant
// ============================================================================

/**
 * @brief Check if variant has peripheral power enable.
 */
inline bool hasVariantPeripheralPower()
{
#if defined(PIN_POWER_EN) || defined(VEXT_ENABLE)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get peripheral power enable pin.
 */
inline int8_t getVariantPowerEnablePin()
{
#if defined(PIN_POWER_EN)
    return PIN_POWER_EN;
#elif defined(VEXT_ENABLE)
    return VEXT_ENABLE;
#else
    return -1;
#endif
}

/**
 * @brief Get peripheral power active state.
 */
inline bool getVariantPowerEnableActiveHigh()
{
#if defined(VEXT_ON_VALUE)
    return (VEXT_ON_VALUE == HIGH);
#elif defined(VEXT_ENABLE)
    return false;  // VEXT is typically active LOW
#else
    return true;  // Default active HIGH
#endif
}

// ============================================================================
// Debug/Logging Helpers
// ============================================================================

/**
 * @brief Get variant power configuration summary string.
 *
 * Useful for debugging configuration issues.
 */
inline const char* getVariantPowerSummary()
{
#if defined(HAS_PMU)
    return "PMU";
#elif defined(BATTERY_PIN)
    return "Analog";
#else
    return "None";
#endif
}

} // namespace power
} // namespace meshtastic
