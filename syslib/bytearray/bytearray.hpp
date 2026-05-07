/**
 ******************************************************************************
 * @file    bytearray.hpp
 * @author  Samuel Almeida Rocha 
 * @version 2.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_SYSLIB_BYTEARRAY
 * @brief   Mutable and read-only non-owning byte-buffer views.
 *
 */

#ifndef HELIOS_SYSLIB_BYTEARRAY_HPP_
#define HELIOS_SYSLIB_BYTEARRAY_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <array>

namespace hel
{



namespace detail
{
  typedef struct
  {
    long long __max_align_ll __attribute__((__aligned__(__alignof__(long long))));
    long double __max_align_ld __attribute__((__aligned__(__alignof__(long double))));
  } max_align_t;
}

// =============================================================================
// ByteArray — mutable view
// =============================================================================

/**
 * @brief   Mutable non-owning view over a contiguous block of bytes.
 * @details
 *   - Provides read/write byte-level access to memory owned elsewhere.
 *   - No memory is allocated or freed.
 *   - All template constructors are `explicit` to prevent accidental mutable
 *     aliasing. Use @ref ConstByteArray for read-only sources.
 *   - An implicit conversion to @ref ConstByteArray is provided (safe upcast).
 *   - Endian helpers (@ref read_le, @ref read_be, @ref write_le, @ref write_be)
 *     are restricted to unsigned integral types to comply with MISRA C++ rules
 *     on bitwise operations.
 *   - Out-of-bounds access is guarded by `assert`; define `NDEBUG` to disable.
 *   - Thread-safety: concurrent reads are safe. Concurrent writes to the
 *     underlying data are the caller's responsibility.
 *
 * @ingroup HELIOS_SYSLIB_BYTEARRAY
 */
class ByteArray
{
public:
    using element_type    = uint8_t;       /*!< Mutable element type. */
    using value_type      = uint8_t;       /*!< Non-cv element type. */
    using index_type      = std::size_t;   /*!< Type used for sizes and indices. */
    using difference_type = std::ptrdiff_t;/*!< Signed pointer difference type. */
    using pointer         = uint8_t*;      /*!< Pointer to element. */
    using const_pointer   = const uint8_t*;/*!< Pointer to const element. */
    using reference       = uint8_t&;      /*!< Reference to element. */
    using const_reference = const uint8_t&;/*!< Reference to const element. */
    using iterator        = uint8_t*;      /*!< Contiguous mutable iterator. */
    using const_iterator  = const uint8_t*;/*!< Contiguous const iterator. */

protected:
    pointer    m_ptr  = nullptr;
    index_type m_count = 0U;
    index_type m_capacity = 0U;  // Reserved for future use; currently always 0.

public:
    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    /**
     * @brief   Default constructor — produces an empty view.
     * @details `data() == nullptr`, `size() == 0`.
     */
    constexpr ByteArray() noexcept = default;

    /**
     * @brief   Construct from a raw byte pointer and an element count.
     * @details The caller guarantees that [@p p, @p p + @p count) is a valid
     *          dereferenceable range for the lifetime of this view.
     *
     * @param[in] p     Pointer to the first byte; may be `nullptr` only when
     *                 @p count is 0.
     * @param[in] count Number of bytes in the view.
     */
    constexpr ByteArray(pointer p, index_type capacity) noexcept
        : m_ptr(p),
          m_count(capacity),
          m_capacity(capacity)
    {}



    /**
     * @brief   Construct from a raw byte pointer and an element count.
     * @details The caller guarantees that [@p p, @p p + @p count) is a valid
     *          dereferenceable range for the lifetime of this view.
     *
     * @param[in] p     Pointer to the first byte; may be `nullptr` only when
     *                  @p count is 0.
     * @param[in] count Number of bytes in the view.
     * @param[in] capacity Total capacity of the buffer in bytes; reserved for
     *                 future use and currently ignored. Must be at least @p count.
     */
    constexpr ByteArray(pointer p, index_type count, index_type capacity) noexcept
        : m_ptr(p),
          m_count(count),
          m_capacity(capacity)
    {

    }


