/**
 ******************************************************************************
 * @file    iimu.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_IMU
 * @brief   Inertial measurement unit device interface.
 *
 * @details
 *   - 3-axis accelerometer and 3-axis gyroscope as a single read operation
 *   - Optional 3-axis magnetometer (returns @ref ReturnCode::ErrorNotSupported
 *     on IMUs without an integrated compass)
 *   - Optional on-die temperature reading
 *   - Configurable measurement ranges and output data rate
 *   - Asynchronous data-ready and motion-event notification via @ref ImuCallback
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Accelerometer data is expressed in milli-g (mg); gyroscope data in
 *   milli-degrees per second (mdps); magnetometer data in milli-Gauss (mGauss).
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_IIMU_HPP_
#define HELIOS_DEV_IIMU_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// ImuAccelRange
// =============================================================================

/**
 * @enum  ImuAccelRange
 * @brief Full-scale measurement range of the accelerometer.
 * @ingroup HELIOS_DEV_IMU
 */
enum class ImuAccelRange : uint8_t
{
  G2  = 0x00U, /*!< ±2 g  — highest sensitivity, lowest noise floor. */
  G4  = 0x01U, /*!< ±4 g                                             */
  G8  = 0x02U, /*!< ±8 g                                             */
  G16 = 0x03U  /*!< ±16 g — widest range, suitable for high-g events. */
};

// =============================================================================
// ImuGyroRange
// =============================================================================

/**
 * @enum  ImuGyroRange
 * @brief Full-scale measurement range of the gyroscope.
 * @ingroup HELIOS_DEV_IMU
 */
enum class ImuGyroRange : uint8_t
{
  Dps250  = 0x00U, /*!< ±250 dps  — highest sensitivity.  */
  Dps500  = 0x01U, /*!< ±500 dps                          */
  Dps1000 = 0x02U, /*!< ±1000 dps                         */
  Dps2000 = 0x03U  /*!< ±2000 dps — widest angular range.  */
};

// =============================================================================
// ImuEvent
// =============================================================================

/**
 * @enum  ImuEvent
 * @brief Events reported via the registered @ref ImuCallback.
 * @ingroup HELIOS_DEV_IMU
 * @details Not all events are supported by every IMU; unsupported events are
 *   never generated.  Call @ref iImu::isEventSupported to query at runtime.
 */
enum class ImuEvent : uint8_t
{
  DataReady        = 0x00U, /*!< A new measurement sample is available.                    */
  MotionDetected   = 0x01U, /*!< Wake-on-motion threshold was exceeded.                   */
  FreeFallDetected = 0x02U, /*!< Free-fall condition detected (near-zero acceleration).    */
  TapDetected      = 0x03U  /*!< Single or double tap detected (shock on any axis).        */
};

// @formatter:on

// =============================================================================
// ImuData
// =============================================================================

/**
 * @struct ImuData
 * @brief  Six-axis inertial measurement sample.
 * @ingroup HELIOS_DEV_IMU
 *
 * @details All fields are fixed-point integers to avoid floating-point
 *   requirements in MISRA-critical code.
 *   - Accelerometer resolution: 1 mg = 0.001 g ≈ 9.807 mm/s²
 *   - Gyroscope resolution: 1 mdps = 0.001 °/s
 */
struct ImuData
{
  int32_t accel_x_mg;   /*!< X-axis acceleration in milli-g (mg).              */
  int32_t accel_y_mg;   /*!< Y-axis acceleration in milli-g (mg).              */
  int32_t accel_z_mg;   /*!< Z-axis acceleration in milli-g (mg).              */
  int32_t gyro_x_mdps;  /*!< X-axis angular rate in milli-degrees per second.  */
  int32_t gyro_y_mdps;  /*!< Y-axis angular rate in milli-degrees per second.  */
  int32_t gyro_z_mdps;  /*!< Z-axis angular rate in milli-degrees per second.  */
};

// =============================================================================
// ImuMagData
// =============================================================================

/**
 * @struct ImuMagData
 * @brief  Three-axis magnetometer sample.
 * @ingroup HELIOS_DEV_IMU
 */
