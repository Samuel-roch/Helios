/**
 ******************************************************************************
 * @file    istream.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_LIB_STREAM
 * @brief   Abstract byte stream interface — transport-agnostic sequential I/O.
 *
 * @details
 *   - Models a bidirectional sequential byte channel.
 *   - Implementations that support only one direction return
 *     @ref ReturnCode::ErrorNotSupported for the unsupported direction.
 *   - Asynchronous data-ready and write-complete events are delivered via
 *     @ref StreamCallback.
 *   - No dynamic allocation; all buffers are caller-owned.
 *
 */

#ifndef HELIOS_LIB_ISTREAM_HPP_
#define HELIOS_LIB_ISTREAM_HPP_

#include <cstdint>
#include <hel_return_code>
#include <hel_bytearray>
#include <hel_callback>

namespace hel
{

// =============================================================================
// StreamEvent
// =============================================================================

/**
 * @enum  StreamEvent
 * @brief Asynchronous events delivered to the @ref StreamCallback.
 * @ingroup HELIOS_LIB_STREAM
 */
enum class StreamEvent : uint8_t
{
    DataAvailable  = 0x00U, /*!< New bytes arrived and are ready to read.        */
    WriteComplete  = 0x01U, /*!< A pending write or flush completed.              */
    Error          = 0x02U, /*!< An I/O error occurred on the stream.             */
    Closed         = 0x03U, /*!< The stream was closed (local or remote).         */
};

// =============================================================================
// StreamCallback
// =============================================================================

/** @brief Callback type for @ref iStream asynchronous events. */
using StreamCallback = Callback<void(StreamEvent)>;

// =============================================================================
// iStream
// =============================================================================

/**
 * @class  iStream
 * @brief  Transport-agnostic abstract byte stream.
 * @ingroup HELIOS_LIB_STREAM
 *
 * @details
 *   Derive from this class to adapt any byte-oriented transport (UART, USB CDC,
 *   TCP socket, pipe, etc.) to a uniform read/write interface.  Upper layers
 *   (protocol parsers, AT client, loggers) program against @ref iStream without
 *   depending on the physical transport.
 *
 *   Polling model:
 *     - Call @ref available() to check for pending bytes, then @ref read().
 *
 *   Interrupt / callback model:
 *     - Register a @ref StreamCallback; the implementation fires
 *       @ref StreamEvent::DataAvailable when new data arrives so the caller
 *       can read without polling.
 *
 * @note  Copy and move are deleted; streams own transport resources and must
 *        not be duplicated or relocated.
 */
class iStream
{
public:

    virtual ~iStream() noexcept = default;

    // -------------------------------------------------------------------------
    // Read
    // -------------------------------------------------------------------------

    /**
     * @brief  Read up to @p buffer.size() bytes from the stream.
     * @param[out] buffer      Caller-supplied buffer; receives the bytes read.
     * @param[out] bytes_read  Number of bytes actually placed into @p buffer.
     * @return @ref ReturnCode::AnsweredRequest on success (even if fewer bytes
     *         than requested were available).
     * @return @ref ReturnCode::ErrorQueueEmpty if no bytes are available.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is write-only.
     * @return @ref ReturnCode::ErrorReadFailed on I/O error.
     */
    [[nodiscard]]
    virtual ReturnCode read(ByteArray buffer, uint32_t& bytes_read) noexcept = 0;

    /**
     * @brief  Read exactly one byte from the stream.
     * @param[out] byte  Receives the byte read.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::ErrorQueueEmpty if no byte is available.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is write-only.
     * @return @ref ReturnCode::ErrorReadFailed on I/O error.
     */
    [[nodiscard]]
    virtual ReturnCode readByte(uint8_t& byte) noexcept = 0;

    // -------------------------------------------------------------------------
    // Write
    // -------------------------------------------------------------------------

    /**
     * @brief  Write all bytes in @p data to the stream.
     * @details The call may buffer internally; use @ref flush() to guarantee
     *          delivery to the underlying transport.
     * @param[in] data  Bytes to write.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is read-only.
     * @return @ref ReturnCode::ErrorBufferTooLarge if @p data exceeds the
     *         internal write buffer.
     * @return @ref ReturnCode::ErrorWriteFailed on I/O error.
     */
    [[nodiscard]]
    virtual ReturnCode write(ConstByteArray data) noexcept = 0;

    /**
     * @brief  Write exactly one byte to the stream.
     * @param[in] byte  Byte to write.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is read-only.
     * @return @ref ReturnCode::ErrorWriteFailed on I/O error.
     */
    [[nodiscard]]
    virtual ReturnCode writeByte(uint8_t byte) noexcept = 0;

    /**
     * @brief  Flush any internally buffered write data to the transport.
     * @return @ref ReturnCode::AnsweredRequest when all bytes have been handed
     *         to the underlying transport.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is read-only.
     * @return @ref ReturnCode::ErrorWriteFailed on I/O error.
     */
    [[nodiscard]]
    virtual ReturnCode flush() noexcept = 0;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /**
     * @brief  Return the number of bytes immediately available to read.
     * @param[out] count  Number of bytes waiting in the receive buffer.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the stream is not open.
     * @return @ref ReturnCode::ErrorNotSupported if the stream is write-only.
     */
    [[nodiscard]]
    virtual ReturnCode available(uint32_t& count) const noexcept = 0;

    /**
     * @brief  Check whether the stream is open and ready for I/O.
     * @param[out] open  @c true if the stream is open.
     * @return @ref ReturnCode::AnsweredRequest on success.
     */
    [[nodiscard]]
    virtual ReturnCode isOpen(bool& open) const noexcept = 0;

    /**
     * @brief  Close the stream and release the underlying transport resource.
     * @details After this call, all I/O methods return
     *          @ref ReturnCode::NotInitialized until the stream is re-opened
     *          by the implementation.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the stream was already closed.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode close() noexcept = 0;

    // -------------------------------------------------------------------------
    // Callback
    // -------------------------------------------------------------------------

    /**
     * @brief  Register a callback for asynchronous stream events.
     * @param[in] callback  Callable invoked with a @ref StreamEvent from ISR or
     *                      driver-task context.  Pass a default-constructed
     *                      @ref StreamCallback to deregister.
     */
    virtual void setCallback(StreamCallback callback) noexcept = 0;

protected:

    iStream() noexcept = default;

    /**
     * @brief  Dispatch a stream event to the registered callback.
     * @details Call from ISR or driver context when an event occurs.
     *          Must be @c noexcept and must not block.
     * @param[in] event  Event to dispatch.
     */
    virtual void handleEvent(StreamEvent event) noexcept = 0;

    iStream(const iStream&)             = delete;
    iStream& operator=(const iStream&)  = delete;
    iStream(iStream&&)                  = delete;
    iStream& operator=(iStream&&)       = delete;
};

} // namespace hel

#endif // HELIOS_LIB_ISTREAM_HPP_
