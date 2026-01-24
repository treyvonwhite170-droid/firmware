/**
 * @file test_main.cpp
 * @brief Unit tests for power management abstractions.
 *
 * Tests the BatteryChemistry SoC calculations and other
 * power management functionality.
 */

#include "TestUtil.h"
#include <unity.h>
#include <utility>

// Include the power management headers
#include "power/IBatteryChemistry.h"

using namespace meshtastic::power;

void setUp(void)
{
    // Set up before each test
}

void tearDown(void)
{
    // Clean up after each test
}

// ============================================================================
// BatteryChemistry Tests
// ============================================================================

void test_BatteryChemistry_DefaultConstruction(void)
{
    BatteryChemistry chem;
    TEST_ASSERT_EQUAL(BatteryType::LION, chem.getType());
    TEST_ASSERT_EQUAL_STRING("LiIon", chem.getName());
}

void test_BatteryChemistry_LiIonSoC_FullBattery(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Full battery at 4.19V should be ~100%
    int8_t soc = chem.calculateSoC(4190);
    TEST_ASSERT_GREATER_OR_EQUAL(95, soc);
    TEST_ASSERT_LESS_OR_EQUAL(100, soc);
}

void test_BatteryChemistry_LiIonSoC_EmptyBattery(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Empty battery at 3.1V should be ~0%
    int8_t soc = chem.calculateSoC(3100);
    TEST_ASSERT_GREATER_OR_EQUAL(0, soc);
    TEST_ASSERT_LESS_OR_EQUAL(5, soc);
}

void test_BatteryChemistry_LiIonSoC_MidBattery(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Mid-range voltage around 3.7V should be ~50%
    int8_t soc = chem.calculateSoC(3720);
    TEST_ASSERT_GREATER_OR_EQUAL(40, soc);
    TEST_ASSERT_LESS_OR_EQUAL(60, soc);
}

void test_BatteryChemistry_LiIonSoC_BelowEmpty(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Below empty threshold should return 0%
    int8_t soc = chem.calculateSoC(2800);
    TEST_ASSERT_EQUAL(0, soc);
}

void test_BatteryChemistry_LiIonSoC_AboveFull(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Above full should clamp to 100%
    int8_t soc = chem.calculateSoC(4500);
    TEST_ASSERT_EQUAL(100, soc);
}

void test_BatteryChemistry_TwoCells(void)
{
    BatteryChemistry chem(BatteryType::LION, 2);

    // 2-cell pack at 8.38V (4.19V per cell) should be ~100%
    int8_t soc = chem.calculateSoC(8380);
    TEST_ASSERT_GREATER_OR_EQUAL(95, soc);
    TEST_ASSERT_LESS_OR_EQUAL(100, soc);

    // 2-cell pack at 6.2V (3.1V per cell) should be ~0%
    soc = chem.calculateSoC(6200);
    TEST_ASSERT_GREATER_OR_EQUAL(0, soc);
    TEST_ASSERT_LESS_OR_EQUAL(5, soc);
}

void test_BatteryChemistry_LiFePO4(void)
{
    BatteryChemistry chem(BatteryType::LIFEPO4, 1);

    TEST_ASSERT_EQUAL(BatteryType::LIFEPO4, chem.getType());
    TEST_ASSERT_EQUAL_STRING("LiFePO4", chem.getName());

    // LiFePO4 full at 3.6V
    int8_t soc = chem.calculateSoC(3600);
    TEST_ASSERT_GREATER_OR_EQUAL(95, soc);
}

void test_BatteryChemistry_ChargingVoltage(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Above charging threshold
    TEST_ASSERT_TRUE(chem.isChargingVoltage(4300));

    // Below charging threshold
    TEST_ASSERT_FALSE(chem.isChargingVoltage(4100));
}

void test_BatteryChemistry_EmptyVoltage(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Below empty threshold
    TEST_ASSERT_TRUE(chem.isEmptyVoltage(3000));

    // Above empty threshold
    TEST_ASSERT_FALSE(chem.isEmptyVoltage(3500));
}

void test_BatteryChemistry_NoBatteryVoltage(void)
{
    BatteryChemistry chem(BatteryType::LION, 1);

    // Very low voltage indicates no battery
    TEST_ASSERT_TRUE(chem.isNoBatteryVoltage(2000));

    // Normal voltage indicates battery present
    TEST_ASSERT_FALSE(chem.isNoBatteryVoltage(3500));
}

void test_BatteryChemistry_CopyAssignment(void)
{
    BatteryChemistry chem1(BatteryType::LION, 1);
    BatteryChemistry chem2(BatteryType::LIFEPO4, 2);

    // Assign chem1 to chem2
    chem2 = chem1;

    TEST_ASSERT_EQUAL(BatteryType::LION, chem2.getType());
    TEST_ASSERT_EQUAL_STRING("LiIon", chem2.getName());
}

void test_BatteryChemistry_MoveAssignment(void)
{
    BatteryChemistry chem1(BatteryType::LION, 1);
    BatteryChemistry chem2(BatteryType::LIFEPO4, 2);

    // Move chem1 to chem2
    chem2 = std::move(chem1);

    TEST_ASSERT_EQUAL(BatteryType::LION, chem2.getType());
}

void test_BatteryChemistry_CustomConfig(void)
{
    BatteryChemistryConfig config;
    config.type = BatteryType::CUSTOM;
    config.num_cells = 1;
    config.name = "CustomBattery";
    config.ocv_mv[0] = 4200;  // 100%
    config.ocv_mv[10] = 3000; // 0%
    config.charging_threshold_mv = 4250;
    config.empty_threshold_mv = 3000;

    BatteryChemistry chem(config);

    TEST_ASSERT_EQUAL(BatteryType::CUSTOM, chem.getType());
    TEST_ASSERT_EQUAL_STRING("CustomBattery", chem.getName());
}

// ============================================================================
// Test Runner
// ============================================================================

void setup()
{
    initializeTestEnvironment();
    UNITY_BEGIN();

    // BatteryChemistry tests
    RUN_TEST(test_BatteryChemistry_DefaultConstruction);
    RUN_TEST(test_BatteryChemistry_LiIonSoC_FullBattery);
    RUN_TEST(test_BatteryChemistry_LiIonSoC_EmptyBattery);
    RUN_TEST(test_BatteryChemistry_LiIonSoC_MidBattery);
    RUN_TEST(test_BatteryChemistry_LiIonSoC_BelowEmpty);
    RUN_TEST(test_BatteryChemistry_LiIonSoC_AboveFull);
    RUN_TEST(test_BatteryChemistry_TwoCells);
    RUN_TEST(test_BatteryChemistry_LiFePO4);
    RUN_TEST(test_BatteryChemistry_ChargingVoltage);
    RUN_TEST(test_BatteryChemistry_EmptyVoltage);
    RUN_TEST(test_BatteryChemistry_NoBatteryVoltage);
    RUN_TEST(test_BatteryChemistry_CopyAssignment);
    RUN_TEST(test_BatteryChemistry_MoveAssignment);
    RUN_TEST(test_BatteryChemistry_CustomConfig);

    exit(UNITY_END());
}

void loop() {}
