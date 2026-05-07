/**
 ******************************************************************************
 * @file    ipwm.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_PWM
 * @brief   PWM output interface.
 *
 * @details
 *   - Per-channel start and stop of PWM generation
 *   - Duty cycle control in permille (0–1000) for 0.1 % resolution
 *   - Output frequency control shared across all channels of the same timer
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Frequency changes affect all channels bound to the same hardware timer.
 *   Methods are **not** thread-safe on the same instance; caller must serialize access.
 */

#ifndef HELIOS_DRV_IPWM_HPP_
#define HELIOS_DRV_IPWM_HPP_

#include <hel_target>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @class  iPwm
 * @brief  Hardware-agnostic PWM output interface.
 * @ingroup HELIOS_DRV_PWM
 *
 * @details
 *   - Per-channel start/stop via @ref start and @ref stop
 *   - Duty cycle set via @ref setDutyCycle in permille (0 = 0 %, 1000 = 100 %)
 *   - Output frequency set via @ref setFrequency (shared by all channels of the timer)
 *   - Duty and frequency changes take effect on the next timer period
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Copy and move are deleted; PWM peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iPwm
{
public:
  virtual ~iPwm() noexcept = default;

  // -------------------------------------------------------------------------
  // Channel control
  // -------------------------------------------------------------------------

  /**
   * @brief     Start PWM generation on the specified channel.
   * @param[in] channel  Zero-based channel index (implementation-defined range).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Channel started successfully.
   *   - @ref ReturnCode::OperationRunning : Channel is already running; no change made.
   *   - @ref ReturnCode::ErrorParam      : @p channel index is out of range.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; channel not started.
   */
  [[nodiscard]] virtual ReturnCode start(uint32_t channel) noexcept = 0;

  /**
   * @brief     Stop PWM generation on the specified channel.
   * @details   The output pin is driven to its idle level after the current period ends.
   * @param[in] channel  Zero-based channel index (implementation-defined range).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Channel stopped successfully.
   *   - @ref ReturnCode::ErrorInvalidState : Channel is not running; no change made.
   *   - @ref ReturnCode::ErrorParam      : @p channel index is out of range.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault.
   */
  [[nodiscard]] virtual ReturnCode stop(uint32_t channel) noexcept = 0;

  // -------------------------------------------------------------------------
  // Duty cycle
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the duty cycle for the specified channel.
   * @details   The new duty cycle takes effect on the next timer period. The value is
   *            expressed in permille: 0 = 0 % (always low), 1000 = 100 % (always high).
   * @param[in] channel       Zero-based channel index (implementation-defined range).
   * @param[in] duty_permille Duty cycle in permille [0, 1000].
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Duty cycle updated.
   *   - @ref ReturnCode::ErrorParam      : @p channel out of range or
   *                                        @p duty_permille > 1000.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; previous duty cycle unchanged.
   */
  [[nodiscard]] virtual ReturnCode setDutyCycle(
    uint32_t channel, uint16_t duty_permille) noexcept = 0;

  // -------------------------------------------------------------------------
  // Frequency
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the PWM output frequency for all channels of this timer.
   * @details   The new frequency takes effect on the next timer period. All channels
   *            bound to the same hardware timer are affected.
   * @param[in] frequency_hz  Desired frequency in Hz (> 0; implementation-defined maximum).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Frequency updated.
   *   - @ref ReturnCode::ErrorParam      : @p frequency_hz is 0 or exceeds the supported range.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; previous frequency unchanged.
   * @warning   Affects all channels of the underlying hardware timer simultaneously.
   */
  [[nodiscard]] virtual ReturnCode setFrequency(uint32_t frequency_hz) noexcept = 0;

protected:
  iPwm& operator=(const iPwm&) = delete;
  iPwm& operator=(iPwm&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_IPWM_HPP_
