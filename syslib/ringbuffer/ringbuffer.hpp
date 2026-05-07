/**
 ******************************************************************************
 * @file    ringbuffer.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup HELIOS_SYSLIB_RINGBUFFER
 * @brief   Fixed-capacity typed circular (ring) buffer.
 */

#ifndef HELIOS_SYSLIB_RINGBUFFER_HPP_
#define HELIOS_SYSLIB_RINGBUFFER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hel
{

// =============================================================================
// RingBufferStatus
// =============================================================================

/**
 * @enum  RingBufferStatus
 * @brief Return codes for @ref RingBuffer operations.
 * @ingroup HELIOS_SYSLIB_RINGBUFFER
 */
enum class RingBufferStatus : uint8_t
{
    Ok    = 0x00U, /*!< Operation completed successfully. */
    Full  = 0x01U, /*!< Buffer is full — push was not performed. */
    Empty = 0x02U, /*!< Buffer is empty — pop/peek was not performed. */
};

// =============================================================================
// RingBuffer<T, N>
// =============================================================================

/**
 * @class  RingBuffer
 * @brief  Fixed-capacity typed circular buffer.
 * @ingroup HELIOS_SYSLIB_RINGBUFFER
 *
 * @details
 *   Implements a FIFO circular buffer with compile-time capacity.
 *   Suitable for streaming data (UART receive, ADC samples, audio frames, etc.).
 *
 *   Key characteristics:
 *   - **No dynamic memory** — storage is a plain `T[N]` member.
 *   - **No exceptions** — all operations return @ref RingBufferStatus.
 *   - **No RTOS dependency** — single-threaded by design; protect with a
 *     mutex when shared between tasks.
 *   - **Constexpr-compatible** — constructors and queries are `constexpr`.
 *   - **Two write modes**:
 *     - @ref push()           — fails with @ref RingBufferStatus::Full when full.
 *     - @ref push_overwrite() — silently discards the oldest element when full.
 *   - Bounds violations in @ref at() and @ref peek(offset, item) are guarded
 *     by `assert`; define `NDEBUG` to disable.
 *
 * @tparam T  Element type. Must be default-constructible and copy-assignable.
 * @tparam N  Maximum number of elements. Must be greater than zero.
 *
 * ### Usage
 * @code
 * RingBuffer<uint8_t, 64> rxBuf;
 *
 * // Write side (e.g. UART ISR)
 * rxBuf.push_overwrite(receivedByte);
 *
 * // Read side (task)
 * uint8_t byte{};
 * if (rxBuf.pop(byte) == RingBufferStatus::Ok) { process(byte); }
 * @endcode
 */
template<typename T, std::size_t N>
class RingBuffer
{
    static_assert(N > 0U, "RingBuffer capacity N must be greater than zero");
    static_assert(std::is_default_constructible<T>::value,
                  "RingBuffer element type T must be default-constructible");
    static_assert(std::is_copy_assignable<T>::value,
                  "RingBuffer element type T must be copy-assignable");

public:

    using value_type      = T;           /*!< Element type. */
    using reference       = T&;          /*!< Mutable reference to element. */
    using const_reference = const T&;    /*!< Const reference to element. */
    using size_type       = std::size_t; /*!< Type used for sizes and indices. */

    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    /**
     * @brief  Default constructor — produces an empty buffer.
     * @details All storage slots are value-initialised.
     */
    constexpr RingBuffer() noexcept = default;

    // -------------------------------------------------------------------------
    // Push (write)
    // -------------------------------------------------------------------------

    /**
     * @brief  Insert an element at the tail of the buffer.
     * @details Fails without modifying the buffer if it is full.
     *
     * @param[in]  item  Element to copy into the buffer.
     * @return @ref RingBufferStatus::Ok   on success.
     * @return @ref RingBufferStatus::Full if the buffer has no free slot.
     */
    [[nodiscard]]
    constexpr RingBufferStatus push(const_reference item) noexcept
    {
        if (isFull()) { return RingBufferStatus::Full; }
        m_buf[m_tail] = item;
        m_tail = advance(m_tail);
        ++m_size;
        return RingBufferStatus::Ok;
    }

    /**
     * @brief  Insert an element, overwriting the oldest if the buffer is full.
     * @details When the buffer is full the head pointer is advanced to discard
     *          the oldest element before writing the new one. Never fails.
     *
     * @param[in]  item  Element to copy into the buffer.
     */
    constexpr void push_overwrite(const_reference item) noexcept
    {
        if (isFull())
        {
            // Discard oldest element by advancing head.
            m_head = advance(m_head);
            --m_size;
        }
        m_buf[m_tail] = item;
        m_tail = advance(m_tail);
        ++m_size;
    }

    // -------------------------------------------------------------------------
    // Pop (read + remove)
    // -------------------------------------------------------------------------

    /**
     * @brief  Remove the oldest element from the head and copy it into @p item.
     *
     * @param[out] item  Destination to copy the element into.
     * @return @ref RingBufferStatus::Ok    on success.
     * @return @ref RingBufferStatus::Empty if the buffer contains no elements.
     */
    [[nodiscard]]
    constexpr RingBufferStatus pop(reference item) noexcept
    {
        if (isEmpty()) { return RingBufferStatus::Empty; }
        item   = m_buf[m_head];
        m_head = advance(m_head);
        --m_size;
        return RingBufferStatus::Ok;
    }

    /**
     * @brief  Discard the oldest element without returning it.
     * @return @ref RingBufferStatus::Ok    on success.
     * @return @ref RingBufferStatus::Empty if the buffer contains no elements.
     */
    [[nodiscard]]
    constexpr RingBufferStatus pop() noexcept
    {
        if (isEmpty()) { return RingBufferStatus::Empty; }
        m_head = advance(m_head);
        --m_size;
        return RingBufferStatus::Ok;
    }

    // -------------------------------------------------------------------------
    // Peek (read without removing)
    // -------------------------------------------------------------------------

    /**
     * @brief  Copy the oldest element into @p item without removing it.
     * @details Equivalent to @ref peek(0, item).
     *
     * @param[out] item  Destination to copy the element into.
     * @return @ref RingBufferStatus::Ok    on success.
     * @return @ref RingBufferStatus::Empty if the buffer contains no elements.
     */
    [[nodiscard]]
    constexpr RingBufferStatus peek(reference item) const noexcept
    {
        if (isEmpty()) { return RingBufferStatus::Empty; }
        item = m_buf[m_head];
        return RingBufferStatus::Ok;
    }

    /**
     * @brief  Copy the element at logical @p offset into @p item without removing it.
     * @details Offset 0 is the oldest (head) element; offset `size() - 1` is the
     *          newest (tail - 1). Asserts that @p offset is less than `size()`.
     *
     * @param[in]  offset  Logical index from the head (0 = oldest).
     * @param[out] item    Destination to copy the element into.
     * @return @ref RingBufferStatus::Ok    on success.
     * @return @ref RingBufferStatus::Empty if the buffer contains no elements.
     */
    [[nodiscard]]
    constexpr RingBufferStatus peek(size_type offset, reference item) const noexcept
    {
        if (isEmpty()) { return RingBufferStatus::Empty; }
        assert(offset < m_size);
        item = m_buf[(m_head + offset) % N];
        return RingBufferStatus::Ok;
    }

    // -------------------------------------------------------------------------
    // Indexed access
    // -------------------------------------------------------------------------

    /**
     * @brief  Return a const reference to the element at logical @p idx.
     * @details Index 0 is the oldest (head) element. Asserts that @p idx is
     *          less than `size()`.
     *
     * @param[in]  idx  Logical index from the head.
     * @return Const reference to the element.
     */
    [[nodiscard]]
    constexpr const_reference operator[](size_type idx) const noexcept
    {
        assert(idx < m_size);
        return m_buf[(m_head + idx) % N];
    }

    /**
     * @brief  Return a const reference to the oldest element (head).
     * @details Asserts that the buffer is not empty.
     * @return Const reference to the front element.
     */
    [[nodiscard]]
    constexpr const_reference front() const noexcept
    {
        assert(!isEmpty());
        return m_buf[m_head];
    }

    /**
     * @brief  Return a const reference to the newest element (tail - 1).
     * @details Asserts that the buffer is not empty.
     * @return Const reference to the back element.
     */
    [[nodiscard]]
    constexpr const_reference back() const noexcept
    {
        assert(!isEmpty());
        return m_buf[(m_tail == 0U ? N : m_tail) - 1U];
    }

    // -------------------------------------------------------------------------
    // State queries
    // -------------------------------------------------------------------------

    /**
     * @brief  Return the number of elements currently stored.
     * @return Element count in [0, N].
     */
    [[nodiscard]]
    constexpr size_type size() const noexcept { return m_size; }

    /**
     * @brief  Return the maximum number of elements the buffer can hold.
     * @return @p N (the template parameter).
     */
    [[nodiscard]]
    constexpr size_type capacity() const noexcept { return N; }

    /**
     * @brief  Return the number of free slots remaining.
     * @return `capacity() - size()`.
     */
    [[nodiscard]]
    constexpr size_type available() const noexcept { return N - m_size; }

    /**
     * @brief  Return @c true when the buffer contains no elements.
     */
    [[nodiscard]]
    constexpr bool isEmpty() const noexcept { return m_size == 0U; }

    /**
     * @brief  Return @c true when the buffer has no remaining free slots.
     */
    [[nodiscard]]
    constexpr bool isFull() const noexcept { return m_size == N; }

    // -------------------------------------------------------------------------
    // Reset
    // -------------------------------------------------------------------------

    /**
     * @brief  Discard all elements and reset head, tail, and size to zero.
     * @details Stored objects are not destroyed — the slots are simply marked
     *          as unused. The elements will be overwritten on the next push.
     */
    constexpr void clear() noexcept
    {
        m_head = 0U;
        m_tail = 0U;
        m_size = 0U;
    }

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief  Advance an index by one with wraparound at @p N.
     * @param[in]  idx  Current index.
     * @return Next index: `(idx + 1) % N`.
     */
    static constexpr size_type advance(size_type idx) noexcept
    {
        return (idx + 1U == N) ? 0U : (idx + 1U);
    }

    // -------------------------------------------------------------------------
    // Data members
    // -------------------------------------------------------------------------

    T         m_buf[N]{};  /*!< Static storage for up to N elements.         */
    size_type m_head{0U};  /*!< Index of the next element to read (oldest).  */
    size_type m_tail{0U};  /*!< Index of the next slot to write (newest + 1).*/
    size_type m_size{0U};  /*!< Number of elements currently stored.          */
};

} // namespace hel

#endif // HELIOS_SYSLIB_RINGBUFFER_HPP_
