/**
 ******************************************************************************
 * @file    return_code.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.2.0
 * @date    2026-05-07
 * @ingroup HELIOS
 * @brief   Unified status codes for all Helios SDK operations.
 *
 * @details
 *   - Informational codes in [0x0000, 0x00FF] — operation succeeded or is in progress.
 *   - Error codes in [0x0100, 0xFFFF] — operation failed; subranges per subsystem:
 *     - [0x0100, 0x01FF] General errors
 *     - [0x0200, 0x02FF] Driver / communication errors
 *     - [0x0300, 0x03FF] Filesystem errors
 *     - [0x0400, 0x04FF] RTOS / synchronization errors
 *     - [0x0500, 0x05FF] Power management errors
 *     - [0x0600, 0x06FF] Network errors
 *   - All Helios APIs return @ref ReturnCode; callers should always check the result.
 *
 */

#ifndef HELIOS_API_RETURN_CODE_HPP_
#define HELIOS_API_RETURN_CODE_HPP_

// @formatter:off

namespace hel
{

// =============================================================================
// ReturnCode
// =============================================================================

/**
 * @brief   Unified status code for all Helios SDK operations.
 * @details
 *   - Codes in [0x0000, 0x00FF] are informational (non-error).
 *   - Codes in [0x0100, 0xFFFF] indicate an error condition, grouped by subsystem:
 *     - [0x0100, 0x01FF] General errors
 *     - [0x0200, 0x02FF] Driver / communication errors
 *     - [0x0300, 0x03FF] Filesystem errors
 *     - [0x0400, 0x04FF] RTOS / synchronization errors
 *     - [0x0500, 0x05FF] Power management errors
 *     - [0x0600, 0x06FF] Network errors
 *
 * @ingroup HELIOS
 */
enum class ReturnCode : unsigned short
{
    // -------------------------------------------------------------------------
    // Informational codes  [0x0000, 0x00FF]
    // -------------------------------------------------------------------------
    AnsweredRequest        = 0x0000U, /*!< Operation completed normally.                          */
    OperationIdle          = 0x0001U, /*!< No specific operation is currently running.            */
    OperationRunning       = 0x0002U, /*!< Busy with a task related to the caller's request.      */
    FunctionBusy           = 0x0003U, /*!< Busy with a task belonging to a different caller.      */
    OperationAborted       = 0x0004U, /*!< Operation was successfully aborted as requested.       */
    OperationPending       = 0x0005U, /*!< Operation has been queued and will start shortly.      */
    DataNotReady           = 0x0006U, /*!< Peripheral has no new data available yet.              */

    // -------------------------------------------------------------------------
    // General errors  [0x0100, 0x01FF]
    // -------------------------------------------------------------------------
    ErrorUnknown           = 0x0100U, /*!< Unknown error.                                         */
    ErrorNotSupported      = 0x0101U, /*!< Operation is not supported by the implementation.      */
    ErrorInvalidState      = 0x0102U, /*!< Operation cannot be performed in the current state.    */
    ErrorTimeout           = 0x0103U, /*!< Operation timed out before completion.                 */
    ErrorParam             = 0x0104U, /*!< One or more parameters are invalid.                    */
    NotInitialized         = 0x0105U, /*!< Peripheral or object has not been initialised.         */
    ErrorGeneral           = 0x0106U, /*!< Unspecified error; peripheral fault or invalid state.  */
    ErrorQueueFull         = 0x0107U, /*!< Queue or buffer is full; cannot accept new items.      */
    ErrorQueueEmpty        = 0x0108U, /*!< Queue or buffer is empty; no items to retrieve.        */
    ErrorBufferTooSmall    = 0x0109U, /*!< Provided buffer is too small for the operation.        */
    ErrorBufferTooLarge    = 0x010AU, /*!< Provided buffer is too large for the operation.        */
    ErrorWriteFailed       = 0x010BU, /*!< Write operation failed.                                */
    ErrorReadFailed        = 0x010CU, /*!< Read operation failed.                                 */
    ErrorCrc               = 0x010DU, /*!< CRC check failed; data may be corrupted.               */
    ErrorNullPointer       = 0x010EU, /*!< Null pointer where a valid pointer was expected.       */
    ErrorDeviceNotFound    = 0x010FU, /*!< Target device not found on the bus.                    */
    ErrorOutOfMemory       = 0x0110U, /*!< Insufficient memory to complete the operation.         */
    ErrorRange             = 0x0111U, /*!< Value is outside the valid range for this parameter.   */
    ErrorBusy              = 0x0112U, /*!< Resource or peripheral is busy; retry later.           */
    ErrorOverflow          = 0x0113U, /*!< Arithmetic or buffer overflow detected.                */
    ErrorAborted           = 0x0114U, /*!< Operation was cancelled due to an error condition.     */
    ErrorAlignment         = 0x0115U, /*!< Address or size does not satisfy alignment requirements. */