    /**
     * @brief   Construct a byte view over a single mutable object.
     * @details The view covers `sizeof(U)` bytes at the address of @p obj.
     *          @p U must not be const-qualified.
     *
     * @tparam  U     Object type; must be non-const.
     * @param[in] obj Reference to the object.
     *
     * @note MISRA C++ deviation: see typed-pointer constructor.
     */
    template<typename U>
    constexpr explicit ByteArray(U& obj) noexcept
        // cppcheck-suppress [misra-c++2023-8.2.2]
        : m_ptr(reinterpret_cast<pointer>(&obj)),
          m_count(sizeof(U)),
          m_capacity(sizeof(U))
    {

    }

    /**
     * @brief   Construct a byte view over a single const object.
     * @details The view covers `sizeof(U)` bytes at the address of @p obj.
     *          @p U may be const-qualified.
     *
     * @tparam  U     Object type; may be const-qualified.
     * @param[in] obj Reference to the object.
     *
     * @note MISRA C++ deviation: see typed-pointer constructor.
     */
    template<typename U>
    constexpr explicit ByteArray(const U& obj) noexcept
        // cppcheck-suppress [misra-c++2023-8.2.2]
        : m_ptr(reinterpret_cast<pointer>(&obj)),
          m_count(sizeof(U)),
          m_capacity(sizeof(U))
    {

    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns the number of bytes in the view.
     * @return  Byte count.
     */
    constexpr index_type size() const noexcept { return m_count; }

    /**
     * @brief   Returns the total size of the viewed memory in bytes.
     * @details Identical to `size()` because the element type is `uint8_t`.
     * @return  Byte count.
     */
    constexpr index_type size_bytes() const noexcept { return m_count; }

    /**
     * @brief   Returns the capacity of the view in bytes.
     * @details Currently always returns 0; reserved for future use if needed.
     * @return  Capacity in bytes.
     */
    constexpr index_type capacity() const noexcept { return m_capacity; }

    /**
     * @brief   Returns `true` when the view contains no bytes.
     * @return  `size() == 0`.
     */
    constexpr bool empty() const noexcept { return m_count == 0U; }

    /**
     * @brief   Returns a mutable pointer to the first byte.
     * @return  Mutable pointer; may be `nullptr` when empty.
     */
    constexpr pointer data() noexcept { return m_ptr; }

    /**
     * @brief   Returns a const pointer to the first byte.
     * @return  Const pointer; may be `nullptr` when empty.
     */
    constexpr const_pointer data() const noexcept { return m_ptr; }

    // -------------------------------------------------------------------------
    // Element access
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns a mutable reference to the byte at @p idx.
     * @details Behavior is undefined (asserted in debug) when @p idx >= size().
     *
     * @param[in] idx Zero-based index.
     * @return  Mutable reference to the byte.
     */
    constexpr reference operator[](index_type idx) noexcept
    {
        assert(idx < m_count);
        return m_ptr[idx];
    }

    /**
     * @brief   Returns a const reference to the byte at @p idx.
     * @param[in] idx Zero-based index.
     * @return  Const reference to the byte.
     */
    constexpr const_reference operator[](index_type idx) const noexcept
    {
        assert(idx < m_count);
        return m_ptr[idx];
    }

    /**
     * @brief   Returns a mutable reference to the first byte.
     * @details Asserts that the view is not empty.
     * @return  Mutable reference to the first byte.
     */
    constexpr reference front() noexcept
    {
        assert(!empty());
        return m_ptr[0];
    }

    /**
     * @brief   Returns a const reference to the first byte.
     * @return  Const reference to the first byte.
     */
    constexpr const_reference front() const noexcept
    {
        assert(!empty());
        return m_ptr[0];
    }

    /**
     * @brief   Returns a mutable reference to the last byte.
     * @details Asserts that the view is not empty.
     * @return  Mutable reference to the last byte.
     */
    constexpr reference back() noexcept
    {
        assert(!empty());
        return m_ptr[m_count - 1U];
    }

    /**
     * @brief   Returns a const reference to the last byte.
     * @return  Const reference to the last byte.
     */
    constexpr const_reference back() const noexcept
    {
        assert(!empty());
        return m_ptr[m_count - 1U];
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    constexpr iterator       begin()  noexcept       { return m_ptr; }          /*!< @brief Mutable begin iterator. */
    constexpr const_iterator begin()  const noexcept { return m_ptr; }          /*!< @brief Const begin iterator. */
    constexpr const_iterator cbegin() const noexcept { return m_ptr; }          /*!< @brief Const begin iterator. */
    constexpr iterator       end()    noexcept       { return m_ptr + m_count; }  /*!< @brief Mutable end iterator. */
    constexpr const_iterator end()    const noexcept { return m_ptr + m_count; }  /*!< @brief Const end iterator. */
    constexpr const_iterator cend()   const noexcept { return m_ptr + m_count; }  /*!< @brief Const end iterator. */

    // -------------------------------------------------------------------------
    // Slicing
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns a mutable view of the first @p count bytes.
     * @param[in] count Number of bytes; must not exceed `size()`.
     * @return  ByteArray viewing the first @p count bytes.
     */
    constexpr ByteArray first(index_type count) noexcept
    {
        assert(count <= m_count);
        return ByteArray(m_ptr, count);
    }

    /**
     * @brief   Returns a mutable view of the last @p count bytes.
     * @param[in] count Number of bytes; must not exceed `size()`.
     * @return  ByteArray viewing the last @p count bytes.
     */
    constexpr ByteArray last(index_type count) noexcept
    {
        assert(count <= m_count);
        return ByteArray(m_ptr + (m_count - count), count);
    }


    // -------------------------------------------------------------------------
    // Type punning
    // -------------------------------------------------------------------------

    /**
     * @brief   Reinterprets the buffer as a pointer to @p U.
     * @details The caller guarantees that the buffer is sufficiently large and
     *          suitably aligned for @p U.
     *
     * @tparam  U   Target type; `alignof(U)` must not exceed `alignof(std::max_align_t)`.
     * @return  Mutable pointer to @p U.
     *
     * @note MISRA C++ deviation: `reinterpret_cast` is required for type punning.
     *       The cast is safe provided the buffer was originally created from an
     *       object of type @p U or compatible layout.
     */
    template<typename U>
    constexpr U* as() noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        // cppcheck-suppress [misra-c++2023-8.2.2]
        return reinterpret_cast<U*>(m_ptr);
    }

    /**
     * @brief   Reinterprets the buffer as a const pointer to @p U.
     * @tparam  U   Target type.
     * @return  Const pointer to @p U.
     *
     * @note MISRA C++ deviation: see mutable overload.
     */
    template<typename U>
    constexpr const U* as() const noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        // cppcheck-suppress [misra-c++2023-8.2.2]
        return reinterpret_cast<const U*>(m_ptr);
    }


