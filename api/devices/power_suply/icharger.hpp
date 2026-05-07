/**
 ******************************************************************************
 * @file    icharger.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-30
 * @ingroup HELIOS_DEV_CHARGER
 * @brief   Battery charger device interface.
 *
 * @details
 *   - Read-only access to charging status, health and active charge phase
 *   - Real-time monitoring of input voltage, battery voltage and charge current
 *   - Control over charging enable/disable and CC/CV parameters
 *   - Event notification via @ref ChargerCallback on status or phase changes
 *   - No dynamic allocation; implementations map each method to the underlying
 *     charger IC or controller
 *
 * @note
 *   All voltages are in millivolts (mV) and all currents in milliamperes (mA).
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.  The event callback may fire from ISR context
 *   (implementation-defined).
 */

#ifndef HELIOS_DEV_ICHARGER_HPP_
#define HELIOS_DEV_ICHARGER_HPP_

#include <hel_target>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// ChargerStatus
// =============================================================================

/**
 * @enum  ChargerStatus
 * @brief High-level charging state reported by the charger.
 * @ingroup HELIOS_DEV_CHARGER
 */
enum class ChargerStatus : uint8_t
{
  Unknown      = 0x00U, /*!< Charging state cannot be determined.                    */
  Charging     = 0x01U, /*!< Charger is actively transferring energy to the battery. */
  Discharging  = 0x02U, /*!< Battery is supplying energy to the system load.         */
  NotCharging  = 0x03U, /*!< Charger is connected but charging is inhibited.         */
  Full         = 0x04U  /*!< Battery is fully charged; charging has terminated.      */
};

// =============================================================================
// ChargerHealth
// =============================================================================

/**
 * @enum  ChargerHealth
 * @brief Battery and charger health condition.
 * @ingroup HELIOS_DEV_CHARGER
 */
enum class ChargerHealth : uint8_t
{
  Unknown             = 0x00U, /*!< Health cannot be determined.                                   */
  Good                = 0x01U, /*!< Battery and charger are operating within normal parameters.    */
  Overheat            = 0x02U, /*!< Temperature is above the maximum operating threshold.          */
  Cold                = 0x03U, /*!< Temperature is below the minimum operating threshold.          */
  Overvoltage         = 0x04U, /*!< Battery or input voltage exceeds safe limits.                  */
  Undervoltage        = 0x05U, /*!< Battery voltage is below the minimum operating level.          */
  Overcurrent         = 0x06U, /*!< Charge or input current exceeds the configured limit.          */
  Dead                = 0x07U, /*!< Battery is not responding or has failed beyond recovery.       */
  SafetyTimerExpired  = 0x08U  /*!< Charge safety timer expired; charging was halted by the IC.    */
};

// =============================================================================
// ChargePhase
// =============================================================================

/**
 * @enum  ChargePhase
 * @brief Active phase within the CC/CV charging algorithm.
 * @ingroup HELIOS_DEV_CHARGER
 * @details The standard Li-Ion/Lead-Acid CC/CV sequence is:
 *   Trickle → Precharge → ConstantCurrent → ConstantVoltage → Maintenance.
 *   Not all implementations support every phase; unsupported phases are skipped.
 */
enum class ChargePhase : uint8_t
{
  None              = 0x00U, /*!< No active charging phase (charger idle or disabled).           */
  Trickle           = 0x01U, /*!< Very low current applied to a deeply discharged battery.       */
  Precharge         = 0x02U, /*!< Limited current applied until voltage reaches precharge limit. */
  ConstantCurrent   = 0x03U, /*!< Main CC phase: constant current until voltage target is met.  */
  ConstantVoltage   = 0x04U, /*!< CV phase: voltage held constant; current tapers to cut-off.  */
  Maintenance       = 0x05U  /*!< Low-current phase to maintain full charge after CV completes.  */
};

// =============================================================================
// ChargerEvent
// =============================================================================

/**
 * @enum  ChargerEvent
 * @brief Events reported via the registered @ref ChargerCallback.
 * @ingroup HELIOS_DEV_CHARGER
 */
enum class ChargerEvent : uint8_t
{
  StatusChanged     = 0x00U, /*!< @ref ChargerStatus has transitioned to a new value.             */
  PhaseChanged      = 0x01U, /*!< @ref ChargePhase has advanced to the next charging stage.       */
  HealthAlert       = 0x02U, /*!< @ref ChargerHealth has changed to a non-Good condition.         */
  FaultDetected     = 0x03U, /*!< An unrecoverable hardware fault was detected; charging halted.  */
  InputConnected    = 0x04U, /*!< A valid input source was detected (power-good asserted).        */
  InputDisconnected = 0x05U, /*!< The input source was removed (power-good de-asserted).          */
  WatchdogExpired   = 0x06U  /*!< I²C watchdog timer expired; IC reset charge parameters to defaults. */
};

/**
 * @enum  ChargerMode
 * @brief Top-level operating mode of the charger IC.
 * @ingroup HELIOS_DEV_CHARGER
 */
