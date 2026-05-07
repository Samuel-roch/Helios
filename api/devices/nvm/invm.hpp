/**
 ******************************************************************************
 * @file    invm.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_NVM
 * @brief   Non-volatile memory device interface.
 *
 * @details
 *   - Unified abstraction over NOR Flash, NAND Flash, and EEPROM
 *   - Blocking read, write, and erase with configurable timeout
 *   - Asynchronous erase and write operations via @ref NvmCallback
 *   - Device capability discovery through @ref NvmInfo
 *   - No dynamic allocation; implementations map each method to the underlying
 *     memory IC or controller
 *
 * @note
 *   All addresses are byte-addressed from the start of the device.
 *   Erase operations must complete before writing to the same region;
 *   writing without prior erase produces undefined data on Flash devices.
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_INVM_HPP_
#define HELIOS_DEV_INVM_HPP_

#include <hel_bytearray>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// NvmType
// =============================================================================

/**
 * @enum  NvmType
 * @brief Physical memory technology of the NVM device.
 * @ingroup HELIOS_DEV_NVM
 */
enum class NvmType : uint8_t
{
  Unknown  = 0x00U, /*!< Memory type could not be determined.                          */
  Nor      = 0x01U, /*!< NOR Flash — byte/word random read, sector or block erase.    */
  Nand     = 0x02U, /*!< NAND Flash — page-based read/write, block erase.             */
  Eeprom   = 0x03U  /*!< EEPROM — byte-addressable read/write, no explicit erase step. */
};

// =============================================================================
// NvmEvent
// =============================================================================

/**
 * @enum  NvmEvent
 * @brief Events reported via the registered @ref NvmCallback.
 * @ingroup HELIOS_DEV_NVM
 */
enum class NvmEvent : uint8_t
{
  WriteComplete  = 0x00U, /*!< Asynchronous write operation completed successfully.  */
  EraseComplete  = 0x01U, /*!< Asynchronous erase operation completed successfully.  */
  ErrorWrite     = 0x02U, /*!< Asynchronous write operation failed.                 */
  ErrorErase     = 0x03U  /*!< Asynchronous erase operation failed.                 */
};

// @formatter:on

// =============================================================================
// NvmInfo
// =============================================================================

/**
 * @struct NvmInfo
 * @brief  Static capability descriptor for a NVM device.
 * @ingroup HELIOS_DEV_NVM
 *
 * @details Populated by @ref iNvm::getInfo and used by the caller to determine
 *   valid address ranges and alignment requirements before issuing operations.
 */
struct NvmInfo
{
  NvmType  type;               /*!< Physical memory technology.                                 */
  uint32_t capacity;           /*!< Total device capacity in bytes.                             */
  uint32_t page_size;          /*!< Read/write page size in bytes (1 for EEPROM/NOR byte mode). */
  uint32_t erase_block_size;   /*!< Smallest erasable unit in bytes (0 if erase is not needed). */
  uint32_t write_granularity;  /*!< Minimum number of bytes per write operation.                */
};

// =============================================================================
// NvmCallback
// =============================================================================

/**
 * @brief Callable type invoked when an asynchronous NVM operation completes.
 *
 * @details Signature: `void handler(NvmEvent event) noexcept`
 *   - @p event — the event that triggered the callback.
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using NvmCallback = Callback<void(NvmEvent)>;

// =============================================================================
// iNvm
// =============================================================================

/**
 * @class  iNvm
 * @brief  Hardware-agnostic non-volatile memory device interface.
 * @ingroup HELIOS_DEV_NVM
 *
 * @details
 *   - Covers NOR Flash, NAND Flash, and EEPROM through a single API.
 *   - Blocking operations (@ref read, @ref write, @ref eraseBlock, @ref eraseChip)
 *     wait for the device to acknowledge completion or timeout.
 *   - Asynchronous operations (@ref writeAsync, @ref eraseBlockAsync) return
 *     immediately; completion is signalled via the @ref NvmCallback.
 *   - Call @ref getInfo to determine the device geometry before accessing memory.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; NVM devices are singletons owned by the BSP layer.
 */
class iNvm
{
public:
  virtual ~iNvm() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the NVM device and verify communication.
   * @details Configures the peripheral to a known state and confirms the device
   *   is accessible. Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : Device did not respond.
   *   - @ref ReturnCode::ErrorReadFailed     : Communication with the device failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  /**
   * @brief      Retrieve the static capability descriptor for this device.
   * @param[out] info  Populated with the device geometry on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p info is valid and populated.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode getInfo(NvmInfo& info) const noexcept = 0;

  /**
   * @brief      Check whether the device is currently executing an operation.
   * @param[out] busy  Set to @c true while a program/erase cycle is in progress.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p busy reflects the current device state.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Status register could not be read.
   */
  [[nodiscard]]
  virtual ReturnCode isBusy(bool& busy) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Blocking transfers
  // -------------------------------------------------------------------------

