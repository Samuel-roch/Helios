/**
 ******************************************************************************
 * @file    icurrent_sensor.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_CURRENT
 * @brief   External current and power monitoring sensor interface.
 *
 * @details
 *   - Bus voltage, shunt current, and calculated power readings
 *   - Optional accumulated energy counter (returns @ref ReturnCode::ErrorNotSupported
 *     on sensors without an energy accumulator register)
 *   - Configurable over-current and under-voltage alert thresholds
 *   - Asynchronous alert notification via @ref CurrentSensorCallback
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Current is expressed in milliamperes (mA); voltage in millivolts (mV);
 *   power in milliwatts (mW); energy in milliwatt-hours (mWh).
 *   Positive current indicates current flowing into the load (discharge);
 *   the sign convention is implementation-defined and depends on the shunt
 *   orientation on the board.
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_ICURRENT_SENSOR_HPP_
#define HELIOS_DEV_ICURRENT_SENSOR_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// CurrentSensorEvent
// =============================================================================

/**
 * @enum  CurrentSensorEvent
 * @brief Events reported via the registered @ref CurrentSensorCallback.
 * @ingroup HELIOS_DEV_CURRENT
 */
enum class CurrentSensorEvent : uint8_t
{
  DataReady          = 0x00U, /*!< A new measurement conversion is complete.              */
  OverCurrentAlert   = 0x01U, /*!< Current exceeded the configured over-current limit.    */
  UnderVoltageAlert  = 0x02U, /*!< Bus voltage fell below the configured lower limit.     */
  OverVoltageAlert   = 0x03U  /*!< Bus voltage exceeded the configured upper limit.       */
};

// @formatter:on

// =============================================================================
// CurrentSensorCallback
// =============================================================================

/**
 * @brief Callable type invoked when a current sensor event occurs.
 *
 * @details Signature: `void handler(CurrentSensorEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using CurrentSensorCallback = Callback<void(CurrentSensorEvent)>;

// =============================================================================
// iCurrentSensor
// =============================================================================

/**
 * @class  iCurrentSensor
 * @brief  Hardware-agnostic current and power monitoring sensor interface.
 * @ingroup HELIOS_DEV_CURRENT
 *
 * @details
 *   - Reads bus voltage via @ref getVoltage, shunt current via @ref getCurrent,
 *     and derived power via @ref getPower.
 *   - Optional accumulated energy counter via @ref getEnergy; cleared with
 *     @ref resetEnergy.  Returns @ref ReturnCode::ErrorNotSupported on sensors
 *     without an energy accumulator register (e.g. INA219).
 *   - Over-current and voltage alert thresholds configurable at runtime.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; sensor instances are singletons owned by the BSP layer.
 */
class iCurrentSensor
{
public:
  virtual ~iCurrentSensor() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the sensor and verify communication.
   * @details Calibrates the IC for the configured shunt resistor value and
   *   confirms the device is responsive on the bus.  Must be called before
   *   any other method.
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
   * @brief      Read the bus voltage.
   * @param[out] voltage_mv  Bus voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getVoltage(int32_t& voltage_mv) noexcept = 0;

  /**
   * @brief      Read the shunt current.
   * @param[out] current_ma  Current in milliamperes (mA) on success.
   *   The sign convention depends on the shunt orientation defined in the
   *   concrete implementation.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getCurrent(int32_t& current_ma) noexcept = 0;

  /**
   * @brief      Read the instantaneous power.
   * @details    Power is derived from bus voltage × shunt current; on ICs with a
   *   dedicated power register (e.g. INA226) the hardware result is returned
   *   directly.
   * @param[out] power_mw  Power in milliwatts (mW) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p power_mw is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getPower(int32_t& power_mw) noexcept = 0;

  /**
   * @brief      Read the accumulated energy counter.
   * @details    Available only on ICs with a hardware energy accumulator
   *   (e.g. INA228, INA238).  The counter increments continuously until
   *   cleared by @ref resetEnergy.
   * @param[out] energy_mwh  Accumulated energy in milliwatt-hours (mWh) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p energy_mwh is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not have an energy accumulator.
   */
  [[nodiscard]]
  virtual ReturnCode getEnergy(int32_t& energy_mwh) noexcept = 0;

  /**
   * @brief  Clear the accumulated energy counter.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Counter cleared successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not have an energy accumulator.
   */
  [[nodiscard]]
  virtual ReturnCode resetEnergy() noexcept = 0;

  // -------------------------------------------------------------------------
  // Alert thresholds
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the over-current alert threshold.
   * @details   The IC fires @ref CurrentSensorEvent::OverCurrentAlert when the
   *   absolute current value exceeds @p threshold_ma.
   * @param[in] threshold_ma  Current limit in milliamperes (mA).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Threshold applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p threshold_ma is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support current alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode setCurrentThreshold(uint32_t threshold_ma) noexcept = 0;

  /**
   * @brief      Read the currently configured over-current alert threshold.
   * @param[out] threshold_ma  Current threshold in milliamperes (mA) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p threshold_ma is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support current alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode getCurrentThreshold(uint32_t& threshold_ma) const noexcept = 0;

  /**
   * @brief     Set the bus under-voltage alert threshold.
   * @details   The IC fires @ref CurrentSensorEvent::UnderVoltageAlert when the
   *   bus voltage falls below @p threshold_mv.
   * @param[in] threshold_mv  Voltage threshold in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Threshold applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p threshold_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support voltage alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode setUnderVoltageThreshold(uint32_t threshold_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured bus under-voltage threshold.
   * @param[out] threshold_mv  Voltage threshold in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p threshold_mv is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support voltage alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode getUnderVoltageThreshold(uint32_t& threshold_mv) const noexcept = 0;

  /**
   * @brief     Set the bus over-voltage alert threshold.
   * @details   The IC fires @ref CurrentSensorEvent::OverVoltageAlert when the
   *   bus voltage exceeds @p threshold_mv.
   * @param[in] threshold_mv  Voltage threshold in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Threshold applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p threshold_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support voltage alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode setOverVoltageThreshold(uint32_t threshold_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured bus over-voltage threshold.
   * @param[out] threshold_mv  Voltage threshold in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p threshold_mv is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support voltage alert thresholds.
   */
  [[nodiscard]]
  virtual ReturnCode getOverVoltageThreshold(uint32_t& threshold_mv) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref CurrentSensorEvent.
   *   Pass a default-constructed @ref CurrentSensorCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(CurrentSensorCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — sensor instances are non-copyable singletons. */
  iCurrentSensor& operator=(const iCurrentSensor&) = delete;
  /** @brief Deleted — sensor instances are non-movable singletons. */
  iCurrentSensor& operator=(iCurrentSensor&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The sensor event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(CurrentSensorEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_ICURRENT_SENSOR_HPP_
