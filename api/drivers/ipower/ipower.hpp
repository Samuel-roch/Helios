/**
 ******************************************************************************
 * @file    ipower.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_POWER
 * @brief   Power management interface.
 *
 * @details
 *   - Controlled entry into low-power sleep modes via @ref SleepMode
 *   - Software-initiated system reset
 *   - Query of the last reset source via @ref ResetSource
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   @ref enterSleep does not return when @ref SleepMode::Standby or
 *   @ref SleepMode::Shutdown are used; only a hardware wakeup event resumes execution.
 *   @ref reset never returns.
 */

#ifndef HELIOS_DRV_IPOWER_HPP_
#define HELIOS_DRV_IPOWER_HPP_

#include <hel_target>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @enum  SleepMode
 * @brief Low-power mode to enter via @ref iPower::enterSleep.
 * @ingroup HELIOS_DRV_POWER
 */
enum class SleepMode : uint8_t
{
  Sleep    = 0x00U, /*!< CPU halted; peripherals and SRAM remain powered.                    */
  Stop     = 0x01U, /*!< CPU and most clocks halted; SRAM and register state retained.        */
  Standby  = 0x02U, /*!< All clocks off except RTC/IWDG; SRAM content may be lost.           */
  Shutdown = 0x03U  /*!< Lowest power; all domains off; requires external wakeup pin.         */
};

/**
 * @enum  ResetSource
 * @brief Identifies the cause of the last system reset.
 * @ingroup HELIOS_DRV_POWER
 */
enum class ResetSource : uint8_t
{
  PowerOn   = 0x00U, /*!< Power-on or power-down reset (POR/PDR).                            */
  External  = 0x01U, /*!< External reset via NRST pin.                                       */
  Software  = 0x02U, /*!< Software-initiated reset (e.g. NVIC SystemReset).                  */
  Watchdog  = 0x03U, /*!< Independent or window watchdog timeout.                            */
  BrownOut  = 0x04U, /*!< Brown-out reset (supply voltage dropped below threshold).          */
  Unknown   = 0xFFU  /*!< Reset source could not be determined.                              */
};

/**
 * @class  iPower
 * @brief  Hardware-agnostic power management interface.
 * @ingroup HELIOS_DRV_POWER
 *
 * @details
 *   - Controlled low-power entry via @ref enterSleep with @ref SleepMode selection
 *   - Software system reset via @ref reset (never returns)
 *   - Last reset source query via @ref getResetSource
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access.
 *        @ref enterSleep with @ref SleepMode::Standby or @ref SleepMode::Shutdown is a
 *        point of no return; execution resumes only after a hardware wakeup event.
 *        Copy and move are deleted; the power manager is a singleton owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iPower
{
public:
  virtual ~iPower() noexcept = default;

  // -------------------------------------------------------------------------
  // Sleep
  // -------------------------------------------------------------------------

  /**
   * @brief     Enter a low-power mode.
   * @details   The call blocks until a wakeup event occurs for @ref SleepMode::Sleep
   *            and @ref SleepMode::Stop. For @ref SleepMode::Standby and
   *            @ref SleepMode::Shutdown this function does **not** return; the MCU
   *            restarts from reset after the wakeup event.
   * @param[in] mode  Desired low-power mode (see @ref SleepMode).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Woke from Sleep or Stop; execution continues.
   *   - @ref ReturnCode::ErrorGeneral   : Entry failed; system remains in run mode.
   * @warning   For Standby and Shutdown, this function never returns.
   *            Ensure all critical data is persisted before calling.
   */
  [[nodiscard]] virtual ReturnCode enterSleep(SleepMode mode) noexcept = 0;

  // -------------------------------------------------------------------------
  // Reset
  // -------------------------------------------------------------------------

  /**
   * @brief  Perform a software system reset.
   * @details Triggers an immediate system reset; this function never returns.
   *          All peripherals and registers are reset to their power-on state.
   * @warning This function never returns. Ensure all critical data is persisted
   *          and all peripherals are in a safe state before calling.
   */
  [[noreturn]] virtual void reset() noexcept = 0;

  // -------------------------------------------------------------------------
  // Reset source
  // -------------------------------------------------------------------------

  /**
   * @brief      Query the source of the last system reset.
   * @param[out] source  Reset source written on success (see @ref ResetSource).
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p source contains a valid value.
   *   - @ref ReturnCode::ErrorGeneral   : Source could not be determined; @p source is
   *                                       set to @ref ResetSource::Unknown.
   */
  [[nodiscard]] virtual ReturnCode getResetSource(ResetSource& source) noexcept = 0;

protected:
  iPower& operator=(const iPower&) = delete;
  iPower& operator=(iPower&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_IPOWER_HPP_
