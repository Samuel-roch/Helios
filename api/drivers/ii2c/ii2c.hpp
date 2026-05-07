/**
 ******************************************************************************
 * @file    ii2c.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_I2C
 * @brief   I2C master communication interface.
 *
 * @details
 *   - Blocking master TX, RX, and combined write-then-read with millisecond timeout
 *   - Interrupt-based asynchronous master TX, RX, and write-then-read
 *   - Abort control for in-progress async operations
 *   - Event notification via @ref I2cCallback on completion or error
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Blocking methods must not be called from an ISR context.
 *   The async callback fires from ISR or driver-task context (implementation-defined).
 *   7-bit and 10-bit slave addresses are both expressed as `uint16_t`;
 *   the implementation is responsible for encoding the address correctly.
 */

#ifndef HELIOS_DRV_II2C_HPP_
#define HELIOS_DRV_II2C_HPP_

#include <hel_target>
#include <hel_bytearray>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @brief Callable type used to receive asynchronous I2C events.
 *
 * @details Signature: `void handler(ReturnCode code, uint16_t count) noexcept`
 *   - @p code   — operation result (@ref ReturnCode::AnsweredRequest on success, error otherwise).
 *   - @p count  — bytes transferred (0 on error).
 */
using I2cCallback = Callback<void(ReturnCode, uint16_t)>;

/**
 * @class  iI2c
 * @brief  Hardware-agnostic I2C master communication interface.
 * @ingroup HELIOS_DRV_I2C
 *
 * @details
 *   - Blocking TX/RX/write-then-read with configurable millisecond timeout
 *   - Interrupt-based async TX, RX, and write-then-read via @ref writeInterrupt,
 *     @ref readInterrupt, and @ref writeReadInterrupt
 *   - Abort control via @ref abort
 *   - Single event callback registered via @ref setCallback; fires from ISR or
 *     driver-task context (implementation-defined)
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Blocking methods must not be called from an ISR context.
 *        Copy and move are deleted; I2C peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iI2c
{
public:
  virtual ~iI2c() noexcept = default;


  // -------------------------------------------------------------------------
  // Blocking transfers
  // -------------------------------------------------------------------------

  /**
   * @brief      Transmit data to a slave and block until complete or timeout.
   * @param[in]  addr        Slave address (7-bit or 10-bit, unshifted).
   * @param[in]  data        View of bytes to transmit.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes transmitted successfully.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; no bytes were consumed.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Bus or peripheral fault; object state remains consistent.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode write(
    uint16_t addr, ByteArray& data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Receive data from a slave and block until complete or timeout.
   * @param[in]  addr        Slave address (7-bit or 10-bit, unshifted).
   * @param[out] data        Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes written to @p data.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; buffer content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Bus or peripheral fault; buffer content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode read(
    uint16_t addr, ByteArray& data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Perform a combined write-then-read (register-read pattern).
   * @details    Issues a write of @p tx_data followed immediately by a repeated
   *             START and a read into @p rx_data, all within a single I2C transaction.
   * @param[in]  addr        Slave address (7-bit or 10-bit, unshifted).
   * @param[in]  tx_data     View of bytes to transmit (e.g. register address).
   * @param[out] rx_data     Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transaction complete; @p rx_data is valid.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; @p rx_data content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Bus or peripheral fault; @p rx_data content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode writeRead(
    uint16_t addr, ByteArray& tx_data, ByteArray& rx_data,
    uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous transfers
  // -------------------------------------------------------------------------

  /**
   * @brief     Transmit data to a slave asynchronously using interrupts.
   * @param[in] addr    Slave address (7-bit or 10-bit, unshifted).
   * @param[in] data    View of bytes to transmit.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref I2cCallback.
   */
  [[nodiscard]] virtual ReturnCode writeInterrupt(
    uint16_t addr, ByteArray& data) noexcept = 0;

  /**
   * @brief      Receive data from a slave asynchronously using interrupts.
   * @param[in]  addr    Slave address (7-bit or 10-bit, unshifted).
   * @param[out] data    Writable view to store received bytes.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref I2cCallback.
   */
  [[nodiscard]] virtual ReturnCode readInterrupt(
    uint16_t addr, ByteArray& data) noexcept = 0;

  /**
   * @brief      Perform an asynchronous combined write-then-read using interrupts.
   * @param[in]  addr      Slave address (7-bit or 10-bit, unshifted).
   * @param[in]  tx_data   View of bytes to transmit (e.g. register address).
   * @param[out] rx_data   Writable view to store received bytes.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref I2cCallback.
   */
  [[nodiscard]] virtual ReturnCode writeReadInterrupt(
    uint16_t addr, ByteArray& tx_data, ByteArray& rx_data) noexcept = 0;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous transfer.
   * @details The registered @ref I2cCallback is invoked after the peripheral stops;
   *          the byte count delivered is implementation-defined.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Abort issued; callback will fire.
   *   - @ref ReturnCode::NotInitialized  : No active transfer; nothing aborted.
   */
  [[nodiscard]] virtual ReturnCode abort() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the asynchronous event callback.
   * @param[in] callback  Callable invoked on every async event (@ref I2cCallback).
   *                      Pass a default-constructed @ref I2cCallback to clear.
   * @note  Safe to call at any time; takes effect for the next async operation.
   */
  virtual void setCallback(I2cCallback callback) noexcept = 0;

protected:
  iI2c& operator=(const iI2c&) = delete;
  iI2c& operator=(iI2c&&) = delete;

  /**
   * @brief     Internal handler for I2C events, called by the driver/ISR.
   * @param[in] code   Result code for the event that occurred.
   * @param[in] count  Bytes transferred (0 on error).
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleEvent(ReturnCode code, uint16_t count) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_II2C_HPP_