    /**
     * @brief   Reinterprets the buffer as a mutable reference to a single object.
     * @details Asserts that `size() >= sizeof(U)` at runtime.
     *
     * @tparam  U   Target type.
     * @return  Mutable reference to the object at the start of the buffer.
     */
    template<typename U>
    constexpr U& as_object() noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        assert(sizeof(U) <= m_count);
        return *as<U>();
    }

    /**
     * @brief   Reinterprets the buffer as a const reference to a single object.
     * @tparam  U   Target type.
     * @return  Const reference to the object at the start of the buffer.
     */
    template<typename U>
    constexpr const U& as_object() const noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        assert(sizeof(U) <= m_count);
        return *as<U>();
    }

    // -------------------------------------------------------------------------
    // Byte-level access
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns the byte value at @p idx.
     * @param[in] idx Zero-based index; must be less than `size()`.
     * @return  Byte value.
     */
    constexpr uint8_t get_byte(index_type idx) const noexcept
    {
        assert(idx < m_count);
        return m_ptr[idx];
    }

    /**
     * @brief   Writes @p value to the byte at @p idx.
     * @param[in] idx   Zero-based index; must be less than `size()`.
     * @param[in] value Byte value to write.
     */
    constexpr void set_byte(index_type idx, uint8_t value) noexcept
    {
        assert(idx < m_count);
        m_ptr[idx] = value;
    }

    // -------------------------------------------------------------------------
    // Endian-aware read
    // -------------------------------------------------------------------------

    /**
     * @brief   Reads an unsigned integer from the buffer in little-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U       Unsigned integral type to read (e.g. `uint16_t`, `uint32_t`).
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     * @return  Value assembled from bytes [offset, offset + sizeof(U)).
     */
    template<typename U>
    constexpr U read_le(index_type offset = 0U) const noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "read_le requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);

        U value{};
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            value |= static_cast<U>(m_ptr[offset + i]) << (i * 8U);
        }
        return value;
    }

    /**
     * @brief   Reads an unsigned integer from the buffer in big-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U       Unsigned integral type to read.
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     * @return  Value assembled from bytes [offset, offset + sizeof(U)).
     */
    template<typename U>
    constexpr U read_be(index_type offset = 0U) const noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "read_be requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);
        U value{};
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            value |= static_cast<U>(m_ptr[offset + i]) << ((sizeof(U) - 1U - i) * 8U);
        }
        return value;
    }

    // -------------------------------------------------------------------------
    // Endian-aware write
    // -------------------------------------------------------------------------

    /**
     * @brief   Writes an unsigned integer to the buffer in little-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U       Unsigned integral type to write.
     * @param[in] value  Value to write.
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     */
    template<typename U>
    constexpr void write_le(U value, index_type offset = 0U) noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "write_le requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            m_ptr[offset + i] = static_cast<uint8_t>(value >> (i * 8U));
        }
    }

    /**
     * @brief   Writes an unsigned integer to the buffer in big-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U       Unsigned integral type to write.
     * @param[in] value  Value to write.
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     */
    template<typename U>
    constexpr void write_be(U value, index_type offset = 0U) noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "write_be requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            m_ptr[offset + i] = static_cast<uint8_t>(value >> ((sizeof(U) - 1U - i) * 8U));
        }
    }
};

