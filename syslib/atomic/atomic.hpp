/**
 ******************************************************************************
 * @file    atomic.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup HELIOS_SYSLIB_ATOMIC
 * @brief   Lock-free atomic value and flag primitives.
 *
 * @details Provides two classes:
 *   - @ref Atomic<T>   - typed atomic value backed by @c std::atomic<T>.
 *   - @ref AtomicFlag  - binary test-and-set flag backed by @c std::atomic_flag.
 *
 *   Both classes enforce lock-freedom at compile time so that ISR-safe usage
 *   is guaranteed across all supported targets (STM32, ESP32, Qt, Linux).
 */

#ifndef HELIOS_SYSLIB_ATOMIC_HPP_
#define HELIOS_SYSLIB_ATOMIC_HPP_

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace hel
{

// =============================================================================
// MemoryOrder
// =============================================================================

/**
 * @enum  MemoryOrder
 * @brief Memory ordering constraints for atomic operations.
 * @ingroup HELIOS_SYSLIB_ATOMIC
 *
 * @details Maps to @c std::memory_order values. Choose the weakest ordering
 *          that is correct for the use case — weaker orderings generate fewer
 *          memory barrier instructions and perform better on multi-core targets.
 *
 * | Order       | Load | Store | Use case |
 * |-------------|------|-------|----------|
 * | kRelaxed    | ✔   | ✔    | Independent counter, no sync needed |
 * | kAcquire    | ✔   |       | Consumer side of a flag/queue |
 * | kRelease    |      | ✔    | Producer side of a flag/queue |
 * | kAcqRel     | ✔   | ✔    | Read-modify-write (exchange, CAS) |
 * | kSeqCst     | ✔   | ✔    | Total ordering — default, safest |
 */
enum class MemoryOrder : uint8_t
{
    Relaxed = 0U, /*!< No synchronisation — atomicity only. Fastest.            */
    Acquire = 1U, /*!< Load barrier: no reads/writes move before this load.     */
    Release = 2U, /*!< Store barrier: no reads/writes move after this store.    */
    AcqRel  = 3U, /*!< Acquire + Release: for read-modify-write operations.     */
    SeqCst  = 4U, /*!< Sequentially consistent: total ordering. Slowest.        */
};

// =============================================================================
// detail — internal helpers
// =============================================================================

namespace detail
{

/**
 * @brief  Convert @ref MemoryOrder to the corresponding @c std::memory_order.
 */
constexpr std::memory_order toStd(MemoryOrder order) noexcept
{
    switch (order)
    {
        case MemoryOrder::Relaxed: return std::memory_order_relaxed;
        case MemoryOrder::Acquire: return std::memory_order_acquire;
        case MemoryOrder::Release: return std::memory_order_release;
        case MemoryOrder::AcqRel:  return std::memory_order_acq_rel;
        case MemoryOrder::SeqCst:  return std::memory_order_seq_cst;
        default:                    return std::memory_order_seq_cst;
    }
}

} // namespace detail

// =============================================================================
// Atomic<T>
// =============================================================================

/**
 * @class  Atomic
 * @brief  Lock-free typed atomic value.
 * @ingroup HELIOS_SYSLIB_ATOMIC
 *
 * @details Wraps @c std::atomic<T> and enforces at compile time that the
 *          selected type is always lock-free on the target platform.
 *
 *          On ARM Cortex-M3/M4 (STM32) the compiler generates @c LDREX /
 *          @c STREX instructions for 8, 16, and 32-bit types — all lock-free.
 *          64-bit types are **not** lock-free on Cortex-M and will be rejected
 *          at compile time by the @c static_assert below.
 *
 *          On Qt (x86/x86-64) all types up to 64-bit are lock-free.
 *
 *          Memory ordering is expressed through @ref MemoryOrder. When in
 *          doubt use the default @ref MemoryOrder::SeqCst — it is always
 *          correct, only potentially slower on multi-core targets.
 *
 *          **ISR-safe pattern** (producer ISR → consumer task):
 *          @code
 *          Atomic<bool> dataReady{false};
 *
 *          // ISR
 *          dataReady.store(true, MemoryOrder::Release);
 *
 *          // Task
 *          if (dataReady.exchange(false, MemoryOrder::AcqRel)) {
 *              processData();
 *          }
 *          @endcode
 *
 * @tparam T  Value type. Must be trivially copyable and always lock-free on
 *            the target. Prefer @c uint8_t, @c uint16_t, @c uint32_t, @c bool,
 *            or pointer types for maximum portability.
 *
 * @note  Copy and move are deleted — atomics must not be copied or moved
 *        because @c std::atomic is not copyable.
 */
template<typename T>
class Atomic
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "Atomic<T>: T must be trivially copyable");

    static_assert(std::atomic<T>::is_always_lock_free,
                  "Atomic<T>: T is not always lock-free on this target. "
                  "On ARM Cortex-M use types of at most 32 bits (uint8_t, "
                  "uint16_t, uint32_t, bool, or pointer types).");

