# Power Management Abstraction Layer

This directory contains a refactored power management system that eliminates `#ifdef` clusters
through clean interface abstractions, following NASA's 10 rules for safety-critical code.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PowerManager                                 │
│  (Unified entry point - singleton pattern)                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌───────────────┐  ┌───────────────┐  ┌────────────────────┐      │
│  │ IBatterySensor │  │ ISleepManager │  │ IPeripheralPower   │      │
│  └───────┬───────┘  └───────┬───────┘  └─────────┬──────────┘      │
│          │                  │                     │                  │
│  ┌───────┴───────┐  ┌───────┴───────┐  ┌─────────┴──────────┐      │
│  │ Implementations│  │ Implementations│  │ Implementations    │      │
│  │ - AnalogSensor │  │ - ESP32Sleep   │  │ - BoardPeripheral  │      │
│  │ - PMUSensor    │  │ - NRF52Sleep   │  │ - NullPeripheral   │      │
│  │ - NullSensor   │  │ - RP2040Sleep  │  │                    │      │
│  └────────────────┘  └────────────────┘  └────────────────────┘      │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## NASA Safety-Critical Coding Compliance

All code in this module follows NASA's 10 rules for safety-critical code:

| Rule | Description | Implementation |
|------|-------------|----------------|
| 1 | Simple control flow | Virtual interface pattern, simple switch statements |
| 2 | Fixed upper-bound loops | Constants: MAX_OCV_POINTS, MAX_WAKE_SOURCES, ADC_SAMPLE_COUNT |
| 3 | No dynamic allocation after init | Static storage, stack allocation |
| 4 | Functions ≤ 60 lines | All functions kept under limit |
| 5 | ≥2 assertions per function | Entry point validation assertions |
| 6 | Smallest scope | Private members, local variables |
| 7 | Check return values | Result enums, explicit validation |
| 8 | Limited preprocessor | Platform isolation, no complex macros |
| 9 | Restrict pointers | Interface pointers only, no pointer arithmetic |
| 10 | All warnings enabled | Clean compilation required |

## Directory Structure

```
src/power/
├── README.md                    # This file
├── IBatterySensor.h             # Battery sensor interface
├── IBatteryChemistry.h          # Battery chemistry interface
├── BatteryChemistry.cpp         # Chemistry implementations
├── ISleepManager.h              # Sleep manager interface
├── IPeripheralPower.h           # Peripheral power interface
├── PowerManager.h               # Unified manager interface
├── PowerManager.cpp             # Manager implementation
├── sensors/
│   ├── BaseBatterySensor.h      # Base sensor class
│   ├── BaseBatterySensor.cpp    # Base implementation
│   ├── AnalogBatterySensor.h    # ADC-based sensor
│   ├── AnalogBatterySensor.cpp  # ADC implementation
│   ├── PMUBatterySensor.h       # PMU-based sensor
│   ├── PMUBatterySensor.cpp     # PMU implementation
│   └── NullBatterySensor.h      # Null object pattern
└── sleep/
    ├── BaseSleepManager.h       # Base sleep manager
    ├── BaseSleepManager.cpp     # Base implementation
    ├── ESP32SleepManager.h      # ESP32-specific sleep
    └── ESP32SleepManager.cpp    # ESP32 implementation
```

## Usage

### Basic Usage

```cpp
#include "power/PowerManager.h"

using namespace meshtastic::power;

void setup() {
    // Initialize with default configuration
    PowerManager& pm = PowerManager::getInstance();
    PowerManagerResult result = pm.initialize();

    if (result != PowerManagerResult::SUCCESS) {
        // Handle initialization error
    }
}

void loop() {
    PowerManager& pm = PowerManager::getInstance();

    // Get battery status
    BatteryReading reading;
    if (pm.getBatteryReading(reading) == BatterySensorResult::SUCCESS) {
        if (reading.charge_percent < 5 && reading.battery_present) {
            // Low battery - enter deep sleep
            pm.deepSleep(SLEEP_FOREVER);
        }
    }
}
```

### Custom Configuration

```cpp
PowerManagerConfig config;
config.battery_type = BatteryType::LIFEPO4;  // LiFePO4 battery
config.num_cells = 1;
config.low_voltage_count = 10;
config.enable_power_saving = true;

PowerManager& pm = PowerManager::getInstance();
pm.initialize(config);
```

### Direct Sensor Access