// =============================================================================
// ConstByteArray — read-only view
// =============================================================================

/**
 * @brief   Read-only non-owning view over a contiguous block of bytes.
 * @details
 *   - Provides read-only byte-level access to memory owned elsewhere.
 *   - Implicitly constructible from @ref ByteArray (safe const upcast).
 *   - Endian helpers are restricted to unsigned integral types.
 *   - Out-of-bounds access is guarded by `assert`; define `NDEBUG` to disable.
 *   - Thread-safety: all operations are `const`; concurrent reads are always safe.
 *
 * @ingroup HELIOS_SYSLIB_BYTEARRAY
 */
class ConstByteArray
{
public:
    using element_type    = const uint8_t;   /*!< Const element type.              */
    using value_type      = uint8_t;         /*!< Non-cv element type.             */
    using index_type      = std::size_t;     /*!< Type used for sizes and indices. */
    using difference_type = std::ptrdiff_t;  /*!< Signed pointer difference type. */
    using pointer         = const uint8_t*;  /*!< Pointer to const element.       */
    using const_pointer   = const uint8_t*;  /*!< Pointer to const element.       */
    using reference       = const uint8_t&;  /*!< Reference to const element.     */
    using const_reference = const uint8_t&;  /*!< Reference to const element.     */
    using iterator        = const uint8_t*;  /*!< Contiguous const iterator.      */
    using const_iterator  = const uint8_t*;  /*!< Contiguous const iterator.      */

