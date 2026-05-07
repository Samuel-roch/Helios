/**
 ******************************************************************************
 * @file    itimer.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_DRV_TIMER
 * @brief   Hardware-agnostic timing interface.
 *
 ******************************************************************************
 */

#ifndef HELIOS_DRV_ITIMER_HPP_
#define HELIOS_DRV_ITIMER_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @enum  TimerMode
 * @brief Controls whether the timer restarts automatically after expiry.
 * @ingroup HELIOS_DRV_TIMER
 */
enum class TimerMode : uint8_t
{
  OneShot = 0x00U, /*!< Counter stops and callback fires once on expiry. */
  Periodic = 0x01U, /*!< Counter reloads and callback fires every period. */
};

/**
 * @brief Callable type invoked when the timer period elapses.
 *
 * @details Signature: `void handler() noexcept`
 *   Called from ISR (or driver-task) context on each period expiry.
 *   In @ref TimerMode::OneShot the timer is already stopped when the
 *   callback fires; in @ref TimerMode::Periodic it has already reloaded.
 */
using TimerCallback = Callback<void()>;

/**
 * @class  iTimer
 * @brief  Hardware-agnostic timer interface for time measurement and
 *         periodic/one-shot event generation.
 * @ingroup HELIOS_DRV_TIMER
 *
 * @details Provides microsecond-resolution timing through a hardware
 *          peripheral.  Implementations map each virtual method to the
 *          target HAL (STM32, ESP32, …).
 *
 *          Helpers @ref count and @ref isActive may be called freely;
 *          @ref start, @ref stop, and @ref reset must not be called
 *          from an ISR context.
 *
 * @note  Copy and move are deleted; timer peripherals are singletons
 *        owned by the BSP layer and must not be duplicated or relocated.
 */
class iTimer
{
public:
  virtual ~iTimer() noexcept = default;

  // -------------------------------------------------------------------------
  // Timer control
  // -------------------------------------------------------------------------

  /**
   * @brief  Start (or restart) the timer with the given period.
   * @details If the timer is already running it is restarted from zero with
   *          the new period and mode.  The callback (if set) fires after
   *          @p period_us microseconds and, in @ref TimerMode::Periodic,
   *          every @p period_us thereafter.
   *
   * @param[in]  period_us  Period in microseconds.  Must be > 0.
   * @param[in]  mode       @ref TimerMode::OneShot or @ref TimerMode::Periodic.
   * @return @ref ReturnCode::AnsweredRequest on success.
   * @return @ref ReturnCode::ErrorGeneral on hardware fault.
   */
  [[nodiscard]] virtual ReturnCode start(
    uint32_t period_us, TimerMode mode) noexcept = 0;

  /**
   * @brief  Stop the timer immediately.
   * @details The counter is frozen at its current value.  No callback fires
   *          as a result of this call.
   *
   * @return @ref ReturnCode::AnsweredRequest if the timer was running and was stopped.
   * @return @ref ReturnCode::NotInitialized if the timer was not running.
   */
  [[nodiscard]] virtual ReturnCode stop() noexcept = 0;

  /**
   * @brief  Reset the counter to zero without stopping the timer.
   * @details In @ref TimerMode::Periodic the next callback fires after a
   *          full period from this call.  Has no effect on the registered
   *          callback or mode.
   *
   * @return @ref ReturnCode::AnsweredRequest on success.
   * @return @ref ReturnCode::NotInitialized if the timer is not running.
   */
  [[nodiscard]] virtual ReturnCode reset() noexcept = 0;

  // -------------------------------------------------------------------------
  // Measurement
  // -------------------------------------------------------------------------

  /**
   * @brief  Return `true` when the timer is currently counting.
   * @details A @ref TimerMode::OneShot timer becomes inactive automatically
   *          after the first expiry.
   *
   * @return `true` if the timer is running.
   */
  [[nodiscard]] virtual bool isActive() const noexcept = 0;

  /**
   * @brief  Elapsed time since the last @ref start or @ref reset, in microseconds.
   * @details Returns 0 when not active.  In @ref TimerMode::Periodic the
   *          value wraps to zero on each expiry.
   *
   * @return Elapsed time in microseconds.
   */
  [[nodiscard]] virtual uint64_t count() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief  Register (or replace) the period-elapsed callback.
   * @details Pass a default-constructed @ref TimerCallback to clear.
   *          The callback is invoked from ISR or driver-task context.
   *
   * @param[in]  callback  Callable invoked on each timer expiry.
   */
  virtual void setCallback(TimerCallback callback) noexcept = 0;

  /**
   * @brief  Internal handler for timer expiry events, called by the ISR bridge.
   * @details Implementations must invoke the registered callback and, for
   *          @ref TimerMode::OneShot, mark the timer as inactive.
   */
  virtual void handleEvent() = 0;

protected:
  iTimer& operator=(const iTimer&) = delete;
  iTimer& operator=(iTimer&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_ITIMER_HPP_
