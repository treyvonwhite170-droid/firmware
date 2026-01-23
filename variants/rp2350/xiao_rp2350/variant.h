/**
 * @file variant.h
 * @brief XIAO RP2350 board variant configuration
 *
 * Pin mapping for Seeed Studio XIAO RP2350
 * Reference: https://wiki.seeedstudio.com/xiao_rp2350_getting_started/
 *
 * XIAO RP2350 Features:
 * - Dual-core Arm Cortex-M33 @ 150MHz (or dual RISC-V Hazard3 @ 150MHz)
 * - 520KB SRAM, 4MB Flash
 * - USB Type-C
 * - 19 GPIOs (analog capable)
 * - WS2812 RGB LED on GPIO22 (power on GPIO23)
 * - Low power modes: ~27µA dormant
 */

#ifndef _VARIANT_XIAO_RP2350_H_
#define _VARIANT_XIAO_RP2350_H_

#define ARDUINO_ARCH_AVR

// ============================================================
// I2C Configuration
// ============================================================
// Default I2C pins (I2C1)
// SDA = GPIO 6 (D4)
// SCL = GPIO 7 (D5)

// ============================================================
// SPI Configuration
// ============================================================
// Default SPI pins (directly usable)
// SCK  = GPIO 2 (D8)
// MOSI = GPIO 3 (D10)
// MISO = GPIO 4 (D9)

// ============================================================
// UART Configuration
// ============================================================
// Recommended pins for SerialModule:
// TX = GPIO 0 (D6)
// RX = GPIO 1 (D7)

// ============================================================
// User Interface
// ============================================================

// XIAO RP2350 has a WS2812 RGB LED
#define HAS_RGB_LED 1
#define RGB_LED_PIN 22       // WS2812 data pin
#define RGB_LED_POWER_PIN 23 // Power control for RGB LED

// User LED (alias for RGB LED control)
#define LED_PIN RGB_LED_PIN

// Boot/User button (directly connected)
#define BUTTON_PIN 27

// External notification output
#define EXT_NOTIFY_OUT 26

// ============================================================
// Power Management Configuration
// ============================================================

// Battery voltage sensing (directly via ADC)
// The XIAO RP2350 can read battery voltage on A0-A3
#define BATTERY_PIN 26                              // GPIO26 = A0
#define ADC_MULTIPLIER 2.0                          // Adjust based on your voltage divider
#define BATTERY_SENSE_RESOLUTION_BITS ADC_RESOLUTION

// Power management features
#define HAS_POWMAN 1                // RP2350 has POWMAN (Power Manager)
#define HAS_AON_TIMER 1             // RP2350 has AON (Always-On) Timer
#define HAS_LPOSC 1                 // RP2350 has Low-Power Oscillator

// USB VBUS detection for wake-from-shutdown
// GPIO24 is the standard VBUS sense pin on RP2350 boards (Pico 2 compatible)
// This pin goes HIGH when USB power (5V) is connected via USB-C or 5V pin
#define VBUS_SENSE_PIN 24           // GPIO24 = VBUS sense (high when USB connected)
#define EXT_PWR_DETECT VBUS_SENSE_PIN  // Alias for PowerFSM integration

// Sleep mode configuration
// GPIO pins that can wake from dormant mode (max 4 on RP2350)
#define WAKE_GPIO_PIN BUTTON_PIN    // Wake on button press

// Shutdown/wake configuration
// The board can be shut down and will wake when:
// 1. USB-C is plugged in (VBUS goes high)
// 2. 5V is applied to the 5V input pin
// Power consumption in shutdown: ~0.6-1.2mA

// ============================================================
// LoRa Radio Configuration (if using external module)
// ============================================================

// Default: No built-in LoRa (use external module)
// Uncomment and configure for your LoRa module

// #define USE_SX1262

// #undef LORA_SCK
// #undef LORA_MISO
// #undef LORA_MOSI
// #undef LORA_CS

// Example SPI1 configuration for external LoRa module
// #define LORA_SCK 10
// #define LORA_MISO 12
// #define LORA_MOSI 11
// #define LORA_CS 3

// #define LORA_DIO0 RADIOLIB_NC
// #define LORA_RESET 15
// #define LORA_DIO1 20
// #define LORA_DIO2 2
// #define LORA_DIO3 RADIOLIB_NC

// #ifdef USE_SX1262
// #define SX126X_CS LORA_CS
// #define SX126X_DIO1 LORA_DIO1
// #define SX126X_BUSY LORA_DIO2
// #define SX126X_RESET LORA_RESET
// #define SX126X_DIO2_AS_RF_SWITCH
// #define SX126X_DIO3_TCXO_VOLTAGE 1.8
// #endif

// ============================================================
// XIAO RP2350 GPIO Reference
// ============================================================
// D0  = GPIO 26 (A0) - ADC capable
// D1  = GPIO 27 (A1) - ADC capable
// D2  = GPIO 28 (A2) - ADC capable
// D3  = GPIO 29 (A3) - ADC capable
// D4  = GPIO 6  (SDA)
// D5  = GPIO 7  (SCL)
// D6  = GPIO 0  (TX)
// D7  = GPIO 1  (RX)
// D8  = GPIO 2  (SCK)
// D9  = GPIO 4  (MISO)
// D10 = GPIO 3  (MOSI)
//
// Internal (directly accessible in firmware):
// GPIO 22 = WS2812 RGB LED Data
// GPIO 23 = RGB LED Power Enable (active high)
// GPIO 24 = VBUS Sense (high when USB power connected)
// GPIO 25 = User LED (directly, active low on some boards)
//
// Power Management:
// - cpuShutdownDefault() - Shutdown until USB connected
// - cpuShutdown(gpio)    - Shutdown until specific GPIO
// - cpuDeepSleep(ms)     - Deep sleep for duration
// - cpuLightSleep(ms)    - Light sleep for duration
//
// Dual-Core (FreeRTOS):
// - launchCore1Task(func) - Run compute task on core1
// - stopCore1Task()       - Stop core1 task
// - isCore1TaskRunning()  - Check core1 status
// - createCore1Task(...)  - Create FreeRTOS task pinned to core1

// ============================================================
// FreeRTOS and Dual-Core Configuration
// ============================================================
// The XIAO RP2350 has dual Cortex-M33 cores (or RISC-V Hazard3).
// FreeRTOS SMP is enabled by default for this board.
//
// Core usage recommendations:
// - Core0: Main loop, I/O, networking, UI
// - Core1: Compute-heavy tasks (signal processing, crypto, etc.)
//
// Note: FreeRTOS on RISC-V cores is experimental (use ARM mode for production)

// Enable FreeRTOS SMP (dual-core scheduler)
#ifndef configNUMBER_OF_CORES
#define configNUMBER_OF_CORES 2
#endif

// Stack sizes for dual-core operation
// When using setup1/loop1, each core gets 4KB stack
// Set this to true for separate 8KB stacks per core
// #define core1_separate_stack true

// Core affinity for compute tasks
// Use createCore1Task() to pin tasks to core1
#define COMPUTE_CORE 1  // Core1 for compute-heavy tasks
#define IO_CORE 0       // Core0 for I/O operations

#endif // _VARIANT_XIAO_RP2350_H_