    // -------------------------------------------------------------------------
    // Constructors
    // -------------------------------------------------------------------------

    /**
     * @brief   Default constructor — produces an empty view.
     * @details `data() == nullptr`, `size() == 0`.
     */
    constexpr ConstByteArray() noexcept = default;

    /**
     * @brief   Construct from a raw const byte pointer and a byte count.
     * @details The caller guarantees that [@p p, @p p + @p count) is valid and
     *          dereferenceable for the lifetime of this view.
     *
     * @param[in] p     Pointer to the first byte; may be `nullptr` only when @p count is 0.
     * @param[in] count Number of bytes in the view.
     */
    constexpr ConstByteArray(const uint8_t* p, index_type count) noexcept
        : m_ptr(p), m_count(count) {}

    /**
     * @brief   Implicit conversion from a mutable @ref ByteArray (safe const upcast).
     * @details The resulting view shares the same memory region as @p other.
     *          The view remains valid for the lifetime of the source @ref ByteArray.
     *
     * @param[in] other Mutable byte view to convert from.
     */
    ConstByteArray(const ByteArray& other) noexcept
        : m_ptr(other.data()), m_count(other.size()) {}

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns the number of bytes in the view.
     * @return  Byte count.
     */
    constexpr index_type size() const noexcept { return m_count; }

    /**
     * @brief   Returns the total size of the viewed memory in bytes.
     * @details Identical to `size()` because the element type is `uint8_t`.
     * @return  Byte count.
     */
    constexpr index_type size_bytes() const noexcept { return m_count; }

    /**
     * @brief   Returns `true` when the view contains no bytes.
     * @return  `size() == 0`.
     */
    constexpr bool empty() const noexcept { return m_count == 0U; }

    /**
     * @brief   Returns a const pointer to the first byte.
     * @return  Const pointer; may be `nullptr` when empty.
     */
    constexpr const_pointer data() const noexcept { return m_ptr; }

    // -------------------------------------------------------------------------
    // Element access
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns a const reference to the byte at @p idx.
     * @details Behavior is undefined (asserted in debug) when @p idx >= size().
     *
     * @param[in] idx Zero-based index.
     * @return  Const reference to the byte.
     */
    constexpr const_reference operator[](index_type idx) const noexcept
    {
        assert(idx < m_count);
        return m_ptr[idx];
    }

    /**
     * @brief   Returns a const reference to the first byte. Asserts that the view is not empty.
     * @return  Const reference to the first byte.
     */
    constexpr const_reference front() const noexcept
    {
        assert(!empty());
        return m_ptr[0U];
    }