    // -------------------------------------------------------------------------
    // Driver / communication errors  [0x0200, 0x02FF]
    // -------------------------------------------------------------------------
    ErrorArbitrationLost   = 0x0200U, /*!< I2C bus arbitration lost to another master.            */
    ErrorNack              = 0x0201U, /*!< NACK received; target did not acknowledge.             */
    ErrorAddressNack       = 0x0202U, /*!< I2C address phase was not acknowledged.                */
    ErrorDataNack          = 0x0203U, /*!< I2C data phase was not acknowledged.                   */
    ErrorFraming           = 0x0204U, /*!< UART framing error (invalid stop bit).                 */
    ErrorParity            = 0x0205U, /*!< Parity check failed on received data.                  */
    ErrorOverrun           = 0x0206U, /*!< Receive buffer overrun; one or more bytes were lost.   */
    ErrorBusError          = 0x0207U, /*!< Bus fault detected (misplaced START/STOP or short).    */
    ErrorDmaFailed         = 0x0208U, /*!< DMA transfer did not complete successfully.            */

    // -------------------------------------------------------------------------
    // Filesystem errors  [0x0300, 0x03FF]
    // -------------------------------------------------------------------------
    ErrorFileNotFound      = 0x0300U, /*!< File or directory does not exist.                      */
    ErrorPathNotFound      = 0x0301U, /*!< Intermediate path component does not exist.            */
    ErrorFileExists        = 0x0302U, /*!< File already exists (exclusive create requested).      */
    ErrorNotADirectory     = 0x0303U, /*!< Path refers to a file, not a directory.                */
    ErrorNotAFile          = 0x0304U, /*!< Path refers to a directory, not a file.                */
    ErrorDirectoryNotEmpty = 0x0305U, /*!< Directory cannot be removed because it is not empty.   */
    ErrorDiskFull          = 0x0306U, /*!< No space left on the filesystem.                       */
    ErrorEndOfFile         = 0x0307U, /*!< Read reached the end of the file.                      */
    ErrorInvalidHandle     = 0x0308U, /*!< File or directory handle is not open or was closed.    */

    // -------------------------------------------------------------------------
    // RTOS and synchronization primitives  [0x0400, 0x04FF]
    // -------------------------------------------------------------------------
    ErrorMutexLockFailed    = 0x0400U, /*!< Failed to acquire mutex lock.                         */
    ErrorMutexUnlockFailed  = 0x0401U, /*!< Failed to release mutex lock.                         */
    ErrorSemaphoreTakeFailed = 0x0402U, /*!< Failed to take semaphore.                            */
    ErrorSemaphoreGiveFailed = 0x0403U, /*!< Failed to give semaphore.                            */
    ErrorQueueSendFailed    = 0x0404U, /*!< Failed to send item to queue.                         */
    ErrorQueueReceiveFailed = 0x0405U, /*!< Failed to receive item from queue.                    */
    ErrorThreadCreateFailed = 0x0406U, /*!< Failed to create thread.                              */
    ErrorThreadDeleteFailed = 0x0407U, /*!< Failed to delete thread.                              */
    ErrorThreadStartFailed  = 0x0408U, /*!< Failed to start thread.                               */
    ErrorThreadStopFailed   = 0x0409U, /*!< Failed to stop thread.                                */
    ErrorThreadSuspendFailed = 0x040AU, /*!< Failed to suspend thread.                            */
    ErrorThreadResumeFailed  = 0x040BU, /*!< Failed to resume thread.                             */

    // -------------------------------------------------------------------------
    // Power management  [0x0500, 0x05FF]
    // -------------------------------------------------------------------------
    ErrorOverVoltage        = 0x0500U, /*!< Voltage exceeded the safe upper limit.                */
    ErrorUnderVoltage       = 0x0501U, /*!< Voltage dropped below the safe lower limit.           */
    ErrorOverCurrent        = 0x0502U, /*!< Current exceeded the safe upper limit.                */
    ErrorOverTemperature    = 0x0503U, /*!< Temperature exceeded the safe upper limit.            */
    ErrorBatteryFault       = 0x0504U, /*!< Battery reported a fault condition.                   */
    ErrorChargeFault        = 0x0505U, /*!< Charger reported a fault condition.                   */
    ErrorPowerNotAvailable  = 0x0506U, /*!< Requested power rail or supply is not available.      */

    // -------------------------------------------------------------------------
    // Network errors  [0x0600, 0x06FF]
    // -------------------------------------------------------------------------
    ErrorNetworkDown        = 0x0600U, /*!< Network interface is down or link is not established. */
    ErrorConnectionRefused  = 0x0601U, /*!< Remote host actively refused the connection.          */
    ErrorConnectionTimeout  = 0x0602U, /*!< Connection attempt timed out.                         */
    ErrorConnectionClosed   = 0x0603U, /*!< Connection was closed by the remote side.             */
    ErrorHostNotFound       = 0x0604U, /*!< Hostname could not be resolved.                       */
    ErrorSocketFailed       = 0x0605U, /*!< Socket operation failed.                              */
};

} // namespace hel

// @formatter:on

#endif // HELIOS_API_RETURN_CODE_HPP_