public:

    using value_type = T; /*!< The wrapped value type. */

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * @brief  Construct with an initial value.
     * @param[in]  value  Initial value. Defaults to zero-initialised @p T.
     */
    constexpr explicit Atomic(T value = T{}) noexcept
        : m_value(value)
    {}

    ~Atomic() noexcept = default;

    // -------------------------------------------------------------------------
    // Load / store
    // -------------------------------------------------------------------------

    /**
     * @brief  Atomically read the current value.
     * @param[in]  order  Memory ordering. Must not be @ref MemoryOrder::Release
     *                    or @ref MemoryOrder::AcqRel.
     * @return Current value.
     */
    [[nodiscard]]
    T load(MemoryOrder order = MemoryOrder::SeqCst) const noexcept
    {
        return m_value.load(detail::toStd(order));
    }

    /**
     * @brief  Atomically write a new value.
     * @param[in]  value  Value to store.
     * @param[in]  order  Memory ordering. Must not be @ref MemoryOrder::Acquire
     *                    or @ref MemoryOrder::AcqRel.
     */
    void store(T value, MemoryOrder order = MemoryOrder::SeqCst) noexcept
    {
        m_value.store(value, detail::toStd(order));
    }

    // -------------------------------------------------------------------------
    // Read-modify-write
    // -------------------------------------------------------------------------

    /**
     * @brief  Atomically replace the value with @p value and return the old value.
     * @param[in]  value  New value to store.
     * @param[in]  order  Memory ordering (default: @ref MemoryOrder::SeqCst).
     * @return Previous value.
     */
    [[nodiscard]]
    T exchange(T value, MemoryOrder order = MemoryOrder::SeqCst) noexcept
    {
        return m_value.exchange(value, detail::toStd(order));
    }

    /**
     * @brief  Atomically compare and conditionally exchange (strong CAS).
     * @details If the current value equals @p expected, replaces it with
     *          @p desired and returns @c true. Otherwise writes the current
     *          value into @p expected and returns @c false.
     *
     * @param[in,out] expected  Expected current value; updated on failure.
     * @param[in]     desired   Value to store on success.
     * @param[in]     order     Memory ordering on success (default: kSeqCst).
     * @return @c true if the exchange was performed.
     */
    [[nodiscard]]
    bool compareExchange(T& expected, T desired,
                         MemoryOrder order = MemoryOrder::SeqCst) noexcept
    {
        return m_value.compare_exchange_strong(
            expected, desired,
            detail::toStd(order),
            detail::toStd(order));
    }

    // -------------------------------------------------------------------------
    // Arithmetic (enabled only for integral types)
    // -------------------------------------------------------------------------

    /**
     * @brief  Atomically add @p value and return the previous value.
     * @tparam U  Deduced from @p T; enabled only for integral types.
     * @param[in]  value  Amount to add.
     * @param[in]  order  Memory ordering (default: kRelaxed for counters).
     * @return Previous value before the addition.
     */
    template<typename U = T>
    [[nodiscard]]
    auto fetchAdd(U value,
                  MemoryOrder order = MemoryOrder::Relaxed) noexcept
        -> std::enable_if_t<std::is_integral<U>::value, T>
    {
        return m_value.fetch_add(value, detail::toStd(order));
    }

    /**
     * @brief  Atomically subtract @p value and return the previous value.
     * @tparam U  Deduced from @p T; enabled only for integral types.
     * @param[in]  value  Amount to subtract.
     * @param[in]  order  Memory ordering (default: kRelaxed for counters).
     * @return Previous value before the subtraction.
     */
    template<typename U = T>
    [[nodiscard]]
    auto fetchSub(U value,
                  MemoryOrder order = MemoryOrder::Relaxed) noexcept
        -> std::enable_if_t<std::is_integral<U>::value, T>
    {
        return m_value.fetch_sub(value, detail::toStd(order));
    }

    /**
     * @brief  Atomically bitwise-OR with @p value and return the previous value.
     * @tparam U  Deduced from @p T; enabled only for integral types.
     */
    template<typename U = T>
    [[nodiscard]]
    auto fetchOr(U value,
                 MemoryOrder order = MemoryOrder::Relaxed) noexcept
        -> std::enable_if_t<std::is_integral<U>::value, T>
    {
        return m_value.fetch_or(value, detail::toStd(order));
    }

    /**
     * @brief  Atomically bitwise-AND with @p value and return the previous value.
     * @tparam U  Deduced from @p T; enabled only for integral types.
     */
    template<typename U = T>
    [[nodiscard]]
    auto fetchAnd(U value,
                  MemoryOrder order = MemoryOrder::Relaxed) noexcept
        -> std::enable_if_t<std::is_integral<U>::value, T>
    {
        return m_value.fetch_and(value, detail::toStd(order));
    }

    /**
     * @brief  Atomically bitwise-XOR with @p value and return the previous value.
     * @tparam U  Deduced from @p T; enabled only for integral types.
     */
    template<typename U = T>
    [[nodiscard]]
    auto fetchXor(U value,
                  MemoryOrder order = MemoryOrder::Relaxed) noexcept
        -> std::enable_if_t<std::is_integral<U>::value, T>
    {
        return m_value.fetch_xor(value, detail::toStd(order));
    }

    // -------------------------------------------------------------------------
    // Convenience operators
    // -------------------------------------------------------------------------

    /**
     * @brief  Implicit conversion — equivalent to @ref load() with kSeqCst.
     */
    operator T() const noexcept { return load(); }

    /**
     * @brief  Assignment — equivalent to @ref store() with kSeqCst.
     */
    Atomic& operator=(T value) noexcept { store(value); return *this; }

    // -------------------------------------------------------------------------
    // Deleted
    // -------------------------------------------------------------------------

    Atomic(const Atomic&)            = delete;
    Atomic& operator=(const Atomic&) = delete;
    Atomic(Atomic&&)                 = delete;
    Atomic& operator=(Atomic&&)      = delete;

