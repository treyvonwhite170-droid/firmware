/**
 * @file BaseSleepManager.h
 * @brief Base class for sleep manager implementations.
 *
 * Provides common functionality for platform-specific sleep managers
 * including preflight checks, observer notification, and state tracking.
 *
 * NASA Safety-Critical Coding Compliance:
 * - Rule 1: Simple state machine for sleep states
 * - Rule 3: No dynamic allocation after initialization
 * - Rule 4: Functions under 60 lines
 * - Rule 6: Minimal variable scope
 *
 * @copyright Copyright (c) Meshtastic Project
 * @license GPL-3.0
 */
#pragma once

#include "../ISleepManager.h"
#include "../../Observer.h"

namespace meshtastic {
namespace power {

/**
 * @brief Maximum time to wait for preflight checks (ms).
 */
static constexpr uint32_t PREFLIGHT_TIMEOUT_MS = 30000;

/**
 * @brief Preflight check interval (ms).
 */
static constexpr uint32_t PREFLIGHT_CHECK_INTERVAL_MS = 100;

/**
 * @brief Base class for sleep manager implementations.
 *
 * Provides common preflight and observer notification logic.
 * Platform-specific implementations override sleep methods.
 */
class BaseSleepManager : public ISleepManager {
  public:
    BaseSleepManager();
    virtual ~BaseSleepManager() = default;

    // Common ISleepManager implementations
    SleepResult initialize() override;
    bool doPreflightCheck() override;
    WakeCause getLastWakeCause() const override { return last_wake_cause_; }
    int32_t getBootCount() const override { return boot_count_; }

    /**
     * @brief Get the preflight sleep observable.
     *
     * Observers can register to veto sleep requests.
     *
     * @return Reference to preflight observable
     */
    Observable<void *> &getPreflightObservable() { return preflight_sleep_; }

    /**
     * @brief Get the deep sleep notification observable.
     *
     * Observers are notified before entering deep sleep.
     *
     * @return Reference to deep sleep observable
     */
    Observable<void *> &getDeepSleepObservable() { return notify_deep_sleep_; }

    /**
     * @brief Get the reboot notification observable.
     *
     * Observers are notified before system reboot.
     *
     * @return Reference to reboot observable
     */
    Observable<void *> &getRebootObservable() { return notify_reboot_; }

  protected:
    /**
     * @brief Set the last wake cause.
     *
     * @param cause Wake cause to store
     */
    void setLastWakeCause(WakeCause cause) { last_wake_cause_ = cause; }

    /**
     * @brief Increment boot count.
     *
     * Called on deep sleep wake.
     */
    void incrementBootCount() { boot_count_++; }

    /**
     * @brief Wait for preflight checks to complete.
     *
     * Polls observers until all allow sleep or timeout.
     *
     * @param skip_preflight Skip preflight if true
     * @return true if sleep is allowed
     */
    bool waitForPreflight(bool skip_preflight);

    /**
     * @brief Notify observers of impending deep sleep.
     */
    void notifyDeepSleepObservers();

    /**
     * @brief Flush console output before sleep.
     */
    void flushConsole();

    /**
     * @brief Disable Bluetooth before sleep.
     */
    void disableBluetooth();

    /**
     * @brief Get platform time in milliseconds.
     *
     * @return Current time in ms
     */
    virtual uint32_t getTimeMs() const;

  private:
    Observable<void *> preflight_sleep_;     ///< Preflight veto observable
    Observable<void *> notify_deep_sleep_;   ///< Deep sleep notification
    Observable<void *> notify_reboot_;       ///< Reboot notification
    WakeCause last_wake_cause_;              ///< Most recent wake cause
    int32_t boot_count_;                     ///< Boot counter (RTC preserved)
    bool initialized_;                        ///< Initialization state
};

} // namespace power
} // namespace meshtastic
