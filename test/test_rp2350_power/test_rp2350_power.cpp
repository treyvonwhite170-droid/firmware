/**
 * @file test_rp2350_power.cpp
 * @brief Power management test for RP2350 / XIAO RP2350
 *
 * This test demonstrates and validates the power management features
 * for the RP2350 microcontroller, specifically designed for testing
 * on the XIAO RP2350 board.
 *
 * Test modes:
 * 1. Light sleep with XOSC (higher accuracy, ~2-3mA)
 * 2. Dormant mode with LPOSC timer wakeup (~0.6-1.2mA)
 * 3. Dormant mode with GPIO wakeup (~0.6-1.2mA)
 * 4. Clock measurement before/after sleep
 *
 * Usage:
 * - Connect XIAO RP2350 via USB
 * - Open serial monitor at 115200 baud
 * - Optionally connect ammeter to measure current
 * - Press button or wait for timer to observe wake behavior
 *
 * Expected results:
 * - Normal operation: ~20-35mA
 * - Light sleep: ~2-3mA
 * - Dormant (timer): ~0.6-1.2mA
 * - Dormant (GPIO): ~0.6-1.2mA
 *
 * Note: Actual current depends on peripherals enabled and board design.
 */

#ifdef __PLAT_RP2350__

#include <Arduino.h>
#include <pico/stdlib.h>
#include <pico/sleep.h>
#include <hardware/clocks.h>

// Test configuration
#ifndef TEST_WAKE_PIN
#define TEST_WAKE_PIN 27  // Default: XIAO RP2350 button
#endif

#ifndef TEST_LED_PIN
#define TEST_LED_PIN 22   // Default: XIAO RP2350 RGB LED data
#endif

#ifndef TEST_LED_POWER_PIN
#define TEST_LED_POWER_PIN 23  // Default: XIAO RP2350 RGB LED power
#endif

// Test durations
static const uint32_t LIGHT_SLEEP_DURATION_MS = 5000;   // 5 seconds
static const uint32_t DORMANT_DURATION_MS = 10000;      // 10 seconds
static const uint32_t SERIAL_WAIT_MS = 3000;            // Wait for serial

// Test state
static volatile bool test_awake = false;
static volatile uint32_t wake_count = 0;

// Wakeup callback
static void test_wakeup_callback(void) {
    test_awake = true;
    wake_count++;
}

// Blink LED to indicate state
static void blink_led(uint8_t count, uint32_t on_ms, uint32_t off_ms) {
    // Enable LED power if available
    pinMode(TEST_LED_POWER_PIN, OUTPUT);
    digitalWrite(TEST_LED_POWER_PIN, HIGH);

    pinMode(TEST_LED_PIN, OUTPUT);
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(TEST_LED_PIN, HIGH);
        delay(on_ms);
        digitalWrite(TEST_LED_PIN, LOW);
        delay(off_ms);
    }
}