enum class ChargerMode : uint8_t
{
  Normal   = 0x00U, /*!< Standard charging operation.                                            */
  Shipping = 0x01U  /*!< Shipping mode: minimal current to prevent deep discharge during storage. */
};

/**
 * @enum  ChargerFault
 * @brief Hardware fault conditions detected by the charger IC.
 * @ingroup HELIOS_DEV_CHARGER
 */
enum class ChargerFault : uint16_t
{
  Normal                = 0x00U, /*!< No fault; NTC temperature within normal range.              */
  NtcCold               = 0x01U, /*!< NTC indicates temperature below safe threshold.             */
  NtcHot                = 0x02U, /*!< NTC indicates temperature above safe threshold.             */
  BatOvervoltage        = 0x03U, /*!< Battery overvoltage fault detected.                         */
  BatUndervoltage       = 0x04U, /*!< Battery undervoltage fault detected.                        */
  InputOvervoltage      = 0x05U, /*!< Input overvoltage fault detected.                           */
  InputUndervoltage     = 0x06U, /*!< Input undervoltage fault detected.                          */
  ChargeOvercurrent     = 0x07U, /*!< Charge overcurrent fault detected.                          */
  WatchdogReset         = 0x08U  /*!< I²C watchdog expired; IC reset charge parameters to defaults. */
};

// @formatter:on

// =============================================================================
// ChargerCallback
// =============================================================================

/**
 * @brief Callable type invoked when a charger event occurs.
 *
 * @details Signature: `void handler(ChargerEvent event) noexcept`
 *   - @p event — the event that triggered the callback; call the corresponding
 *     getter (e.g. @ref iCharger::getStatus, @ref iCharger::getHealth) to read
 *     the updated value.
 *
 * @note  The callback may fire from ISR context; the implementation must not
 *        block or call functions that disable interrupts for extended periods.
 */
using ChargerCallback = Callback<void(ChargerEvent)>;

// =============================================================================
// iCharger
// =============================================================================

/**
 * @class  iCharger
 * @brief  Hardware-agnostic battery charger device interface.
 * @ingroup HELIOS_DEV_CHARGER
 *
 * @details
 *   - Monitors charging status (@ref getStatus), health (@ref getHealth) and
 *     active phase (@ref getPhase) throughout the charging cycle.
 *   - Reads real-time electrical measurements: input voltage (@ref getInputVoltage),
 *     battery voltage (@ref getBatteryVoltage) and charge current (@ref getChargeCurrent).
 *   - Controls the charger via @ref setChargingEnabled and the CC/CV parameters
 *     @ref setInputCurrentLimit, @ref setConstantChargeCurrent and
 *     @ref setConstantChargeVoltage.
 *   - Delivers asynchronous notifications through a @ref ChargerCallback registered
 *     via @ref setCallback.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; charger devices are singletons owned by the BSP layer.
 */
