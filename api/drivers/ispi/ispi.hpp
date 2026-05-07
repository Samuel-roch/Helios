/**
 ******************************************************************************
 * @file    ispi.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_SPI
 * @brief   SPI master communication interface.
 *
 * @details
 *   - Blocking half-duplex TX, half-duplex RX, and full-duplex TX/RX with timeout
 *   - Interrupt-based asynchronous half-duplex and full-duplex transfers
 *   - DMA-based asynchronous half-duplex and full-duplex transfers
 *   - Abort control for in-progress async operations
 *   - Event notification via @ref SpiCallback on completion or error
 *   - Chip-select management is the caller's responsibility (via iGpio)
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Blocking methods must not be called from an ISR context.
 *   The async callback fires from ISR or driver-task context (implementation-defined).
 *   Assert chip-select before calling any transfer method; deassert after the
 *   callback fires (async) or the call returns (blocking).
 */

#ifndef HELIOS_DRV_ISPI_HPP_
#define HELIOS_DRV_ISPI_HPP_

#include <hel_target>
#include <hel_bytearray>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @brief Callable type used to receive asynchronous SPI events.
 *
 * @details Signature: `void handler(ReturnCode code, uint16_t count) noexcept`
 *   - @p code   — operation result (@ref ReturnCode::AnsweredRequest on success, error otherwise).
 *   - @p count  — bytes transferred (0 on error).
 */
using SpiCallback = Callback<void(ReturnCode, uint16_t)>;

/**
 * @class  iSpi
 * @brief  Hardware-agnostic SPI master communication interface.
 * @ingroup HELIOS_DRV_SPI
 *
 * @details
 *   - Blocking half-duplex TX/RX and full-duplex TX+RX via @ref write, @ref read,
 *     and @ref writeRead
 *   - Interrupt async via @ref writeInterrupt, @ref readInterrupt, @ref writeReadInterrupt
 *   - DMA async via @ref writeDMA, @ref readDMA, @ref writeReadDMA
 *   - Abort control via @ref abort
 *   - Single event callback registered via @ref setCallback; fires from ISR or
 *     driver-task context (implementation-defined)
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Blocking methods must not be called from an ISR context.
 *        Chip-select is not managed by this interface; use @ref iGpio for CS control.
 *        Copy and move are deleted; SPI peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iSpi
{
public:
  virtual ~iSpi() noexcept = default;

  // -------------------------------------------------------------------------
  // Blocking transfers
  // -------------------------------------------------------------------------

  /**
   * @brief      Transmit data (half-duplex) and block until complete or timeout.
   * @details    Received bytes during transmission are discarded.
   * @param[in]  data        View of bytes to transmit.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes transmitted successfully.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; no bytes were consumed.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; object state remains consistent.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode write(
    ByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Receive data (half-duplex) and block until complete or timeout.
   * @details    Dummy bytes are clocked out during reception (implementation-defined value).
   * @param[out] data        Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes written to @p data.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; buffer content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; buffer content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode read(
    ByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Perform a full-duplex transfer and block until complete or timeout.
   * @details    @p tx_data and @p rx_data must have the same size; bytes are clocked
   *             simultaneously in both directions.
   * @param[in]  tx_data     View of bytes to transmit.
   * @param[out] rx_data     Writable view to store received bytes (same size as @p tx_data).
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer complete; @p rx_data is valid.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; @p rx_data content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorParam      : @p tx_data and @p rx_data sizes differ.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p rx_data content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode writeRead(
    ByteArray tx_data, ByteArray rx_data, uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous interrupt transfers
  // -------------------------------------------------------------------------

  /**
   * @brief     Transmit data (half-duplex) asynchronously using interrupts.
   * @param[in] data    View of bytes to transmit.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode writeInterrupt(ByteArray data) noexcept = 0;

  /**
   * @brief      Receive data (half-duplex) asynchronously using interrupts.
   * @param[out] data    Writable view to store received bytes.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode readInterrupt(ByteArray data) noexcept = 0;

  /**
   * @brief      Perform a full-duplex transfer asynchronously using interrupts.
   * @param[in]  tx_data    View of bytes to transmit.
   * @param[out] rx_data    Writable view to store received bytes (same size as @p tx_data).
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorParam      : @p tx_data and @p rx_data sizes differ.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode writeReadInterrupt(
    ByteArray tx_data, ByteArray rx_data) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous DMA transfers
  // -------------------------------------------------------------------------

  /**
   * @brief     Transmit data (half-duplex) asynchronously using DMA.
   * @param[in] data    View of bytes to transmit; must remain valid until the callback
   *                    fires or @ref abort is called.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode writeDMA(ByteArray data) noexcept = 0;

  /**
   * @brief      Receive data (half-duplex) asynchronously using DMA.
   * @param[out] data    Writable view to store received bytes; must remain valid until
   *                     the callback fires or @ref abort is called.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode readDMA(ByteArray data) noexcept = 0;

  /**
   * @brief      Perform a full-duplex transfer asynchronously using DMA.
   * @param[in]  tx_data    View of bytes to transmit; must remain valid until the
   *                        callback fires or @ref abort is called.
   * @param[out] rx_data    Writable view to store received bytes (same size as @p tx_data);
   *                        must remain valid until the callback fires or @ref abort is called.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorParam      : @p tx_data and @p rx_data sizes differ.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref SpiCallback.
   */
  [[nodiscard]] virtual ReturnCode writeReadDMA(
    ByteArray tx_data, ByteArray rx_data) noexcept = 0;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous transfer.
   * @details The registered @ref SpiCallback is invoked after the peripheral stops;
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
   * @param[in] callback  Callable invoked on every async event (@ref SpiCallback).
   *                      Pass a default-constructed @ref SpiCallback to clear.
   * @note  Safe to call at any time; takes effect for the next async operation.
   */
  virtual void setCallback(SpiCallback callback) noexcept = 0;

protected:
  iSpi& operator=(const iSpi&) = delete;
  iSpi& operator=(iSpi&&) = delete;

  /**
   * @brief     Internal handler for SPI events, called by the driver/ISR.
   * @param[in] code   Result code for the event that occurred.
   * @param[in] count  Bytes transferred (0 on error).
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleEvent(ReturnCode code, uint16_t count) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_ISPI_HPP_
