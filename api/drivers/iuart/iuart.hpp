/**
 ******************************************************************************
 * @file    iuart.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_DRV_UART
 * @brief   UART serial communication interface.
 *
 * @details
 *   - Blocking TX/RX with millisecond timeout
 *   - Interrupt and DMA asynchronous transfers with configurable @ref UartMode
 *   - Abort control for in-progress async operations
 *   - Event notification via @ref UartCallback
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Blocking methods must not be called from an ISR context.
 *   Async methods may be called from task context; the callback fires from ISR
 *   or driver-task context (implementation-defined).
 */

#ifndef HELIOS_DRV_IUART_HPP_
#define HELIOS_DRV_IUART_HPP_

#include <hel_target>
#include <hel_bytearray>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

/**
 * @enum  UartMode
 * @brief Selects the DMA/interrupt transfer mode for async UART operations.
 * @ingroup HELIOS_DRV_UART
 */
enum class UartMode : uint8_t
{
  SingleShot        = 0x00U, /*!< Single-shot transfer; callback fires on completion. */
  InterruptToIdle   = 0x01U, /*!< Interrupt-driven transfer; callback fires on completion. */
  InterruptCircular = 0x02U, /*!< Continuous circular interrupt-driven transfer; callback fires on half/full completion. */
  DMAToIdle         = 0x03U, /*!< DMA transfer with idle detection; callback fires on completion or idle. */
  DMACircular       = 0x04U, /*!< Continuous circular DMA; callback fires on half/full completion. */
};

/**
 * @enum  UartEvent
 * @brief UART events reported via the registered @ref UartCallback.
 * @ingroup HELIOS_DRV_UART
 */
enum class UartEvent : uint8_t
{
  TxComplete        = 0x00U, /*!< Transmission completed successfully. */
  TxHalfComplete    = 0x01U, /*!< Transmission half-complete (circular/DMA). */
  RxComplete        = 0x02U, /*!< Reception completed successfully. */
  RxHalfComplete    = 0x03U, /*!< Reception half-complete (circular/DMA). */
  TxRxComplete      = 0x04U, /*!< Simultaneous Tx/Rx completed. */
  TxRxHalfComplete  = 0x05U, /*!< Simultaneous Tx/Rx half-complete. */
  AbortComplete     = 0x06U, /*!< Requested abort completed. */
  ErrorOverrun      = 0x07U, /*!< Overrun error: data lost before read. */
  ErrorParity       = 0x08U, /*!< Parity error detected. */
  ErrorFraming      = 0x09U, /*!< Framing error detected. */
  ErrorNoise        = 0x0AU, /*!< Noise error detected. */
  ErrorBreak        = 0x0BU, /*!< Break condition detected on line. */
  ErrorGeneral      = 0x0CU, /*!< Unspecified hardware error. */
  AbortError        = 0x0DU, /*!< Abort triggered by an error condition. */
};

// @formatter:on

/**
 * @brief Callable type used to receive asynchronous UART events.
 *
 * @details Signature: `void handler(UartEvent event, uint16_t count) noexcept`
 *   - @p event  — event that occurred (see @ref UartEvent, e.g. @ref UartEvent::TxComplete,
 *                 @ref UartEvent::RxComplete, @ref UartEvent::AbortComplete).
 *   - @p count  — bytes transferred (0 for error/abort events).
 */
using UartCallback = Callback<void(UartEvent, uint16_t)>;

/**
 * @brief Default blocking timeout for UART transfers in milliseconds.
 */
constexpr uint32_t kUartDefaultTimeoutMs = 1000U;  /*!< Default blocking timeout for UART transfers. */

