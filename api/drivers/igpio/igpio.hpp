/**
 ******************************************************************************
 * @file    igpio.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-24
 * @ingroup HELIOS_DRV_GPIO
 * @brief   General-purpose digital I/O interface.
 *
 * @details
 *   - Digital read and write on a single pin
 *   - Toggle output level in a single call
 *   - Interrupt arm via @ref enableInterrupt, combining trigger mode and callback
 *     registration in a single call
 *   - Event notification via @ref GpioCallback on interrupt; the callback receives
 *     the edge that actually fired (@ref GpioIrqMode)
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Methods are **not** thread-safe on the same instance; caller must serialize access.
 *   The interrupt callback fires from ISR context (implementation-defined).
 */

#ifndef HELIOS_DRV_IGPIO_HPP_
#define HELIOS_DRV_IGPIO_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// =============================================================================
// GpioIrqMode
// =============================================================================

/**
 * @enum  GpioIrqMode
 * @brief Selects which edge(s) trigger the EXTI interrupt on a GPIO pin.
 * @ingroup HELIOS_DRV_GPIO
 */
enum class GpioIrqMode : uint8_t
{
  Disabled = 0U, /*!< Interrupt disabled; no edge generates an event.           */
  Rising   = 1U, /*!< Trigger on the low-to-high transition (rising edge).      */
  Falling  = 2U, /*!< Trigger on the high-to-low transition (falling edge).     */
  Change   = 3U  /*!< Trigger on both rising and falling edges.                 */
};

// =============================================================================
// GpioCallback
// =============================================================================

/**
 * @brief Callable type invoked when a GPIO interrupt fires.
 *
 * @details Signature: `void handler(GpioIrqMode cause) noexcept`
 *   - @p cause  — @ref GpioIrqMode::Rising  if a rising  edge triggered the ISR.
 *   - @p cause  — @ref GpioIrqMode::Falling if a falling edge triggered the ISR.
 *   - @p cause  — @ref GpioIrqMode::Change  when the implementation cannot
 *                 distinguish which edge fired (e.g. single-callback HAL families).
 *
 * @note  The callback is invoked from ISR context; it must not block or call
 *        any function that disables interrupts for an extended period.
 */
using GpioCallback = Callback<void(GpioIrqMode)>;

// =============================================================================
// iGpio
// =============================================================================

/**
 * @class  iGpio
 * @brief  Hardware-agnostic general-purpose digital I/O interface.
 * @ingroup HELIOS_DRV_GPIO
 *
 * @details
 *   - Digital read (@ref read) and write (@ref write) on a single pin.
 *   - Output toggle via @ref toggle.
 *   - Interrupt armed via @ref enableInterrupt, which sets the trigger mode and
 *     registers the @ref GpioCallback in one call.
 *   - Implementations shall map each virtual method to the target HAL.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   the caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; GPIO pins are singletons owned by the BSP layer
 *   and must not be duplicated or relocated.
 */
class iGpio
{
public:
  virtual ~iGpio() noexcept = default;

  // -------------------------------------------------------------------------
  // Digital I/O
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the current logical level of the pin.
   * @param[out] state  Set to `true` if the pin is high, `false` if low.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Read successful; @p state is valid.
   *   - @ref ReturnCode::NotInitialized   : Pin object is in an invalid state.
   *   - @ref ReturnCode::ErrorGeneral     : Peripheral fault; @p state is unspecified.
   */
  virtual ReturnCode read(bool& state) const noexcept = 0;

  /**
   * @brief     Drive the pin to the requested logical level.
   * @param[in] state  `true` drives the pin high, `false` drives it low.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Pin level set successfully.
   *   - @ref ReturnCode::NotInitialized   : Pin object is in an invalid state.
   *   - @ref ReturnCode::ErrorGeneral     : Peripheral fault.
   */
  virtual ReturnCode write(bool state) noexcept = 0;

  /**
   * @brief  Toggle the current output level of the pin.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Level toggled successfully.
   *   - @ref ReturnCode::NotInitialized   : Pin object is in an invalid state.
   *   - @ref ReturnCode::ErrorGeneral     : Peripheral fault.
   */
  virtual ReturnCode toggle() noexcept = 0;

  // -------------------------------------------------------------------------
  // Interrupt
  // -------------------------------------------------------------------------

  /**
   * @brief     Arm the EXTI interrupt with the given trigger mode and callback.
   * @details   Registers this GPIO instance in the ISR dispatch table and stores
   *            @p callback so it is invoked from @ref handleIrq when the interrupt
   *            fires.  The NVIC priority and enable must be configured by the BSP
   *            layer (e.g. CubeMX-generated code) before calling this method.
   *            Call with @p mode = @ref GpioIrqMode::Disabled to stop receiving
   *            events (the callback is retained but never called).
   *
   * @param[in] mode      Edge(s) that will trigger the interrupt.
   * @param[in] callback  Callable invoked from ISR context on each event.
   *                      Pass a default-constructed @ref GpioCallback to clear.
   */
  virtual void enableInterrupt(GpioIrqMode mode, GpioCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — GPIO instances are non-copyable singletons. */
  iGpio& operator=(const iGpio&) = delete;
  /** @brief Deleted — GPIO instances are non-movable singletons. */
  iGpio& operator=(iGpio&&) = delete;

  /**
   * @brief     Internal ISR handler — called by the driver's ISR dispatch.
   * @param[in] cause  Edge that triggered the interrupt.
   * @warning   Must only be called from ISR context via the driver's static
   *            dispatch function.  Not part of the public API.
   */
  virtual void handleIrq(GpioIrqMode cause) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_IGPIO_HPP_