struct ImuMagData
{
  int32_t x_mgauss; /*!< X-axis magnetic field in milli-Gauss (mGauss). */
  int32_t y_mgauss; /*!< Y-axis magnetic field in milli-Gauss (mGauss). */
  int32_t z_mgauss; /*!< Z-axis magnetic field in milli-Gauss (mGauss). */
};

// =============================================================================
// ImuCallback
// =============================================================================

/**
 * @brief Callable type invoked when an IMU event occurs.
 *
 * @details Signature: `void handler(ImuEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using ImuCallback = Callback<void(ImuEvent)>;

// =============================================================================
// iImu
// =============================================================================

/**
 * @class  iImu
 * @brief  Hardware-agnostic inertial measurement unit device interface.
 * @ingroup HELIOS_DEV_IMU
 *
 * @details
 *   - Reads 3-axis accelerometer and 3-axis gyroscope as a single sample
 *     via @ref read.
 *   - Optional magnetometer via @ref readMag; returns
 *     @ref ReturnCode::ErrorNotSupported if the IC has no compass.
 *   - Optional on-die temperature via @ref getTemperature.
 *   - Measurement ranges configured with @ref setAccelRange and @ref setGyroRange.
 *   - Output data rate controlled with @ref setOutputDataRate.
 *   - Asynchronous data-ready and motion events delivered via @ref ImuCallback.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; IMU devices are singletons owned by the BSP layer.
 */
class iImu
{
public:
  virtual ~iImu() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the IMU and verify communication.
   * @details Resets the IC to a known state, validates the device ID, and
   *   applies default measurement ranges.  Must be called before any other method.
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
   * @brief      Read the latest accelerometer and gyroscope sample.
   * @param[out] data  Populated with the current 6-axis measurement on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p data is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode read(ImuData& data) noexcept = 0;

  /**
   * @brief      Read the latest magnetometer sample.
   * @param[out] data  Populated with the current 3-axis magnetic field on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p data is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not have an integrated magnetometer.
   */
  [[nodiscard]]
  virtual ReturnCode readMag(ImuMagData& data) noexcept = 0;

  /**
   * @brief      Read the on-die temperature.
   * @param[out] temperature_dc  Temperature in tenths of °C (e.g. 253 = 25.3 °C).
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p temperature_dc is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not expose an on-die temperature sensor.
   */
  [[nodiscard]]
  virtual ReturnCode getTemperature(int32_t& temperature_dc) noexcept = 0;

  /**
   * @brief      Check whether a new measurement sample is available.
   * @param[out] ready  Set to @c true if a sample is waiting to be read.
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
   * @brief     Set the accelerometer full-scale range.
   * @param[in] range  Desired @ref ImuAccelRange.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Range applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setAccelRange(ImuAccelRange range) noexcept = 0;

  /**
   * @brief      Read the currently configured accelerometer range.
   * @param[out] range  Set to the active @ref ImuAccelRange on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p range is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getAccelRange(ImuAccelRange& range) const noexcept = 0;

  /**
   * @brief     Set the gyroscope full-scale range.
   * @param[in] range  Desired @ref ImuGyroRange.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Range applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setGyroRange(ImuGyroRange range) noexcept = 0;

  /**
   * @brief      Read the currently configured gyroscope range.
   * @param[out] range  Set to the active @ref ImuGyroRange on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p range is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getGyroRange(ImuGyroRange& range) const noexcept = 0;

  /**
   * @brief     Set the output data rate (ODR) for all active sensors.
   * @details   The IC selects the nearest supported rate; the actual rate applied
   *   can be read back with @ref getOutputDataRate.
   * @param[in] rate_hz  Desired output data rate in Hz.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : ODR applied successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam       : @p rate_hz is zero or exceeds the IC maximum.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setOutputDataRate(uint16_t rate_hz) noexcept = 0;

  /**
   * @brief      Read the currently active output data rate.
   * @param[out] rate_hz  Active ODR in Hz on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p rate_hz is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getOutputDataRate(uint16_t& rate_hz) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref ImuEvent.
   *   Pass a default-constructed @ref ImuCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(ImuCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — IMU instances are non-copyable singletons. */
  iImu& operator=(const iImu&) = delete;
  /** @brief Deleted — IMU instances are non-movable singletons. */
  iImu& operator=(iImu&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The IMU event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(ImuEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_IIMU_HPP_
