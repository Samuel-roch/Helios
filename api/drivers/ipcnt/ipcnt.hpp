/**
 ******************************************************************************
 * @file    ipcnt.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_PCNT
 * @brief   Hardware pulse counter interface.
 *
 * @details
 *   - Start and stop pulse counting on a hardware timer/counter
 *   - Non-blocking count read and explicit counter reset
 *   - Configurable threshold with event notification via @ref PcntCallback
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Methods are **not** thread-safe on the same instance; caller must serialize access.
 *   The threshold callback fires from ISR context (implementation-defined).
 */

#ifndef HELIOS_DRV_IPCNT_HPP_
#define HELIOS_DRV_IPCNT_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @brief Callable type used to receive pulse counter events.
 *
 * @details Signature: `void handler(ReturnCode code, uint32_t count) noexcept`
 *   - @p code   — @ref ReturnCode::AnsweredRequest on a normal threshold/overflow event,
 *                 error code on peripheral fault.
 *   - @p count  — counter value captured at the moment the event fired.
 */
using PcntCallback = Callback<void(ReturnCode, uint32_t)>;

/**
 * @class  iPcnt
 * @brief  Hardware-agnostic pulse counter interface.
 * @ingroup HELIOS_DRV_PCNT
 *
 * @details
 *   - Start/stop counting via @ref start and @ref stop
 *   - Non-blocking count read via @ref read
 *   - Counter reset via @ref reset (safe to call while running)
 *   - Optional threshold notification via @ref setThreshold and @ref setCallback;
 *     callback fires when the counter reaches or exceeds the threshold, and on overflow
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Copy and move are deleted; counter peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iPcnt
{
public:
  virtual ~iPcnt() noexcept = default;

  // -------------------------------------------------------------------------
  // Counter control
  // -------------------------------------------------------------------------

  /**
   * @brief  Start pulse counting.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Counter started successfully.
   *   - @ref ReturnCode::OperationRunning : Counter is already running; no change made.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; counter not started.
   */
  [[nodiscard]] virtual ReturnCode start() noexcept = 0;

  /**
   * @brief  Stop pulse counting.
   * @details The current count is preserved; call @ref reset to clear it.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Counter stopped successfully.
   *   - @ref ReturnCode::ErrorInvalidState : Counter is not running; no change made.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault.
   */
  [[nodiscard]] virtual ReturnCode stop() noexcept = 0;

  /**
   * @brief  Reset the counter value to zero.
   * @note   Safe to call while the counter is running; the reset is atomic.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Counter reset to zero.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; counter value unchanged.
   */
  [[nodiscard]] virtual ReturnCode reset() noexcept = 0;

  // -------------------------------------------------------------------------
  // Counter read
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the current counter value.
   * @param[out] count  Current pulse count written on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p count is valid.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p count is unspecified.
   */
  [[nodiscard]] virtual ReturnCode read(uint32_t& count) noexcept = 0;

  // -------------------------------------------------------------------------
  // Threshold configuration
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the count threshold that triggers the registered callback.
   * @details   The callback fires when the counter reaches or exceeds @p threshold,
   *            and again on every subsequent overflow until @ref stop is called.
   *            Pass 0 to disable threshold events.
   * @param[in] threshold  Count value at which the event fires (0 = disabled).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Threshold applied successfully.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; previous threshold unchanged.
   */
  [[nodiscard]] virtual ReturnCode setThreshold(uint32_t threshold) noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the threshold/overflow event callback.
   * @param[in] callback  Callable invoked on threshold or overflow events (@ref PcntCallback).
   *                      Pass a default-constructed @ref PcntCallback to clear.
   * @note  Safe to call at any time; takes effect immediately.
   */
  virtual void setCallback(PcntCallback callback) noexcept = 0;

protected:
  iPcnt& operator=(const iPcnt&) = delete;
  iPcnt& operator=(iPcnt&&) = delete;

  /**
   * @brief     Internal handler for pulse counter events, called by the driver/ISR.
   * @param[in] code   Result code for the event that occurred.
   * @param[in] count  Counter value captured at the moment the event fired.
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleEvent(ReturnCode code, uint32_t count) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_IPCNT_HPP_
