/**
 ******************************************************************************
 * @file    irtc.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_RTC
 * @brief   Real-time clock interface.
 *
 * @details
 *   - Get and set the current date and time via @ref Calendar
 *   - Single alarm with event notification via @ref RtcCallback
 *   - Alarm can be cleared at any time
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   The RTC peripheral is typically backed by a dedicated clock domain (LSE/LSI)
 *   and retains time across system resets when a backup supply is present.
 *   Methods are **not** thread-safe on the same instance; caller must serialize access.
 *   The alarm callback fires from ISR context (implementation-defined).
 */

#ifndef HELIOS_DRV_IRTC_HPP_
#define HELIOS_DRV_IRTC_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <ctime>
#include <cstdint>

namespace hel
{

/**
 * @brief Callable type used to receive RTC alarm events.
 *
 * @details Signature: `void handler(ReturnCode code) noexcept`
 *   - @p code  — @ref ReturnCode::AnsweredRequest when the alarm fired normally,
 *                error code on peripheral fault.
 */
using RtcCallback = Callback<void()>;


/**
 * @class  iRtc
 * @brief  Hardware-agnostic real-time clock interface.
 * @ingroup HELIOS_DRV_RTC
 *
 * @details
 *   - Current time read/write via @ref getTime and @ref setTime using @ref Calendar
 *   - Single alarm configured via @ref setAlarm; cleared via @ref clearAlarm
 *   - Alarm event delivered through the callback registered with @ref setCallback
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Copy and move are deleted; the RTC peripheral is a singleton owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iRtc
{
public:

  // -------------------------------------------------------------------------
  // Time access
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the current date and time from the RTC.
   * @param[out] datetime  Receives the current date and time on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p datetime contains valid current time.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p datetime is unspecified.
   */
  [[nodiscard]]
  virtual ReturnCode getTime(struct tm &datetime) noexcept = 0;

  /**
   * @brief     Set the current date and time on the RTC.
   * @param[in] datetime  Date and time to apply.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Time set successfully.
   *   - @ref ReturnCode::ErrorParam      : @p datetime contains an out-of-range field.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; previous time unchanged.
   */
  [[nodiscard]]
  virtual ReturnCode setTime(const struct tm &datetime) noexcept = 0;

  // -------------------------------------------------------------------------
  // Alarm
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the alarm to fire at the specified date and time.
   * @details   The registered @ref RtcCallback is invoked when the RTC time matches
   *            @p alarm. Any previously armed alarm is replaced.
   * @param[in] alarm  Date and time at which the alarm should fire.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Alarm armed successfully.
   *   - @ref ReturnCode::ErrorParam      : @p alarm contains an out-of-range field.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; alarm not set.
   */
  [[nodiscard]] virtual ReturnCode setAlarm(const struct tm &datetime) noexcept = 0;

  /**
   * @brief  Disarm the current alarm.
   * @details No callback is fired. If no alarm is armed, the call is a no-op.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Alarm cleared (or was already inactive).
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; alarm state is unspecified.
   */
  [[nodiscard]] virtual ReturnCode clearAlarm() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the alarm event callback.
   * @param[in] callback  Callable invoked when the alarm fires (@ref RtcCallback).
   *                      Pass a default-constructed @ref RtcCallback to clear.
   * @note  Safe to call at any time; takes effect for the next alarm event.
   */
  virtual void setCallback(RtcCallback callback) noexcept = 0;

protected:
  iRtc& operator=(const iRtc&) = delete;
  iRtc& operator=(iRtc&&) = delete;

  /**
   * @brief     Internal handler for RTC alarm events, called by the driver/ISR.
   * @param[in] code  Result code for the event that occurred.
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleAlarm() = 0;
};

} // namespace hel

#endif // HELIOS_DRV_IRTC_HPP_