private:

    std::atomic<T> m_value; /*!< Underlying standard atomic. */
};

// =============================================================================
// AtomicFlag
// =============================================================================

/**
 * @class  AtomicFlag
 * @brief  Lock-free binary flag — always lock-free on every platform.
 * @ingroup HELIOS_SYSLIB_ATOMIC
 *
 * @details Based on @c std::atomic_flag, which is the **only** type the C++
 *          standard guarantees to be lock-free on all implementations.
 *
 *          Intended for simple signalling between an ISR and a task, or
 *          between two tasks, without the overhead of a semaphore.
 *
 *          **ISR signals task pattern:**
 *          @code
 *          AtomicFlag dataReady;
 *
 *          // ISR
 *          dataReady.set();
 *
 *          // Task loop
 *          if (dataReady.testAndClear()) {
 *              processData();
 *          }
 *          @endcode
 *
 *          **Spin-lock pattern (use sparingly on embedded):**
 *          @code
 *          AtomicFlag lock;
 *
 *          // Acquire
 *          while (lock.testAndSet()) { }  // spin until we set it ourselves
 *          // ... critical section ...
 *          lock.clear();                  // release
 *          @endcode
 *
 * @note  The flag starts in the **cleared** (false) state. There is no
 *        constructor that starts it set.
 * @note  Copy and move are deleted — flags must not be copied or moved.
 */
class AtomicFlag
{
public:

    /**
     * @brief  Construct a cleared flag.
     */
    constexpr AtomicFlag() noexcept = default;

    ~AtomicFlag() noexcept = default;

    // -------------------------------------------------------------------------
    // Operations
    // -------------------------------------------------------------------------

    /**
     * @brief  Atomically set the flag to @c true and return the previous value.
     * @details Returns @c false if the flag was previously clear (the caller
     *          successfully set it), or @c true if it was already set.
     *
     *          Uses @ref MemoryOrder::AcqRel so that both the set and any
     *          preceding stores are visible to any thread that subsequently
     *          reads the flag.
     *
     * @return Previous flag state: @c false = was clear (set succeeded),
     *                              @c true  = was already set.
     */
    [[nodiscard]]
    bool testAndSet(MemoryOrder order = MemoryOrder::AcqRel) noexcept
    {
        return m_flag.test_and_set(detail::toStd(order));
    }

