/**
 ******************************************************************************
 * @file    iqspi.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_QSPI
 * @brief   Quad-SPI flash memory interface.
 *
 * @details
 *   - Blocking read, program, and erase operations with millisecond timeout
 *   - DMA-based asynchronous read and program
 *   - Sector, block, and full-chip erase via @ref EraseType
 *   - Abort control for in-progress async operations
 *   - Event notification via @ref QspiCallback on DMA completion or error
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Erase operations are significantly slower than read/program; plan timeouts accordingly.
 *   Blocking methods must not be called from an ISR context.
 *   The DMA callback fires from ISR or driver-task context (implementation-defined).
 */

#ifndef HELIOS_DRV_IQSPI_HPP_
#define HELIOS_DRV_IQSPI_HPP_

#include <hel_target>
#include <hel_bytearray>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @enum  EraseType
 * @brief Granularity of an erase operation on the external flash device.
 * @ingroup HELIOS_DRV_QSPI
 */
enum class EraseType : uint8_t
{
  Sector = 0x00U, /*!< Erase the smallest erasable unit (typically 4 KB).   */
  Block  = 0x01U, /*!< Erase a larger block (typically 32 KB or 64 KB).     */
  Chip   = 0x02U  /*!< Erase the entire flash device (address is ignored).  */
};

/**
 * @brief Callable type used to receive asynchronous QSPI events.
 *
 * @details Signature: `void handler(ReturnCode code, uint32_t count) noexcept`
 *   - @p code   — operation result (@ref ReturnCode::AnsweredRequest on success, error otherwise).
 *   - @p count  — bytes transferred (0 for erase or error events).
 */
using QspiCallback = Callback<void(ReturnCode, uint32_t)>;

/**
 * @class  iQspi
 * @brief  Hardware-agnostic Quad-SPI flash memory interface.
 * @ingroup HELIOS_DRV_QSPI
 *
 * @details
 *   - Blocking read, program, and erase via @ref read, @ref write, and @ref erase
 *   - DMA-based async read and program via @ref readDMA and @ref writeDMA
 *   - Abort control via @ref abort
 *   - Single event callback registered via @ref setCallback; fires from ISR or
 *     driver-task context (implementation-defined)
 *   - Flash addresses are byte-addressed; the valid range is device-specific
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Blocking methods must not be called from an ISR context.
 *        Copy and move are deleted; QSPI peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iQspi
{
public:
  virtual ~iQspi() noexcept = default;

  // -------------------------------------------------------------------------
  // Blocking operations
  // -------------------------------------------------------------------------

  /**
   * @brief      Read bytes from the flash device and block until done or timeout.
   * @param[in]  addr        Byte address in flash (device-specific range).
   * @param[out] data        Writable view to store the read bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes written to @p data.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; buffer content is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : An operation is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral or device fault; buffer content is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode read(
    uint32_t addr, ByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Program (write) bytes to the flash device and block until done or timeout.
   * @details    The target flash pages must be erased before programming; writing to
   *             non-erased pages yields undefined data.
   * @param[in]  addr        Byte address in flash (must be page-aligned; device-specific).
   * @param[in]  data        View of bytes to program.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : All bytes programmed successfully.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; flash state is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : An operation is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral or device fault.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode write(
    uint32_t addr, ConstByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief      Erase a region of the flash device and block until done or timeout.
   * @details    Erase duration varies significantly by @p type; plan @p timeout_ms accordingly.
   *             For @ref EraseType::Chip, @p addr is ignored.
   * @param[in]  addr        Byte address of the sector or block to erase.
   * @param[in]  type        Erase granularity (see @ref EraseType).
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Erase complete; region is ready for programming.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; flash state is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : An operation is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral or device fault.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]] virtual ReturnCode erase(
    uint32_t addr, EraseType type, uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous DMA operations
  // -------------------------------------------------------------------------

  /**
   * @brief     Read bytes from the flash device asynchronously using DMA.
   * @param[in] addr  Byte address in flash (device-specific range).
   * @param[out] data  Writable view to store the read bytes; must remain valid until
   *                   the callback fires or @ref abort is called.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA read started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : An operation is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref QspiCallback.
   */
  [[nodiscard]] virtual ReturnCode readDMA(
    uint32_t addr, ByteArray data) noexcept = 0;

  /**
   * @brief     Program bytes to the flash device asynchronously using DMA.
   * @details   The target flash pages must be erased before programming.
   * @param[in] addr  Byte address in flash (must be page-aligned; device-specific).
   * @param[in] data  View of bytes to program; must remain valid until the callback
   *                  fires or @ref abort is called.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA program started; callback fires on completion.
   *   - @ref ReturnCode::FunctionBusy   : An operation is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref QspiCallback.
   */
  [[nodiscard]] virtual ReturnCode writeDMA(
    uint32_t addr, ConstByteArray data) noexcept = 0;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous DMA operation.
   * @details The registered @ref QspiCallback is invoked after the peripheral stops;
   *          flash state after an aborted write or erase is unspecified.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Abort issued; callback will fire.
   *   - @ref ReturnCode::NotInitialized  : No active DMA operation; nothing aborted.
   */
  [[nodiscard]] virtual ReturnCode abort() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the asynchronous event callback.
   * @param[in] callback  Callable invoked on every DMA event (@ref QspiCallback).
   *                      Pass a default-constructed @ref QspiCallback to clear.
   * @note  Safe to call at any time; takes effect for the next async operation.
   */
  virtual void setCallback(QspiCallback callback) noexcept = 0;

protected:
  iQspi& operator=(const iQspi&) = delete;
  iQspi& operator=(iQspi&&) = delete;

  /**
   * @brief     Internal handler for QSPI events, called by the driver/ISR.
   * @param[in] code   Result code for the event that occurred.
   * @param[in] count  Bytes transferred (0 for erase or error events).
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleEvent(ReturnCode code, uint32_t count) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_IQSPI_HPP_
