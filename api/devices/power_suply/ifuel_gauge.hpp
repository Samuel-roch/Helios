/**
 ******************************************************************************
 * @file    ifuel_gauge.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-30
 * @ingroup HELIOS_DEV_FUEL_GAUGE
 * @brief   Battery fuel gauge (coulomb counter) device interface.
 *
 * @details
 *   - Real-time measurements of voltage, current and temperature
 *   - State-of-charge as percentage and discrete capacity level
 *   - Remaining and full-charge capacity in miliampere-hours (mAh)
 *   - Estimated time-to-empty and time-to-full in seconds
 *   - Charge cycle count for battery lifetime tracking
 *   - Alert notification via @ref FuelGaugeCallback on configurable thresholds
 *   - No dynamic allocation; implementations map each method to the underlying
 *     fuel gauge IC or coulomb counter peripheral
 *
 * @note
 *   All voltages are in millivolts (mV), currents in milliamperes (mA),
 *   charges in milliampere-hours (mAh), and temperatures in tenths of a
 *   degree Celsius (dC).
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.  The alert callback may fire from ISR context
 *   (implementation-defined).
 */

#ifndef HELIOS_DEV_IFUEL_GAUGE_HPP_
#define HELIOS_DEV_IFUEL_GAUGE_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// FuelGaugeAlert
// =============================================================================

/**
 * @enum  FuelGaugeAlert
 * @brief Alert conditions reported via the registered @ref FuelGaugeCallback.
 * @ingroup HELIOS_DEV_FUEL_GAUGE
 */
enum class FuelGaugeAlert : uint8_t
{
  LowCapacity      = 0x00U, /*!< State-of-charge fell below the low threshold.             */
  CriticalCapacity = 0x01U, /*!< State-of-charge fell below the critical threshold.        */
  Overvoltage      = 0x02U, /*!< Battery voltage exceeded the upper alert limit.           */
  Undervoltage     = 0x03U, /*!< Battery voltage fell below the lower alert limit.         */
  Overtemperature  = 0x04U, /*!< Battery temperature exceeded the upper alert limit.       */
  Undertemperature = 0x05U, /*!< Battery temperature fell below the lower alert limit.     */
  FullCharge       = 0x06U  /*!< Battery reached full charge; charging has terminated.     */
};


/**
 * @enum  OperationMode
 * @brief Top-level power mode of the fuel gauge IC.
 * @ingroup HELIOS_DEV_FUEL_GAUGE
 */
enum class OperationMode : uint8_t
{
  Normal    = 0x00U, /*!< Normal operation; all measurements and alerts active.               */
  Sleep     = 0x01U, /*!< Low-power sleep; measurements and alerts are paused.                */
  DeepSleep = 0x02U  /*!< Deep sleep; minimal power consumption, no measurements or alerts.   */
};

// @formatter:on

// =============================================================================
// FuelGaugeCallback
// =============================================================================

/**
 * @brief Callable type invoked when a fuel gauge alert fires.
 *
 * @details Signature: `void handler(FuelGaugeAlert alert) noexcept`
 *   - @p alert — the condition that triggered the callback; call the
 *     corresponding getter (e.g. @ref iFuelGauge::getStateOfCharge,
 *     @ref iFuelGauge::getVoltage) to read the current value.
 *
 * @note  The callback may fire from ISR context; the implementation must not
 *        block or call functions that disable interrupts for extended periods.
 */
using FuelGaugeCallback = Callback<void(FuelGaugeAlert)>;

// =============================================================================
// iFuelGauge
// =============================================================================

/**
 * @class  iFuelGauge
 * @brief  Hardware-agnostic battery fuel gauge (coulomb counter) interface.
 * @ingroup HELIOS_DEV_FUEL_GAUGE
 *
 * @details
 *   - Electrical measurements: voltage (@ref getVoltage), current (@ref getCurrent)
 *     and temperature (@ref getTemperature).
 *   - Capacity reporting: state-of-charge percentage (@ref getStateOfCharge),
 *     discrete level (@ref getCapacityLevel), remaining charge (@ref getRemainingCharge)
 *     and design capacity (@ref getFullChargeCapacity).
 *   - Lifetime metrics: cycle count (@ref getCycleCount).
 *   - Time estimates: time to empty (@ref getTimeToEmpty) and time to full
 *     (@ref getTimeToFull) when supported by the IC.
 *   - Alert notifications delivered via a @ref FuelGaugeCallback registered
 *     with @ref setCallback.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; fuel gauge devices are singletons owned by the BSP layer.
 */
