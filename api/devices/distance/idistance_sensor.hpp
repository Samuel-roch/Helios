/**
 ******************************************************************************
 * @file    idistance_sensor.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_DISTANCE
 * @brief   Distance sensor interface for time-of-flight and ultrasonic devices.
 *
 * @details
 *   - Single-shot blocking measurement with configurable timeout
 *   - Continuous ranging mode with data-ready notification
 *   - Configurable ranging profile (Short / Medium / Long) that adjusts the
 *     trade-off between maximum range, ambient light immunity, and accuracy
 *   - Out-of-range notification when the target is beyond the measurable distance
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Distance is expressed in millimeters (mm).
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.  The callback may fire from ISR context.
 */

#ifndef HELIOS_DEV_IDISTANCE_SENSOR_HPP_
#define HELIOS_DEV_IDISTANCE_SENSOR_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// DistanceRangingMode
// =============================================================================

/**
 * @enum  DistanceRangingMode
 * @brief Ranging profile that controls the trade-off between range and immunity.
 * @ingroup HELIOS_DEV_DISTANCE
 * @details Implementations map these profiles to the IC-specific timing budgets
 *   or pulse configurations.  On ICs with only one supported mode,
 *   @ref Medium is always accepted and others return
 *   @ref ReturnCode::ErrorNotSupported.
 */
enum class DistanceRangingMode : uint8_t
{
  Short  = 0x00U, /*!< Up to ~1.3 m; best ambient light immunity.         */
  Medium = 0x01U, /*!< Up to ~3 m; balanced range and immunity.           */
  Long   = 0x02U  /*!< Up to ~4+ m; maximum range, reduced immunity.      */
};

// =============================================================================
// DistanceSensorEvent
// =============================================================================

/**
 * @enum  DistanceSensorEvent
 * @brief Events reported via the registered @ref DistanceSensorCallback.
 * @ingroup HELIOS_DEV_DISTANCE
 */
enum class DistanceSensorEvent : uint8_t
{
  DataReady  = 0x00U, /*!< A new distance measurement is available.                     */
  OutOfRange = 0x01U  /*!< Target is beyond the sensor's measurable distance.           */
};

// @formatter:on

// =============================================================================
// DistanceSensorCallback
// =============================================================================

/**
 * @brief Callable type invoked when a distance sensor event occurs.
 *
 * @details Signature: `void handler(DistanceSensorEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using DistanceSensorCallback = Callback<void(DistanceSensorEvent)>;

// =============================================================================
// iDistanceSensor
// =============================================================================

/**
 * @class  iDistanceSensor
 * @brief  Hardware-agnostic distance sensor interface.
 * @ingroup HELIOS_DEV_DISTANCE
 *
 * @details
 *   - Single-shot blocking measurement via @ref getDistance, which triggers one
 *     ranging cycle and waits until the result is available or the timeout expires.
 *   - Continuous mode via @ref startContinuous / @ref stopContinuous; each new
 *     measurement fires @ref DistanceSensorEvent::DataReady via the callback.
 *   - Ranging profile selectable with @ref setRangingMode.
 *   - Measurement period in continuous mode configurable with @ref setMeasurementPeriod.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; sensor instances are singletons owned by the BSP layer.
 */
class iDistanceSensor
{
public:
  virtual ~iDistanceSensor() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the sensor and verify communication.
   * @details Loads default calibration, configures the IC, and confirms the
   *   device is responsive.  Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed     : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  /**
   * @brief     Start continuous ranging.
   * @details   The sensor repeatedly performs measurements at the interval
   *   configured by @ref setMeasurementPeriod.  Each completed measurement fires
   *   @ref DistanceSensorEvent::DataReady via the registered callback.
   *   Calling while already running is a no-op and returns @ref ReturnCode::AnsweredRequest.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Continuous ranging started (or already running).
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode startContinuous() noexcept = 0;

  /**
   * @brief  Stop continuous ranging.
   * @details Halts the continuous measurement loop.  Any in-progress measurement
   *   completes before the sensor enters standby; no further callbacks are fired.
   *   Calling while already stopped is a no-op and returns @ref ReturnCode::AnsweredRequest.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Continuous ranging stopped (or was not running).
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode stopContinuous() noexcept = 0;

  // -------------------------------------------------------------------------
  // Measurement
  // -------------------------------------------------------------------------

  /**
   * @brief      Perform a single-shot blocking distance measurement.
   * @details    Triggers one ranging cycle, waits for the result, and returns
   *   the measured distance.  Must not be called while continuous ranging is active.
   * @param[out] distance_mm  Measured distance in millimeters on success.
   * @param[in]  timeout_ms   Maximum time to wait for the measurement to complete.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p distance_mm is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorTimeout    : Measurement did not complete within @p timeout_ms.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   *   - @ref ReturnCode::FunctionBusy    : Continuous ranging is active; stop it first.
   */
  [[nodiscard]]
  virtual ReturnCode getDistance(uint32_t& distance_mm, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Check whether a new measurement result is ready to be read.
   * @param[out] ready  Set to @c true if a measurement is waiting in the IC buffer.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p ready reflects the current DRDY state.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Status register could not be read.
   */
  [[nodiscard]]
  virtual ReturnCode isDataReady(bool& ready) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Configuration
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the ranging profile.
   * @param[in] mode  Desired @ref DistanceRangingMode.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Mode applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support the requested mode.
   */
  [[nodiscard]]
  virtual ReturnCode setRangingMode(DistanceRangingMode mode) noexcept = 0;

  /**
   * @brief      Read the currently configured ranging profile.
   * @param[out] mode  Set to the active @ref DistanceRangingMode on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p mode is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getRangingMode(DistanceRangingMode& mode) const noexcept = 0;

  /**
   * @brief     Set the measurement period for continuous ranging mode.
   * @details   The IC attempts to honour the requested period; the actual period
   *   is bounded by the minimum measurement time for the active ranging profile.
   * @param[in] period_ms  Desired period between consecutive measurements in milliseconds.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Period applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p period_ms is zero or below the IC minimum.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support configurable measurement period.
   */
  [[nodiscard]]
  virtual ReturnCode setMeasurementPeriod(uint32_t period_ms) noexcept = 0;

  /**
   * @brief      Read the currently configured measurement period.
   * @param[out] period_ms  Active measurement period in milliseconds on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p period_ms is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support configurable measurement period.
   */
  [[nodiscard]]
  virtual ReturnCode getMeasurementPeriod(uint32_t& period_ms) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref DistanceSensorEvent.
   *   Pass a default-constructed @ref DistanceSensorCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(DistanceSensorCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — sensor instances are non-copyable singletons. */
  iDistanceSensor& operator=(const iDistanceSensor&) = delete;
  /** @brief Deleted — sensor instances are non-movable singletons. */
  iDistanceSensor& operator=(iDistanceSensor&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The sensor event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(DistanceSensorEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_IDISTANCE_SENSOR_HPP_