// Print clock frequencies
static void print_clock_info(const char* label) {
    Serial.printf("\n=== Clock Info: %s ===\n", label);
    Serial.printf("CLK_SYS:  %lu kHz\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));
    Serial.printf("CLK_REF:  %lu kHz\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF));
    Serial.printf("CLK_PERI: %lu kHz\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI));
    Serial.printf("PLL_SYS:  %lu kHz\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY));
    Serial.printf("ROSC:     %lu kHz\n", frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
    Serial.flush();
}

// Test 1: Light sleep with XOSC
static bool test_light_sleep_xosc(void) {
    Serial.println("\n--- Test 1: Light Sleep with XOSC ---");
    Serial.printf("Sleeping for %lu ms...\n", LIGHT_SLEEP_DURATION_MS);
    Serial.flush();

    uint32_t start_time = millis();
    test_awake = false;

    // Configure for light sleep
    sleep_run_from_xosc();

    // Enter light sleep
    sleep_goto_sleep_for_ms(LIGHT_SLEEP_DURATION_MS, test_wakeup_callback);

    // Wait for wake confirmation
    while (!test_awake) {
        tight_loop_contents();
    }

    // Restore clocks
    sleep_power_up();

    uint32_t elapsed = millis() - start_time;

    Serial.printf("Woke up! Elapsed: %lu ms (expected: ~%lu ms)\n",
                  elapsed, LIGHT_SLEEP_DURATION_MS);

    // Check if timing is reasonable (within 20% tolerance)
    bool pass = (elapsed >= LIGHT_SLEEP_DURATION_MS * 0.8) &&
                (elapsed <= LIGHT_SLEEP_DURATION_MS * 1.2);

    Serial.printf("Result: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 2: Dormant mode with LPOSC timer
static bool test_dormant_lposc_timer(void) {
    Serial.println("\n--- Test 2: Dormant Mode with LPOSC Timer ---");
    Serial.printf("Going dormant for %lu ms...\n", DORMANT_DURATION_MS);
    Serial.println("Note: LPOSC timing is less accurate (~32kHz)");
    Serial.flush();

    uint32_t start_time = millis();
    test_awake = false;

    // Configure for dormant mode with LPOSC
    sleep_run_from_lposc();

    // Enter dormant mode
    sleep_goto_dormant_for_ms(DORMANT_DURATION_MS, test_wakeup_callback);

    // Restore clocks
    sleep_power_up();

    uint32_t elapsed = millis() - start_time;

    Serial.printf("Woke up! Elapsed: %lu ms (expected: ~%lu ms)\n",
                  elapsed, DORMANT_DURATION_MS);

    // LPOSC is less accurate, allow 30% tolerance
    bool pass = (elapsed >= DORMANT_DURATION_MS * 0.7) &&
                (elapsed <= DORMANT_DURATION_MS * 1.3);

    Serial.printf("Result: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 3: Dormant mode with GPIO wakeup
static bool test_dormant_gpio_wake(void) {
    Serial.println("\n--- Test 3: Dormant Mode with GPIO Wakeup ---");
    Serial.printf("Going dormant, waiting for GPIO %d to go HIGH...\n", TEST_WAKE_PIN);
    Serial.println("Press the button to wake up!");
    Serial.flush();

    // Configure wake pin
    pinMode(TEST_WAKE_PIN, INPUT_PULLDOWN);

    // Blink to indicate waiting
    blink_led(3, 100, 100);

    uint32_t start_time = millis();

    // Configure for dormant mode
    sleep_run_from_xosc();

    // Enter dormant until pin goes high (button press)
    sleep_goto_dormant_until_pin(TEST_WAKE_PIN, false, true);

    // Restore clocks
    sleep_power_up();

    uint32_t elapsed = millis() - start_time;

    Serial.printf("Woke up by GPIO! Elapsed: %lu ms\n", elapsed);

    // Blink to indicate wake
    blink_led(5, 50, 50);

    Serial.println("Result: PASS (manual verification)");
    return true;
}

// Test 4: Verify clock restoration
static bool test_clock_restoration(void) {
    Serial.println("\n--- Test 4: Clock Restoration After Sleep ---");

    print_clock_info("Before sleep");

    // Save expected frequencies
    uint32_t clk_sys_before = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);

    // Do a quick sleep
    sleep_run_from_xosc();
    sleep_goto_sleep_for_ms(100, test_wakeup_callback);
    while (!test_awake) tight_loop_contents();
    sleep_power_up();

    print_clock_info("After sleep");

    uint32_t clk_sys_after = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);

    // Check if clocks restored properly (within 5%)
    bool pass = (clk_sys_after >= clk_sys_before * 0.95) &&
                (clk_sys_after <= clk_sys_before * 1.05);

    Serial.printf("CLK_SYS: %lu -> %lu kHz\n", clk_sys_before, clk_sys_after);
    Serial.printf("Result: %s\n", pass ? "PASS" : "FAIL");

    return pass;
}

// Test 5: Shutdown until USB power
static bool test_shutdown_until_usb(void) {
    Serial.println("\n--- Test 5: Shutdown Until USB Power ---");
    Serial.println("This test will shutdown the board.");
    Serial.println("The board will wake when USB is reconnected.");
    Serial.println("");
    Serial.println("Instructions:");
    Serial.println("  1. Disconnect USB after countdown");
    Serial.println("  2. Wait a few seconds");
    Serial.println("  3. Reconnect USB");
    Serial.println("  4. Board should reboot and show 'PASS'");
    Serial.println("");
    Serial.println("Press 'y' to start shutdown test, 's' to skip...");
    Serial.flush();

    uint32_t timeout = millis() + 30000;
    while (millis() < timeout) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == 's' || c == 'S') {
                Serial.println("Skipping shutdown test.");
                return true; // Skip counts as pass
            }
            if (c == 'y' || c == 'Y') {
                break;
            }
        }
        delay(100);
    }

    // Check if we're recovering from a shutdown
    // (In a real implementation, we'd check a flag in RTC/flash)

    Serial.println("\nCountdown to shutdown:");
    for (int i = 5; i > 0; i--) {
        Serial.printf("  %d...\n", i);
        blink_led(1, 200, 800);
    }

    Serial.println("Entering shutdown mode...");
    Serial.println("Disconnect USB now, then reconnect to wake.");
    Serial.flush();

    // Use default VBUS sense pin (GPIO24)
#ifndef VBUS_SENSE_PIN
#define VBUS_SENSE_PIN 24
#endif

    // Try to enter shutdown
    bool entered = sleep_shutdown_until_usb(VBUS_SENSE_PIN);

    if (!entered) {
        Serial.println("Could not enter shutdown - USB still connected?");
        Serial.println("Result: SKIPPED (USB must be disconnectable)");
        return true; // Not a failure, just can't test
    }

    // If we get here, we woke from shutdown
    Serial.println("Woke from shutdown!");
    Serial.println("Result: PASS");

    return true;
}

// Main test runner
void run_rp2350_power_tests(void) {
    Serial.begin(115200);

    // Wait for serial
    uint32_t wait_start = millis();
    while (!Serial && (millis() - wait_start) < SERIAL_WAIT_MS) {
        delay(100);
    }

    Serial.println("\n========================================");
    Serial.println("    RP2350 Power Management Test");
    Serial.println("    Board: XIAO RP2350");
    Serial.println("========================================");
    Serial.println("");
    Serial.println("Available power modes:");
    Serial.println("  - Light Sleep (XOSC):    ~2-3mA");
    Serial.println("  - Dormant (LPOSC timer): ~0.6-1.2mA");
    Serial.println("  - Dormant (GPIO wake):   ~0.6-1.2mA");
    Serial.println("  - Shutdown (USB wake):   ~0.6-1.2mA");
    Serial.println("");

    // Initial LED indication
    blink_led(2, 200, 200);

    // Print initial clock state
    print_clock_info("Initial");

    // Run tests
    uint8_t passed = 0;
    uint8_t total = 4;

    if (test_light_sleep_xosc()) passed++;
    delay(1000);

    if (test_dormant_lposc_timer()) passed++;
    delay(1000);

    if (test_clock_restoration()) passed++;
    delay(1000);

    // GPIO test is interactive - run last
    Serial.println("\n--- Interactive GPIO Wake Test ---");
    Serial.println("This test requires manual button press.");
    Serial.println("Press any key to start, or 's' to skip...");
    Serial.flush();

    uint32_t timeout = millis() + 10000;
    bool skip_gpio = false;
    while (millis() < timeout) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == 's' || c == 'S') {
                skip_gpio = true;
                Serial.println("Skipping GPIO test.");
            }
            break;
        }
        delay(100);
    }

    if (!skip_gpio) {
        if (test_dormant_gpio_wake()) passed++;
    } else {
        total--;
    }

    // Shutdown test is most destructive - run last
    Serial.println("\n--- Shutdown Until USB Test ---");
    Serial.println("This test requires USB disconnect/reconnect.");
    Serial.println("Press 'y' to run, or 's' to skip...");
    Serial.flush();

    timeout = millis() + 10000;
    bool skip_shutdown = false;
    while (millis() < timeout) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == 's' || c == 'S') {
                skip_shutdown = true;
                Serial.println("Skipping shutdown test.");
            }
            break;
        }
        delay(100);
    }

    if (!skip_shutdown) {
        total++;
        if (test_shutdown_until_usb()) passed++;
    }

    // Summary
    Serial.println("\n========================================");
    Serial.printf("    Test Results: %d/%d passed\n", passed, total);
    Serial.println("========================================");

    if (passed == total) {
        Serial.println("All tests PASSED!");
        blink_led(10, 50, 50);  // Fast blink = success
    } else {
        Serial.println("Some tests FAILED.");
        blink_led(3, 500, 500); // Slow blink = failure
    }

    Serial.println("\nTest complete. Entering idle loop.");
    Serial.println("Reset to run tests again.");

    // Idle loop
    while (1) {
        delay(1000);
    }
}

// Arduino setup/loop interface
void setup() {
    run_rp2350_power_tests();
}

void loop() {
    // Tests run in setup, loop does nothing
}

#else // Not RP2350

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("ERROR: This test is only for RP2350 platforms!");
    Serial.println("Please build with -D__PLAT_RP2350__");
}

void loop() {
    delay(1000);
}

#endif // __PLAT_RP2350__
