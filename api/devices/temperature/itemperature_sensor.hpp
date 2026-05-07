/**
 ******************************************************************************
 * @file    itemperature_sensor.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_TEMPERATURE
 * @brief   External temperature (and optional humidity) sensor interface.
 *
 * @details
 *   - Blocking temperature read with tenths-of-°C resolution
 *   - Optional humidity reading (returns @ref ReturnCode::ErrorNotSupported
 *     on sensors without an integrated humidity element)
 *   - Configurable high/low alert thresholds with callback notification
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Temperature is expressed in tenths of °C (deci-Celsius, dc);
 *   e.g. 253 represents 25.3 °C and -100 represents -10.0 °C.
 *   Humidity is expressed in percent [0, 100].
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_ITEMPERATURE_SENSOR_HPP_
#define HELIOS_DEV_ITEMPERATURE_SENSOR_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// TemperatureEvent
// =============================================================================

/**
 * @enum  TemperatureEvent
 * @brief Events reported via the registered @ref TemperatureSensorCallback.
 * @ingroup HELIOS_DEV_TEMPERATURE
 */
enum class TemperatureEvent : uint8_t
{
  DataReady          = 0x00U, /*!< A new measurement is available to read.              */
  HighAlertTriggered = 0x01U, /*!< Temperature rose above the configured high threshold. */
  LowAlertTriggered  = 0x02U  /*!< Temperature fell below the configured low threshold.  */
};

// @formatter:on

// =============================================================================
// TemperatureSensorCallback
// =============================================================================

/**
 * @brief Callable type invoked when a temperature sensor event occurs.
 *
 * @details Signature: `void handler(TemperatureEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using TemperatureSensorCallback = Callback<void(TemperatureEvent)>;

// =============================================================================
// iTemperatureSensor
// =============================================================================

/**
 * @class  iTemperatureSensor
 * @brief  Hardware-agnostic external temperature sensor interface.
 * @ingroup HELIOS_DEV_TEMPERATURE
 *
 * @details
 *   - Reads temperature via @ref getTemperature in tenths of °C.
 *   - Reads optional humidity via @ref getHumidity in percent; returns
 *     @ref ReturnCode::ErrorNotSupported on sensors without a humidity element.
 *   - High and low alert thresholds can be configured; the IC asserts an interrupt
 *     line when a threshold is crossed, delivered via @ref TemperatureSensorCallback.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; sensor instances are singletons owned by the BSP layer.
 */
class iTemperatureSensor
{
public:
  virtual ~iTemperatureSensor() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the sensor and verify communication.
   * @details Configures the IC to a known state and confirms the device is
   *   responsive on the bus.  Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed     : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  // -------------------------------------------------------------------------
  // Measurements
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the current temperature.
   * @param[out] temperature_dc  Temperature in tenths of °C on success
   *   (e.g. 253 = 25.3 °C; -100 = -10.0 °C).
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p temperature_dc is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTemperature(int32_t& temperature_dc) noexcept = 0;

  /**
   * @brief      Read the current relative humidity.
   * @param[out] humidity_pct  Relative humidity in percent [0, 100] on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p humidity_pct is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : Sensor does not have a humidity element.
   */
  [[nodiscard]]
  virtual ReturnCode getHumidity(uint8_t& humidity_pct) noexcept = 0;

  // -------------------------------------------------------------------------
  // Alert thresholds
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the high-temperature alert threshold.
   * @details   The IC fires @ref TemperatureEvent::HighAlertTriggered when the
   *   temperature rises above this value.  Returns @ref ReturnCode::ErrorNotSupported
   *   if the IC does not support programmable alert thresholds.
   * @param[in] threshold_dc  High threshold in tenths of °C.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Threshold applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p threshold_dc is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support programmable thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode setHighAlert(int32_t threshold_dc) noexcept = 0;

  /**
   * @brief      Read the currently configured high-temperature alert threshold.
   * @param[out] threshold_dc  High threshold in tenths of °C on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p threshold_dc is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support programmable thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode getHighAlert(int32_t& threshold_dc) const noexcept = 0;

  /**
   * @brief     Set the low-temperature alert threshold.
   * @details   The IC fires @ref TemperatureEvent::LowAlertTriggered when the
   *   temperature falls below this value.  Returns @ref ReturnCode::ErrorNotSupported
   *   if the IC does not support programmable alert thresholds.
   * @param[in] threshold_dc  Low threshold in tenths of °C.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Threshold applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p threshold_dc is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support programmable thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode setLowAlert(int32_t threshold_dc) noexcept = 0;

  /**
   * @brief      Read the currently configured low-temperature alert threshold.
   * @param[out] threshold_dc  Low threshold in tenths of °C on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p threshold_dc is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support programmable thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode getLowAlert(int32_t& threshold_dc) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref TemperatureEvent.
   *   Pass a default-constructed @ref TemperatureSensorCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(TemperatureSensorCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — sensor instances are non-copyable singletons. */
  iTemperatureSensor& operator=(const iTemperatureSensor&) = delete;
  /** @brief Deleted — sensor instances are non-movable singletons. */
  iTemperatureSensor& operator=(iTemperatureSensor&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The sensor event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(TemperatureEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_ITEMPERATURE_SENSOR_HPP_
