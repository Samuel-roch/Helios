/**
 ******************************************************************************
 * @file    iio_expander.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_IO_EXPANDER
 * @brief   GPIO expander device interface.
 *
 * @details
 *   - Per-pin direction and pull-up configuration
 *   - Individual pin read/write and atomic port-level read/write (up to 16 pins)
 *   - Per-pin interrupt-on-change with callback notification
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Pins are indexed from 0.  8-pin expanders (e.g. PCF8574) use indices [0, 7];
 *   16-pin expanders (e.g. MCP23017) use indices [0, 15].  Passing an index
 *   beyond the physical pin count returns @ref ReturnCode::ErrorParam.
 *   Port-level operations use a @c uint16_t bitmask; for 8-pin devices the
 *   upper byte is ignored on write and reads as 0x00.
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_IIO_EXPANDER_HPP_
#define HELIOS_DEV_IIO_EXPANDER_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// IoExpanderPinMode
// =============================================================================

/**
 * @enum  IoExpanderPinMode
 * @brief Direction and pull configuration for a single GPIO expander pin.
 * @ingroup HELIOS_DEV_IO_EXPANDER
 */
enum class IoExpanderPinMode : uint8_t
{
  Input       = 0x00U, /*!< High-impedance digital input.                             */
  InputPullUp = 0x01U, /*!< Digital input with internal pull-up resistor enabled.     */
  Output      = 0x02U  /*!< Push-pull digital output.                                 */
};

// =============================================================================
// IoExpanderEvent
// =============================================================================

/**
 * @enum  IoExpanderEvent
 * @brief Events reported via the registered @ref IoExpanderCallback.
 * @ingroup HELIOS_DEV_IO_EXPANDER
 */
enum class IoExpanderEvent : uint8_t
{
  InputChanged = 0x00U  /*!< One or more input pins changed state; call
                         *   @ref iIoExpander::readPort or
                         *   @ref iIoExpander::readPin to read the new values. */
};

// @formatter:on

// =============================================================================
// IoExpanderCallback
// =============================================================================

/**
 * @brief Callable type invoked when a GPIO expander event occurs.
 *
 * @details Signature: `void handler(IoExpanderEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using IoExpanderCallback = Callback<void(IoExpanderEvent)>;

// =============================================================================
// iIoExpander
// =============================================================================

/**
 * @class  iIoExpander
 * @brief  Hardware-agnostic GPIO expander device interface.
 * @ingroup HELIOS_DEV_IO_EXPANDER
 *
 * @details
 *   - Configures pin direction per-pin via @ref setPinMode.
 *   - Reads and writes individual pins via @ref readPin and @ref writePin.
 *   - Reads and writes all pins atomically via @ref readPort and @ref writePort
 *     using a @c uint16_t bitmask (bit 0 = pin 0, bit N = pin N).
 *   - Per-pin interrupt-on-change enabled with @ref setInterruptEnabled;
 *     the @ref IoExpanderCallback fires when any enabled input changes state.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; expander instances are singletons owned by the BSP layer.
 */
class iIoExpander
{
public:
  virtual ~iIoExpander() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the GPIO expander and verify communication.
   * @details Configures the IC to a known state (all pins as inputs) and
   *   confirms the device is responsive.  Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed     : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  // -------------------------------------------------------------------------
  // Pin configuration
  // -------------------------------------------------------------------------

  /**
   * @brief     Configure the mode of a single pin.
   * @param[in] pin   Pin index [0, N−1] where N is the physical pin count.
   * @param[in] mode  Desired @ref IoExpanderPinMode.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Mode applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p pin exceeds the physical pin count.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : Requested mode not supported by the IC.
   */
  [[nodiscard]]
  virtual ReturnCode setPinMode(uint8_t pin, IoExpanderPinMode mode) noexcept = 0;

  /**
   * @brief      Read the configured mode of a single pin.
   * @param[in]  pin   Pin index.
   * @param[out] mode  Set to the active @ref IoExpanderPinMode on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p mode is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p pin exceeds the physical pin count.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getPinMode(uint8_t pin, IoExpanderPinMode& mode) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Pin-level I/O
  // -------------------------------------------------------------------------

  /**
   * @brief     Write a logical level to a single output pin.
   * @param[in] pin    Pin index.
   * @param[in] value  @c true = high; @c false = low.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Level applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p pin exceeds the physical pin count.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorInvalidState : Pin is configured as an input.
   */
  [[nodiscard]]
  virtual ReturnCode writePin(uint8_t pin, bool value) noexcept = 0;

  /**
   * @brief      Read the logical level of a single pin.
   * @param[in]  pin    Pin index.
   * @param[out] value  Set to @c true if the pin is high, @c false if low.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p value is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p pin exceeds the physical pin count.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode readPin(uint8_t pin, bool& value) noexcept = 0;

  // -------------------------------------------------------------------------
  // Port-level I/O
  // -------------------------------------------------------------------------

  /**
   * @brief     Write all output pins atomically.
   * @details   Each bit in @p value corresponds to a pin: bit 0 = pin 0,
   *   bit N = pin N.  Bits corresponding to input pins are ignored.
   *   For 8-pin ICs the upper byte of @p value is ignored.
   * @param[in] value  Bitmask of levels to apply to all output pins.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Levels applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode writePort(uint16_t value) noexcept = 0;

  /**
   * @brief      Read all pin levels atomically.
   * @details    Each bit in @p value corresponds to a pin: bit 0 = pin 0,
   *   bit N = pin N.  Output pin bits reflect the last written value.
   *   For 8-pin ICs the upper byte of @p value reads as 0x00.
   * @param[out] value  Bitmask of the current logical levels of all pins.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p value is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode readPort(uint16_t& value) noexcept = 0;

  // -------------------------------------------------------------------------
  // Interrupt on change
  // -------------------------------------------------------------------------

  /**
   * @brief     Enable or disable interrupt-on-change for a single input pin.
   * @details   When enabled, a state change on @p pin causes the IC to assert its
   *   INT line, triggering @ref IoExpanderCallback with
   *   @ref IoExpanderEvent::InputChanged.
   *   Returns @ref ReturnCode::ErrorNotSupported on ICs without interrupt support.
   * @param[in] pin      Pin index.
   * @param[in] enabled  @c true to enable; @c false to disable.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p pin exceeds the physical pin count.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support interrupt-on-change.
   */
  [[nodiscard]]
  virtual ReturnCode setInterruptEnabled(uint8_t pin, bool enabled) noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref IoExpanderEvent.
   *   Pass a default-constructed @ref IoExpanderCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(IoExpanderCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — expander instances are non-copyable singletons. */
  iIoExpander& operator=(const iIoExpander&) = delete;
  /** @brief Deleted — expander instances are non-movable singletons. */
  iIoExpander& operator=(iIoExpander&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The expander event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(IoExpanderEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_IIO_EXPANDER_HPP_
