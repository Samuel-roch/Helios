/**
 ******************************************************************************
 * @file    iwdg.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_WDG
 * @brief   Watchdog timer interface.
 *
 * @details
 *   - Start the watchdog with a configurable timeout
 *   - Periodic refresh (kick) to prevent a system reset
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   On most MCUs the independent watchdog (IWDG) cannot be stopped once started.
 *   Failing to call @ref refresh within the configured timeout triggers a system reset.
 *   @ref start must be called before @ref refresh.
 */

#ifndef HELIOS_DRV_IWDG_HPP_
#define HELIOS_DRV_IWDG_HPP_

#include <hel_target>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @class  iWdg
 * @brief  Hardware-agnostic watchdog timer interface.
 * @ingroup HELIOS_DRV_WDG
 *
 * @details
 *   - Single-call start via @ref start with a millisecond timeout
 *   - Periodic counter reset via @ref refresh to prevent system reset
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: @ref refresh may be called from any context (task or ISR),
 *        but @ref start must be called exactly once from task context during initialization.
 *        Copy and move are deleted; the watchdog peripheral is a singleton owned by the
 *        BSP layer and must not be duplicated or relocated.
 * @warning Once started, the watchdog cannot be stopped on most targets.
 *          Ensure @ref refresh is called reliably within every @p timeout_ms window.
 */
class iWdg
{
public:
  virtual ~iWdg() noexcept = default;

  // -------------------------------------------------------------------------
  // Watchdog control
  // -------------------------------------------------------------------------

  /**
   * @brief     Initialize and start the watchdog with the specified timeout.
   * @details   The watchdog begins counting immediately after this call returns.
   *            On most targets this call is irreversible; the watchdog cannot be stopped.
   * @param[in] timeout_ms  Watchdog timeout in milliseconds (implementation-defined range).
   *                        If @ref refresh is not called within this window, the system resets.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Watchdog started; @ref refresh must now be called
   *                                        within every @p timeout_ms window.
   *   - @ref ReturnCode::OperationRunning : Watchdog is already running; no change made.
   *   - @ref ReturnCode::ErrorParam      : @p timeout_ms is 0 or exceeds the supported range.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; watchdog not started.
   */
  [[nodiscard]]
  virtual ReturnCode start(uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief  Refresh (kick) the watchdog counter to prevent a system reset.
   * @details Resets the internal countdown to the value configured in @ref start.
   *          Must be called at least once within every configured timeout window.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Counter refreshed; timeout window restarted.
   *   - @ref ReturnCode::NotInitialized  : @ref start has not been called; nothing done.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault.
   * @note  IRQ-safe: may be called from task or ISR context.
   */
  [[nodiscard]]
  virtual ReturnCode refresh() noexcept = 0;

protected:
  iWdg& operator=(const iWdg&) = delete;
  iWdg& operator=(iWdg&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_IWDG_HPP_