    /**
     * @brief  Atomically clear the flag (set to @c false).
     * @details Must not be called with @ref MemoryOrder::Acquire or
     *          @ref MemoryOrder::AcqRel.
     * @param[in]  order  Memory ordering (default: @ref MemoryOrder::Release).
     */
    void clear(MemoryOrder order = MemoryOrder::Release) noexcept
    {
        m_flag.clear(detail::toStd(order));
    }

    /**
     * @brief  Atomically set the flag to @c true without returning old value.
     * @details Convenience wrapper. Uses @ref MemoryOrder::Release so that
     *          stores preceding this call are visible to the consumer.
     */
    void set(MemoryOrder order = MemoryOrder::Release) noexcept
    {
        (void)m_flag.test_and_set(detail::toStd(order));
    }

    /**
     * @brief  Atomically test the flag and clear it if set.
     * @details Performs a single atomic read-modify-write: if the flag is set,
     *          clears it and returns @c true. If already clear, returns @c false.
     *
     *          Useful for a consumer that wants to act on the flag exactly once:
     *          @code
     *          if (flag.testAndClear()) { handleEvent(); }
     *          @endcode
     *
     * @param[in]  order  Memory ordering (default: @ref MemoryOrder::AcqRel).
     * @return @c true if the flag was set (and has now been cleared).
     */
    [[nodiscard]]
    bool testAndClear(MemoryOrder order = MemoryOrder::AcqRel) noexcept
    {
        if (m_flag.test_and_set(detail::toStd(order)))
        {
            // Flag was already set — clear it and report success.
            m_flag.clear(detail::toStd(order));
            return true;
        }
        // Flag was clear — we just set it; undo that.
        m_flag.clear(detail::toStd(order));
        return false;
    }

    /**
     * @brief  Read the current flag state without modifying it.
     * @details Available in C++20 via @c std::atomic_flag::test(). On C++17
     *          this is emulated with a test-and-set + conditional clear, which
     *          is not strictly read-only but is equivalent in observable effect.
     * @param[in]  order  Memory ordering (default: @ref MemoryOrder::Acquire).
     * @return Current flag state.
     */
    [[nodiscard]]
    bool test(MemoryOrder order = MemoryOrder::Acquire) const noexcept
    {
#if defined(__cpp_lib_atomic_flag_test) && (__cpp_lib_atomic_flag_test >= 201907L)
        return m_flag.test(detail::toStd(order));
#else
        // C++17 fallback: test_and_set on a mutable flag, then restore.
        const bool wasSet = m_flag.test_and_set(detail::toStd(order));
        if (!wasSet) { m_flag.clear(detail::toStd(order)); }
        return wasSet;
#endif
    }

    // -------------------------------------------------------------------------
    // Deleted
    // -------------------------------------------------------------------------

    AtomicFlag(const AtomicFlag&)            = delete;
    AtomicFlag& operator=(const AtomicFlag&) = delete;
    AtomicFlag(AtomicFlag&&)                 = delete;
    AtomicFlag& operator=(AtomicFlag&&)      = delete;

private:

    mutable std::atomic_flag m_flag = ATOMIC_FLAG_INIT; /*!< Underlying flag — cleared on init. */
};

// =============================================================================
// Convenience aliases
// =============================================================================

using AtomicBool    = Atomic<bool>;      /*!< @brief Atomic boolean. */
using AtomicU8      = Atomic<uint8_t>;   /*!< @brief Atomic unsigned 8-bit integer. */
using AtomicU16     = Atomic<uint16_t>;  /*!< @brief Atomic unsigned 16-bit integer. */
using AtomicU32     = Atomic<uint32_t>;  /*!< @brief Atomic unsigned 32-bit integer. */
using AtomicI8      = Atomic<int8_t>;    /*!< @brief Atomic signed 8-bit integer. */
using AtomicI16     = Atomic<int16_t>;   /*!< @brief Atomic signed 16-bit integer. */
using AtomicI32     = Atomic<int32_t>;   /*!< @brief Atomic signed 32-bit integer. */

} // namespace hel

#endif // HELIOS_SYSLIB_ATOMIC_HPP_