  /**
   * @brief      Read bytes from the device into a buffer.
   * @param[in]  address     Byte-addressed start offset within the device.
   * @param[out] buffer      Destination buffer; must be at least as large as the
   *   requested length.
   * @param[in]  timeout_ms  Maximum time to wait for the device in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : All bytes read successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam       : @p address or @p buffer.size() is out of range.
   *   - @ref ReturnCode::ErrorReadFailed  : Communication or device error.
   *   - @ref ReturnCode::ErrorTimeout     : Device did not respond within @p timeout_ms.
   *   - @ref ReturnCode::FunctionBusy     : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode read(uint32_t address, ByteArray buffer, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief     Write bytes from a buffer to the device.
   * @details   For Flash devices the target region must be erased before writing.
   *   Partial-page writes are allowed where supported by the IC; the implementation
   *   is responsible for any required read-modify-write if the hardware enforces
   *   page alignment.
   * @param[in] address     Byte-addressed start offset within the device.
   * @param[in] data        Source buffer; data is not modified.
   * @param[in] timeout_ms  Maximum time to wait for the device in milliseconds.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : All bytes written successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam       : @p address or @p data.size() is out of range.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication or device error.
   *   - @ref ReturnCode::ErrorTimeout     : Device did not respond within @p timeout_ms.
   *   - @ref ReturnCode::FunctionBusy     : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode write(uint32_t address, ConstByteArray data, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief     Erase the block that contains the given address.
   * @details   The erase granularity is reported by @ref NvmInfo::erase_block_size.
   *   The entire block containing @p address is erased regardless of the offset
   *   within the block.  For EEPROM devices that do not require an explicit erase
   *   step, the implementation may return @ref ReturnCode::ErrorNotSupported.
   * @param[in] address     Any byte address within the block to erase.
   * @param[in] timeout_ms  Maximum time to wait for the erase to complete.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Block erased successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p address is out of device range.
   *   - @ref ReturnCode::ErrorWriteFailed  : Erase command or verification failed.
   *   - @ref ReturnCode::ErrorTimeout      : Erase did not complete within @p timeout_ms.
   *   - @ref ReturnCode::ErrorNotSupported : Device does not require an explicit erase step.
   *   - @ref ReturnCode::FunctionBusy      : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode eraseBlock(uint32_t address, uint32_t timeout_ms) noexcept = 0;

  /**
   * @brief     Erase the entire device.
   * @details   All bytes are set to the erased state (0xFF for Flash, 0xFF or 0x00
   *   depending on the IC for EEPROM).  This is a destructive, irreversible operation.
   *   For EEPROM devices that do not require an explicit erase step, the
   *   implementation may return @ref ReturnCode::ErrorNotSupported.
   * @param[in] timeout_ms  Maximum time to wait for the erase to complete.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Chip erased successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed  : Erase command or verification failed.
   *   - @ref ReturnCode::ErrorTimeout      : Erase did not complete within @p timeout_ms.
   *   - @ref ReturnCode::ErrorNotSupported : Device does not require an explicit erase step.
   *   - @ref ReturnCode::FunctionBusy      : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode eraseChip(uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous operations
  // -------------------------------------------------------------------------

  /**
   * @brief     Begin an asynchronous write operation.
   * @details   Returns immediately. When the write completes (or fails) the
   *   registered @ref NvmCallback is invoked with @ref NvmEvent::WriteComplete
   *   or @ref NvmEvent::ErrorWrite respectively.
   *
   * @warning   The memory referenced by @p data must remain valid and unmodified
   *   until the callback fires; the implementation may read from it after this
   *   call returns.
   *
   * @param[in] address  Byte-addressed start offset within the device.
   * @param[in] data     Source buffer; caller must preserve it until the callback.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Async write started successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam       : @p address or @p data.size() is out of range.
   *   - @ref ReturnCode::FunctionBusy     : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode writeAsync(uint32_t address, ConstByteArray data) noexcept = 0;

  /**
   * @brief     Begin an asynchronous block erase operation.
   * @details   Returns immediately. When the erase completes (or fails) the
   *   registered @ref NvmCallback is invoked with @ref NvmEvent::EraseComplete
   *   or @ref NvmEvent::ErrorErase respectively.
   * @param[in] address  Any byte address within the block to erase.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Async erase started successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam        : @p address is out of device range.
   *   - @ref ReturnCode::ErrorNotSupported : Device does not require an explicit erase step.
   *   - @ref ReturnCode::FunctionBusy      : A previous async operation has not yet completed.
   */
  [[nodiscard]]
  virtual ReturnCode eraseBlockAsync(uint32_t address) noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous operation callback.
   * @param[in] callback  Callable invoked on each @ref NvmEvent.
   *   Pass a default-constructed @ref NvmCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(NvmCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — NVM instances are non-copyable singletons. */
  iNvm& operator=(const iNvm&) = delete;
  /** @brief Deleted — NVM instances are non-movable singletons. */
  iNvm& operator=(iNvm&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The NVM event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(NvmEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_INVM_HPP_