class iCharger
{
public:
  virtual ~iCharger() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the charger IC and verify communication.
   * @details Configures the IC to a known state and confirms the device is
   *   responsive on the bus. Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  /**
   * @brief     Set the top-level operating mode of the charger.
   * @param[in] mode  Desired @ref ChargerMode (Normal or Shipping).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Mode applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setOperationMode(ChargerMode mode) noexcept = 0;

  // -------------------------------------------------------------------------
  // Status and health
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the current charging status.
   * @param[out] status  Set to the active @ref ChargerStatus on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p status is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getStatus(ChargerStatus& status) const noexcept = 0;

  /**
   * @brief      Read the current battery and charger health.
   * @param[out] health  Set to the active @ref ChargerHealth on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p health is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getHealth(ChargerHealth& health) const noexcept = 0;

  /**
   * @brief      Read the active phase within the CC/CV charging algorithm.
   * @param[out] phase  Set to the active @ref ChargePhase on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p phase is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getPhase(ChargePhase& phase) const noexcept = 0;

  /**
   * @brief      Read the current hardware fault condition.
   * @param[out] fault  Set to the active @ref ChargerFault on success.
   *   Set to @ref ChargerFault::Normal if no fault is present.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p fault is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getFault(ChargerFault& fault) const noexcept = 0;

  /**
   * @brief  Restore all charger configuration registers to their IC defaults.
   * @details Resets CC/CV parameters, current limits and voltage limits to
   *   the power-on default values defined by the IC vendor. Does not affect
   *   the initialized state of this driver object.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Defaults restored successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode resetDefaults() noexcept = 0;

  // -------------------------------------------------------------------------
  // Measurements
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the voltage present at the charger input.
   * @param[out] voltage_mv  Input voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getInputVoltage(int32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief      Read the battery terminal voltage.
   * @param[out] voltage_mv  Battery voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getBatteryVoltage(int32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief      Read the instantaneous charge current flowing into the battery.
   * @param[out] current_ma  Charge current in milliamperes (mA) on success.
   *   Positive values indicate charging; negative values indicate discharging.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getChargeCurrent(int32_t& current_ma) const noexcept = 0;

  /**
   * @brief      Check whether a valid input power source is present.
   * @param[out] present  Set to @c true if a valid input source is detected,
   *   @c false otherwise.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p present is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode isInputPresent(bool& present) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Configuration
  // -------------------------------------------------------------------------

  /**
   * @brief     Enable or disable the charging function.
   * @param[in] enabled  @c true to allow charging; @c false to inhibit it.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setChargingEnabled(bool enabled) noexcept = 0;

  /**
   * @brief     Set the maximum current drawn from the input source.
   * @param[in] current_ma  Input current limit in milliamperes (mA).
   *   The IC rounds the value to the nearest supported step.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Limit applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p current_ma exceeds the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setInputCurrentLimit(uint32_t current_ma) noexcept = 0;

  /**
   * @brief      Read the currently configured input current limit.
   * @param[out] current_ma  Current limit in milliamperes (mA) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getInputCurrentLimit(uint32_t& current_ma) const noexcept = 0;

  /**
   * @brief     Set the minimum input voltage threshold (VINDPM).
   * @details   If the input voltage falls below this limit, the IC reduces
   *   charge current to keep the input above the threshold.
   * @param[in] voltage_mv  Input voltage limit in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Limit applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p voltage_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setInputVoltageLimit(uint32_t voltage_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured input voltage limit (VINDPM).
   * @param[out] voltage_mv  Voltage limit in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getInputVoltageLimit(uint32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief     Set the pre-charge current applied to a deeply discharged battery.
   * @details   Applied during the Trickle and Precharge phases until the battery
   *   voltage rises above the pre-charge-to-fast-charge threshold.
   * @param[in] current_ma  Pre-charge current in milliamperes (mA).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p current_ma is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setPreChargeCurrent(uint32_t current_ma) noexcept = 0;

  /**
   * @brief      Read the currently configured pre-charge current.
   * @param[out] current_ma  Pre-charge current in milliamperes (mA) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getPreChargeCurrent(uint32_t& current_ma) const noexcept = 0;

  /**
   * @brief     Set the termination current threshold for end-of-charge detection.
   * @details   Charging stops when the current in the CV phase drops below this
   *   threshold. A lower value results in a fuller charge; too low may prevent
   *   termination on some batteries.
   * @param[in] current_ma  Termination current in milliamperes (mA).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p current_ma is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setTerminationCurrent(uint32_t current_ma) noexcept = 0;

  /**
   * @brief      Read the currently configured termination current threshold.
   * @param[out] current_ma  Termination current in milliamperes (mA) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTerminationCurrent(uint32_t& current_ma) const noexcept = 0;

  /**
   * @brief     Set the constant charge current (CC phase fast-charge current).
   * @param[in] current_ma  Charge current in milliamperes (mA).
   *   The IC rounds the value to the nearest supported step.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p current_ma exceeds the IC's maximum charge current.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setConstantChargeCurrent(uint32_t current_ma) noexcept = 0;

  /**
   * @brief      Read the currently configured constant charge current.
   * @param[out] current_ma  Charge current in milliamperes (mA) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p current_ma is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getConstantChargeCurrent(uint32_t& current_ma) const noexcept = 0;

  /**
   * @brief     Set the constant charge voltage target (CV phase regulation voltage).
   * @details   Defines the battery voltage at which the charger transitions from
   *   CC to CV phase. Must not exceed the battery's maximum charge voltage.
   * @param[in] voltage_mv  Charge voltage target in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p voltage_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setConstantChargeVoltage(uint32_t voltage_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured constant charge voltage target.
   * @param[out] voltage_mv  Charge voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getConstantChargeVoltage(uint32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief     Set the maximum battery voltage allowed during charging.
   * @details   Acts as an absolute upper bound; the IC inhibits charging if the
   *   battery voltage exceeds this limit. Should be set at or above
   *   @ref setConstantChargeVoltage.
   * @param[in] voltage_mv  Voltage limit in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Limit applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p voltage_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setChargingVoltageLimit(uint32_t voltage_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured charging voltage limit.
   * @param[out] voltage_mv  Voltage limit in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getChargingVoltageLimit(uint32_t& voltage_mv) const noexcept = 0;

  /**
   * @brief     Set the boost (OTG) output voltage target.
   * @details   Applies only when the charger operates in OTG/boost mode,
   *   supplying power from the battery to the VBUS output.
   * @param[in] voltage_mv  Boost output voltage in millivolts (mV).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Setting applied successfully.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p voltage_mv is outside the IC's supported range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setBoostVoltage(uint32_t voltage_mv) noexcept = 0;

  /**
   * @brief      Read the currently configured boost output voltage target.
   * @param[out] voltage_mv  Boost voltage in millivolts (mV) on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Read successful; @p voltage_mv is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the charger IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getBoostVoltage(uint32_t& voltage_mv) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref ChargerEvent.
   *   Pass a default-constructed @ref ChargerCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or perform operations that disable interrupts for extended periods.
   */
  virtual void setCallback(ChargerCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — charger instances are non-copyable singletons. */
  iCharger& operator=(const iCharger&) = delete;
  /** @brief Deleted — charger instances are non-movable singletons. */
  iCharger& operator=(iCharger&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The charger event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(ChargerEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_ICHARGER_HPP_