/**
 * @class  iUart
 * @brief  Hardware-agnostic UART/USART serial communication interface.
 * @ingroup HELIOS_DRV_UART
 *
 * @details
 *   - Blocking TX/RX with configurable millisecond timeout
 *   - Interrupt and DMA async transfers via @ref writeInterrupt, @ref readInterrupt,
 *     @ref writeDMA, and @ref readDMA
 *   - Abort control for in-progress async operations (@ref abortWrite, @ref abortRead)
 *   - Single event callback registered via @ref setCallback; fires from ISR or
 *     driver-task context (implementation-defined)
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Blocking methods must not be called from an ISR context.
 *        Copy and move are deleted; UART peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iUart
{
public:
  virtual ~iUart() noexcept = default;

  // -------------------------------------------------------------------------
  // Blocking transfers
  // -------------------------------------------------------------------------

  /**
   * @brief      Transmit data and block until complete or timeout.
   * @details
   *   - Blocks the calling task until all bytes are sent or @p timeout_ms elapses.
   *   - The source buffer is not modified.
   *
   * @param[in]  data        Read-only view of bytes to transmit.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds; 0 returns immediately.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes transmitted successfully.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; no bytes were consumed.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; no bytes were consumed.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; object state remains consistent.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]]
  virtual ReturnCode write(ConstByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Receive exactly `data.size()` bytes, blocking until done or timeout.
   * @param[out] data        Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes written to @p data.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; buffer content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; no bytes were written.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; buffer content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]]
  virtual ReturnCode read(ByteArray data, uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous transfers (interrupt / DMA)
  // -------------------------------------------------------------------------

  /**
   * @brief      Start an interrupt-driven transmission; returns immediately.
   * @details    Completion is reported via the registered @ref UartCallback
   *             with @ref UartEvent::TxComplete (or @ref UartEvent::TxHalfComplete
   *             for @ref UartMode::InterruptCircular, once per half).
   *
   * @param[in]  data  Read-only view of bytes to transmit; must remain valid
   *                   until the callback fires.
   * @param[in]  mode  Transfer mode; must be one of @ref UartMode::SingleShot,
   *                   @ref UartMode::InterruptToIdle, or @ref UartMode::InterruptCircular.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback will fire.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   */
  [[nodiscard]]
  virtual ReturnCode writeInterrupt(ConstByteArray data, UartMode mode) noexcept = 0;

  /**
   * @brief      Start an interrupt-driven reception; returns immediately.
   * @details    Completion is reported via the registered @ref UartCallback
   *             with @ref UartEvent::RxComplete (or @ref UartEvent::RxHalfComplete
   *             for @ref UartMode::InterruptCircular, once per half).
   *
   * @param[out] data  Writable view to store received bytes; must remain valid
   *                   until the callback fires.
   * @param[in]  mode  Transfer mode; must be one of @ref UartMode::SingleShot,
   *                   @ref UartMode::InterruptToIdle, or @ref UartMode::InterruptCircular.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback will fire.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   */
  [[nodiscard]]
  virtual ReturnCode readInterrupt(ByteArray data, UartMode mode) noexcept = 0;

  /**
   * @brief      Start a DMA transmission; returns immediately.
   * @details    Completion is reported via the registered @ref UartCallback
   *             with @ref UartEvent::TxComplete (or @ref UartEvent::TxHalfComplete
   *             for @ref UartMode::DMACircular, once per half).
   *
   * @param[in]  data  Read-only view of bytes to transmit; must remain valid
   *                   until the callback fires — the DMA controller reads
   *                   directly from it after the call returns.
   * @param[in]  mode  Transfer mode; must be one of @ref UartMode::SingleShot,
   *                   @ref UartMode::DMAToIdle, or @ref UartMode::DMACircular.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback will fire.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   */
  [[nodiscard]]
  virtual ReturnCode writeDMA(ConstByteArray data, UartMode mode) noexcept = 0;

  /**
   * @brief      Start a DMA reception; returns immediately.
   * @details    Completion is reported via the registered @ref UartCallback
   *             with @ref UartEvent::RxComplete (or @ref UartEvent::RxHalfComplete
   *             for @ref UartMode::DMACircular, once per half); idle-line
   *             detection under @ref UartMode::DMAToIdle also reports
   *             @ref UartEvent::RxComplete with the byte count received so far.
   *
   * @param[out] data  Writable view to store received bytes; must remain valid
   *                   until the callback fires — the DMA controller writes
   *                   directly into it after the call returns.
   * @param[in]  mode  Transfer mode; must be one of @ref UartMode::SingleShot,
   *                   @ref UartMode::DMAToIdle, or @ref UartMode::DMACircular.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Transfer started; callback will fire.
   *   - @ref ReturnCode::FunctionBusy   : A transfer is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   */
  [[nodiscard]]
  virtual ReturnCode readDMA(ByteArray data, UartMode mode) noexcept = 0;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous transmission.
   * @details The registered @ref UartCallback is invoked after the peripheral stops with
   *          @ref UartEvent::AbortComplete on success or @ref UartEvent::AbortError on fault;
   *          the byte count delivered is implementation-defined.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Abort issued; callback will fire.
   *   - @ref ReturnCode::NotInitialized  : No active TX transfer; nothing aborted.
   */
  [[nodiscard]]
  virtual ReturnCode abortWrite() noexcept = 0;

  /**
   * @brief  Abort an ongoing asynchronous reception.
   * @details The registered @ref UartCallback is invoked after the peripheral stops with
   *          @ref UartEvent::AbortComplete on success or @ref UartEvent::AbortError on fault;
   *          the byte count delivered is implementation-defined.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Abort issued; callback will fire.
   *   - @ref ReturnCode::NotInitialized  : No active RX transfer; nothing aborted.
   */
  [[nodiscard]]
  virtual ReturnCode abortRead() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback and state
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the asynchronous event callback.
   * @param[in] callback  Callable invoked on every async event (@ref UartCallback).
   *                      Pass a default-constructed @ref UartCallback to clear.
   * @note  Safe to call at any time; takes effect for the next async operation.
   */
  virtual void setCallback(UartCallback callback) noexcept = 0;

  /**
   * @brief     Internal handler for UART events, called by the driver or ISR.
   * @details   Invokes the registered @ref UartCallback with the event and byte count.
   *            Implementations must not block, allocate memory, or throw.
   *
   * @param[in] event  UART event that occurred (see @ref UartEvent).
   * @param[in] count  Bytes transferred; 0 for error and abort events.
   * @warning   Must only be called from the driver implementation or ISR context.
   *            Not part of the public application API.
   */
  virtual void handleEvent(UartEvent event, uint16_t count) noexcept = 0;

protected:
  iUart& operator=(const iUart&) = delete;
  iUart& operator=(iUart&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_IUART_HPP_