```cpp
PowerManager& pm = PowerManager::getInstance();

// Get raw battery sensor
IBatterySensor* sensor = pm.getBatterySensor();
LOG_INFO("Using sensor: %s", sensor->getSensorTypeName());

// Get sleep manager
ISleepManager* sleep = pm.getSleepManager();
if (sleep && sleep->isModeSupported(SleepMode::LIGHT)) {
    WakeCause cause = sleep->lightSleep(5000);
    LOG_INFO("Woke from light sleep, cause: %d", cause);
}
```

## Battery Chemistry Support

The system supports multiple battery chemistries with accurate SoC estimation:

| Chemistry | Voltage Range | Use Case |
|-----------|---------------|----------|
| LiIon (default) | 3.0V - 4.2V | Most devices |
| LiFePO4 | 2.5V - 3.65V | Solar applications |
| Lead-Acid | 1.95V - 2.12V (per cell) | 12V systems |
| Alkaline | 1.0V - 1.58V | Non-rechargeable |
| NiMH | 1.0V - 1.4V | Rechargeable AA/AAA |
| LTO | 1.5V - 2.8V | Extreme conditions |

## Sleep Modes

| Mode | Description | RAM | Wake Time |
|------|-------------|-----|-----------|
| MODEM | WiFi/BT power management | Preserved | Instant |
| LIGHT | CPU halted, fast wake | Preserved | ~1ms |
| DEEP | Full sleep, reboot on wake | RTC only | ~100ms |
| SHUTDOWN | Power off | None | Button press |

## Adding New Sensors

1. Create a new class inheriting from `BaseBatterySensor`
2. Implement required virtual methods:
   - `initialize()` - Hardware setup
   - `readVoltageRaw()` - Raw voltage read
   - `getSensorTypeName()` - Identifier string
   - `getPriority()` - Detection priority (higher = try first)
3. Optionally override detection methods:
   - `detectChargingState()`
   - `detectExternalPower()`
   - `detectBatteryPresent()`
4. Register in `PowerManager::initializeBatterySensor()`

### Example: Custom Fuel Gauge Sensor

```cpp
class MAX17048Sensor : public BaseBatterySensor {
public:
    MAX17048Sensor(const BatteryChemistry& chemistry)
        : BaseBatterySensor(chemistry) {}

    BatterySensorResult initialize() override {
        // Initialize I2C communication with MAX17048
        if (!i2c_probe(MAX17048_ADDR)) {
            return BatterySensorResult::ERROR_COMMUNICATION;
        }
        setInitialized(true);
        return BatterySensorResult::SUCCESS;
    }

    const char* getSensorTypeName() const override { return "MAX17048"; }
    uint8_t getPriority() const override { return 50; } // Medium priority

protected:
    BatterySensorResult readVoltageRaw(uint16_t& voltage_mv) override {
        // Read from fuel gauge registers
        voltage_mv = read_vcell_register();
        return BatterySensorResult::SUCCESS;
    }
};
```

## Adding Platform Sleep Support

1. Create a new class inheriting from `BaseSleepManager`
2. Implement platform-specific sleep methods
3. Register in `PowerManager::initializeSleepManager()`

## Migration from Legacy Code

The new system is designed for gradual migration:

1. **Phase 1**: Include `PowerManager.h` alongside existing code
2. **Phase 2**: Route battery reads through PowerManager
3. **Phase 3**: Route sleep calls through PowerManager
4. **Phase 4**: Remove legacy code

### Legacy Compatibility

```cpp
// Legacy code
#ifdef BATTERY_PIN
uint16_t voltage = analogRead(BATTERY_PIN);
#endif

// New code (works everywhere)
int16_t voltage = PowerManager::getInstance().getBatteryVoltage_mV();
```

## Testing

The abstraction layer enables unit testing without hardware:

```cpp
// Create mock sensor
class MockBatterySensor : public IBatterySensor {
    // ... implement with test values
};

// Inject into test
MockBatterySensor mock;
// ... run tests
```

## Changelog

### v1.0.0 (Initial Release)
- Abstract interfaces for battery sensing, sleep management, peripheral power
- NASA safety-critical coding compliance
- Support for ESP32, NRF52, RP2040 platforms
- LiIon, LiFePO4, Lead-Acid, Alkaline, NiMH, LTO battery chemistries
- Null object pattern for graceful degradation

## License

GPL-3.0 - Part of the Meshtastic project

## References

- [NASA JPL Coding Standard](https://www.nasa.gov/pdf/418878main_NASASTD_8739_8B_with_Change1.pdf)
- [Power of Ten - Rules for Developing Safety-Critical Code](https://spinroot.com/gerard/pdf/P10.pdf)