class iFuelGauge
{
public:
  virtual ~iFuelGauge() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the fuel gauge IC and verify communication.
   * @details Configures the IC to a known state and confirms it is responsive
   *   on the bus. Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest    : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed    : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  /**
   * @brief  Perform a full reset of the fuel gauge IC.
   * @details Resets all learned capacity data and re-initiates the SoC
   *   estimation algorithm. Use with care — accumulated learning is lost.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Reset successful.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode reset() noexcept = 0;

  /**
   * @brief     Set the top-level power mode of the fuel gauge IC.
   * @param[in] mode  Desired @ref OperationMode.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Mode applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setOperationMode(OperationMode mode) noexcept = 0;

  /**
   * @brief      Read the current power mode of the fuel gauge IC.
   * @param[out] mode  Set to the active @ref OperationMode on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p mode is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getOperationMode(OperationMode& mode) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Electrical measurements
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the minimum battery voltage below which the system should shut down.
   * @details   The IC uses this threshold to estimate remaining capacity near empty.
   * @param[in] voltage_mv  Terminate voltage in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam       : @p voltage_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setTerminateVoltage(uint16_t voltage_mv) noexcept = 0;

  /**
   * @brief      Read the battery terminal voltage.
   * @param[out] voltage_mv  Battery voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getVoltage(int32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief      Read the instantaneous battery current.
   * @param[out] current_ma  Battery current in milliamperes (mA) on success.
   *   Positive values indicate charging; negative values indicate discharging.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getCurrent(int32_t& current_ma) const noexcept = 0;

  /**
   * @brief      Read the battery temperature.
   * @param[out] temp_dc  Battery temperature in tenths of a degree Celsius (dC) on success.
   *   For example, 253 represents 25.3 °C.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p temp_dc is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTemperature(int32_t& temp_dc) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Capacity configuration
  // -------------------------------------------------------------------------

  /**
   * @brief     Set the design capacity of the installed battery cell.
   * @details   Used by the IC's SoC estimation algorithm as the reference
   *   full-charge value. Should match the cell datasheet at the nominal rate.
   * @param[in] capacity_mah  Design capacity in milliampere-hours (mAh).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setCapacity(uint32_t capacity_mah) noexcept = 0;

  /**
   * @brief     Set the design energy of the installed battery cell.
   * @details   Provides a more accurate SoC estimate when the IC supports
   *   energy-based compensation. Optional; not all ICs expose this parameter.
   * @param[in] energy_mwh  Design energy in milliwatt-hours (mWh).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support design energy configuration.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setDesignEnergy(uint32_t energy_mwh) noexcept = 0;

  // -------------------------------------------------------------------------
  // State of charge
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the state of charge as an integer percentage.
   * @param[out] percent  State of charge in percent [0, 100] on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p percent is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getStateOfCharge(uint8_t& percent) const noexcept = 0;

  /**
   * @brief      Read the remaining charge in the battery.
   * @param[out] charge_mah  Remaining charge in milliampere-hours (mAh) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p charge_mah is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getRemainingCharge(int32_t& charge_mah) const noexcept = 0;

  /**
   * @brief      Read the learned full-charge capacity of the battery.
   * @details    May differ from the design capacity as the IC updates its model
   *   through charge cycles.
   * @param[out] capacity_mah  Full-charge capacity in milliampere-hours (mAh) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p capacity_mah is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getFullChargeCapacity(int32_t& capacity_mah) const noexcept = 0;

  /**
   * @brief      Read the estimated time remaining until the battery is empty.
   * @param[out] seconds  Estimated time to empty in seconds on success.
   *   Returns @ref ReturnCode::ErrorNotSupported when the IC does not provide this estimate.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Read successful; @p seconds is valid.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support time-to-empty estimation.
   *   - @ref ReturnCode::ErrorReadFailed  : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTimeToEmpty(uint32_t& seconds) const noexcept = 0;

  /**
   * @brief      Read the estimated time remaining until the battery is fully charged.
   * @param[out] seconds  Estimated time to full in seconds on success.
   *   Returns @ref ReturnCode::ErrorNotSupported when the IC does not provide this estimate.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Read successful; @p seconds is valid.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not support time-to-full estimation.
   *   - @ref ReturnCode::ErrorReadFailed  : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTimeToFull(uint32_t& seconds) const noexcept = 0;

  /**
   * @brief      Read the total number of charge cycles accumulated by the battery.
   * @details    One cycle is counted when the cumulative discharged charge equals
   *   the design capacity. Used for battery lifetime tracking.
   * @param[out] count  Cycle count on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Read successful; @p count is valid.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not track cycle count.
   *   - @ref ReturnCode::ErrorReadFailed  : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getCycleCount(uint16_t& count) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Power
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the average power drawn from or delivered to the battery.
   * @param[out] power_mw  Average power in milliwatts (mW) on success.
   *   Positive values indicate charging; negative values indicate discharging.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Read successful; @p power_mw is valid.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not report average power.
   *   - @ref ReturnCode::ErrorReadFailed  : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getAveragePower(int32_t& power_mw) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Alert notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the alert callback.
   * @param[in] callback  Callable invoked on each @ref FuelGaugeAlert.
   *   Pass a default-constructed @ref FuelGaugeCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or perform operations that disable interrupts for extended periods.
   */
  virtual void setCallback(FuelGaugeCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — fuel gauge instances are non-copyable singletons. */
  iFuelGauge& operator=(const iFuelGauge&) = delete;
  /** @brief Deleted — fuel gauge instances are non-movable singletons. */
  iFuelGauge& operator=(iFuelGauge&&) = delete;

  /**
   * @brief     Internal handler for alert events, called by the driver or ISR.
   * @details   Invokes the registered @ref FuelGaugeCallback with the alert condition.
   *            Implementations must not block, allocate memory, or throw.
   *
   * @param[in] alert  Alert condition that fired (see @ref FuelGaugeAlert).
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public application API.
   */
  virtual void handleAlert(FuelGaugeAlert alert) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_IFUEL_GAUGE_HPP_