    /**
     * @brief   Returns a const reference to the last byte. Asserts that the view is not empty.
     * @return  Const reference to the last byte.
     */
    constexpr const_reference back() const noexcept
    {
        assert(!empty());
        return m_ptr[m_count - 1U];
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    constexpr const_iterator begin()  const noexcept { return m_ptr; }             /*!< @brief Const begin iterator. */
    constexpr const_iterator end()    const noexcept { return m_ptr + m_count; }   /*!< @brief Const end iterator.   */
    constexpr const_iterator cbegin() const noexcept { return m_ptr; }             /*!< @brief Const begin iterator. */
    constexpr const_iterator cend()   const noexcept { return m_ptr + m_count; }   /*!< @brief Const end iterator.   */

    // -------------------------------------------------------------------------
    // Slicing
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns a read-only view of the first @p count bytes.
     * @param[in] count Number of bytes; must not exceed `size()`.
     * @return  @ref ConstByteArray viewing the first @p count bytes.
     */
    constexpr ConstByteArray first(index_type count) const noexcept
    {
        assert(count <= m_count);
        return ConstByteArray(m_ptr, count);
    }

    /**
     * @brief   Returns a read-only view of the last @p count bytes.
     * @param[in] count Number of bytes; must not exceed `size()`.
     * @return  @ref ConstByteArray viewing the last @p count bytes.
     */
    constexpr ConstByteArray last(index_type count) const noexcept
    {
        assert(count <= m_count);
        return ConstByteArray(m_ptr + (m_count - count), count);
    }

    // -------------------------------------------------------------------------
    // Byte-level access
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns the byte value at @p idx.
     * @param[in] idx Zero-based index; must be less than `size()`.
     * @return  Byte value.
     */
    constexpr uint8_t get_byte(index_type idx) const noexcept
    {
        assert(idx < m_count);
        return m_ptr[idx];
    }

    // -------------------------------------------------------------------------
    // Endian-aware read
    // -------------------------------------------------------------------------

    /**
     * @brief   Reads an unsigned integer from the buffer in little-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U        Unsigned integral type (e.g. `uint16_t`, `uint32_t`).
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     * @return  Value assembled from bytes [offset, offset + sizeof(U)).
     */
    template<typename U>
    constexpr U read_le(index_type offset = 0U) const noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "read_le requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);
        U value{};
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            value |= static_cast<U>(m_ptr[offset + i]) << (i * 8U);
        }
        return value;
    }

    /**
     * @brief   Reads an unsigned integer from the buffer in big-endian order.
     * @details Asserts that `offset + sizeof(U) <= size()`.
     *
     * @tparam  U        Unsigned integral type.
     * @param[in] offset Byte offset of the first byte; defaults to 0.
     * @return  Value assembled from bytes [offset, offset + sizeof(U)).
     */
    template<typename U>
    constexpr U read_be(index_type offset = 0U) const noexcept
    {
        static_assert(std::is_integral<U>::value && std::is_unsigned<U>::value,
                      "read_be requires an unsigned integral type");
        assert(offset + sizeof(U) <= m_count);
        U value{};
        for (std::size_t i = 0U; i < sizeof(U); ++i)
        {
            value |= static_cast<U>(m_ptr[offset + i]) << ((sizeof(U) - 1U - i) * 8U);
        }
        return value;
    }

    // -------------------------------------------------------------------------
    // Type punning (read-only)
    // -------------------------------------------------------------------------

    /**
     * @brief   Reinterprets the buffer as a const pointer to @p U.
     * @details The caller guarantees the buffer is sufficiently large and
     *          suitably aligned for @p U.
     *
     * @tparam  U   Target type; `alignof(U)` must not exceed `alignof(std::max_align_t)`.
     * @return  Const pointer to @p U.
     *
     * @note MISRA C++ deviation: `reinterpret_cast` is required for type punning.
     *       Cast is safe provided the buffer was originally created from an object
     *       of type @p U or a layout-compatible type.
     */
    template<typename U>
    constexpr const U* as() const noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        // cppcheck-suppress [misra-c++2023-8.2.2]
        return reinterpret_cast<const U*>(m_ptr);
    }

    /**
     * @brief   Reinterprets the buffer as a const reference to a single object.
     * @details Asserts that `size() >= sizeof(U)` at runtime.
     *
     * @tparam  U   Target type.
     * @return  Const reference to the object at the start of the buffer.
     */
    template<typename U>
    constexpr const U& as_object() const noexcept
    {
        static_assert(alignof(U) <= alignof(detail::max_align_t),
                      "Target type alignment exceeds std::max_align_t");
        assert(sizeof(U) <= m_count);
        return *as<U>();
    }

private:
    const uint8_t* m_ptr   = nullptr; /*!< Pointer to the first byte of the viewed memory. */
    index_type     m_count = 0U;      /*!< Number of bytes in the view.                    */
};

} // namespace hel

#endif // HELIOS_SYSLIB_BYTEARRAY_HPP_
