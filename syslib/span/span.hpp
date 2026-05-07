/**
 ******************************************************************************
 * @file    span.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_SYSLIB_SPAN
 * @brief   Non-owning view over a contiguous sequence of elements.
 *

 */

#ifndef HELIOS_SYSLIB_SPAN_HPP_
#define HELIOS_SYSLIB_SPAN_HPP_

#include <cassert>
#include <cstddef>
#include <limits>
#include <array>
#include <type_traits>

namespace hel
{

/**
 * @brief   Sentinel value indicating a dynamically-sized span.
 * @details Equivalent to `std::dynamic_extent` (C++20). When used as the
 *          @p Extent template argument, the span's size is stored at runtime
 *          rather than encoded in the type.
 * @ingroup HELIOS_SYSLIB_SPAN
 */
constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

/**
 * @brief   Non-owning view over a contiguous sequence of elements.
 * @details
 *   - Provides bounds-safe, pointer-like access to memory owned elsewhere.
 *   - When @p Extent equals @ref dynamic_extent the size is stored at runtime;
 *     otherwise it is encoded in the type and no size field is consulted.
 *   - No memory is allocated or freed by this class.
 *   - All operations are `constexpr` and `noexcept`.
 *   - Out-of-bounds element access is guarded by `assert`; release builds
 *     with `NDEBUG` defined skip those checks.
 *   - Thread-safety: multiple concurrent reads are safe. Concurrent writes to
 *     the underlying data are the caller's responsibility.
 *
 * @tparam T      Element type. May be cv-qualified.
 * @tparam Extent Number of elements encoded in the type, or
 *                @ref dynamic_extent for a runtime-sized span.
 *
 * @ingroup HELIOS_SYSLIB_SPAN
 */
template<class T, std::size_t Extent = dynamic_extent>
class Span
{
public:
    using element_type    = T;                             /*!< Possibly cv-qualified element type. */
    using value_type      = typename std::remove_cv<T>::type; /*!< Non-cv element type. */
    using index_type      = std::size_t;                   /*!< Type used for sizes and indices. */
    using difference_type = std::ptrdiff_t;                /*!< Signed difference between two pointers. */
    using pointer         = T*;                            /*!< Pointer to element. */
    using const_pointer   = const T*;                      /*!< Pointer to const element. */
    using reference       = T&;                            /*!< Reference to element. */
    using const_reference = const T&;                      /*!< Reference to const element. */
    using iterator        = T*;                            /*!< Contiguous iterator. */
    using const_iterator  = const T*;                      /*!< Contiguous const iterator. */

    /** @brief Compile-time extent; equals @ref dynamic_extent for runtime-sized spans. */
    static constexpr std::size_t extent = Extent;

private:
    pointer    ptr_  = nullptr;
    index_type size_ = (Extent == dynamic_extent) ? 0U : Extent;

public:
    /**
     * @brief   Default constructor — produces an empty span.
     * @details Only available when @p Extent is 0 or @ref dynamic_extent.
     *          The resulting span has `data() == nullptr` and `size() == 0`.
     */
    constexpr Span() noexcept
        : ptr_(nullptr), size_((Extent == dynamic_extent) ? 0U : Extent)
    {
        static_assert(Extent == 0U || Extent == dynamic_extent,
                      "Default constructor requires Extent == 0 or dynamic_extent");
    }

    /**
     * @brief   Construct from a pointer and an element count.
     * @details The caller guarantees that [@p p, @p p + @p count) is a valid,
     *          dereferenceable range for the lifetime of this span.
     *          When @p Extent is not @ref dynamic_extent, @p count must equal
     *          @p Extent at runtime.
     *
     * @param[in] p     Pointer to the first element; may be `nullptr` only when
     *                  @p count is 0.
     * @param[in] count Number of elements in the view.
     */
    constexpr Span(pointer p, index_type count) noexcept
        : ptr_(p), size_((Extent == dynamic_extent) ? count : Extent)
    {
        assert(Extent == dynamic_extent || count == Extent);
    }

    /**
     * @brief   Construct from a half-open pointer range [@p first, @p last).
     * @details The caller guarantees the range is valid for the lifetime of
     *          this span. `last - first` must equal @p Extent when @p Extent
     *          is not @ref dynamic_extent.
     *
     * @param[in] first Pointer to the first element.
     * @param[in] last  One-past-the-end pointer.
     */
    constexpr Span(pointer first, pointer last) noexcept
        : ptr_(first),
          size_((Extent == dynamic_extent) ? static_cast<index_type>(last - first) : Extent)
    {
        assert(last >= first);
        assert(Extent == dynamic_extent ||
               static_cast<index_type>(last - first) == Extent);
    }

    /**
     * @brief   Construct from a C-style array.
     * @details Deduces the element count from the array size. Only participates
     *          in overload resolution when @p Extent is @ref dynamic_extent or
     *          @p Extent equals @p N.
     *
     * @tparam  N       Array size, deduced automatically.
     * @param[in] arr   Reference to the array.
     */
    template<std::size_t N>
    constexpr Span(element_type (&arr)[N]) noexcept  // NOLINT(modernize-avoid-c-arrays)
        : ptr_(arr), size_((Extent == dynamic_extent) ? N : Extent)
    {
        static_assert(Extent == dynamic_extent || Extent == N,
                      "Array size must match the static extent");
    }

    /**
     * @brief   Construct from a mutable `std::array`.
     * @details Only participates in overload resolution when @p Extent is
     *          @ref dynamic_extent or @p Extent equals @p N.
     *
     * @tparam  N       Array size, deduced automatically.
     * @param[in] arr   Reference to the array.
     */
    template<std::size_t N>
    constexpr Span(std::array<value_type, N>& arr) noexcept
        : ptr_(arr.data()), size_((Extent == dynamic_extent) ? N : Extent)
    {
        static_assert(Extent == dynamic_extent || Extent == N,
                      "std::array size must match the static extent");
    }

    /**
     * @brief   Construct from a const `std::array`.
     * @details Requires @p T to be const-qualified. Only participates in
     *          overload resolution when @p Extent is @ref dynamic_extent or
     *          @p Extent equals @p N.
     *
     * @tparam  N       Array size, deduced automatically.
     * @param[in] arr   Reference to the const array.
     */
    template<std::size_t N>
    constexpr Span(const std::array<value_type, N>& arr) noexcept
        : ptr_(arr.data()), size_((Extent == dynamic_extent) ? N : Extent)
    {
        static_assert(std::is_const<element_type>::value,
                      "Constructing Span<T> from const std::array requires T to be const");
        static_assert(Extent == dynamic_extent || Extent == N,
                      "std::array size must match the static extent");
    }

    /**
     * @brief   Converting constructor from a compatible span type.
     * @details Participates in overload resolution only when:
     *          - `U(*)[]` is implicitly convertible to `T(*)[]` (i.e. same type
     *            or adding const), and
     *          - the extents are compatible (at least one is dynamic, or both
     *            are equal).
     *
     * @tparam  U   Source element type.
     * @tparam  E   Source extent.
     * @param[in] other The span to convert from.
     */
    template<class U, std::size_t E,
             class = typename std::enable_if<
                 std::is_convertible<U (*)[], T (*)[]>::value &&
                 (E == dynamic_extent || Extent == dynamic_extent || E == Extent)>::type>
    constexpr Span(const Span<U, E>& other) noexcept
        : ptr_(other.data()),
          size_((Extent == dynamic_extent) ? other.size() : Extent)
    {
        static_assert(std::is_const<element_type>::value || !std::is_const<U>::value,
                      "Cannot construct Span<T> from Span<const U> unless T is const");
    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns the number of elements in the span.
     * @return  Element count.
     */
    constexpr index_type size() const noexcept
    {
        return size_;
    }

    /**
     * @brief   Returns the total size of the viewed memory in bytes.
     * @return  `size() * sizeof(element_type)`.
     */
    constexpr index_type size_bytes() const noexcept
    {
        return size_ * sizeof(element_type);
    }

    /**
     * @brief   Returns `true` when the span contains no elements.
     * @return  `size() == 0`.
     */
    constexpr bool empty() const noexcept
    {
        return size_ == 0U;
    }

    /**
     * @brief   Returns a pointer to the first element.
     * @return  Pointer to `T`; may be `nullptr` when the span is empty.
     */
    constexpr pointer data() const noexcept
    {
        return ptr_;
    }

    // -------------------------------------------------------------------------
    // Element access
    // -------------------------------------------------------------------------

    /**
     * @brief   Accesses the element at position @p idx.
     * @details Behavior is undefined (asserted in debug) when @p idx >= size().
     *
     * @param[in] idx Zero-based index.
     * @return  Reference to the element.
     */
    constexpr reference operator[](index_type idx) const noexcept
    {
        assert(idx < size_);
        return ptr_[idx];
    }

    /**
     * @brief   Accesses the first element.
     * @details Behavior is undefined (asserted in debug) when the span is empty.
     * @return  Reference to the first element.
     */
    constexpr reference front() const noexcept
    {
        assert(size_ > 0U);
        return ptr_[0];
    }

    /**
     * @brief   Accesses the last element.
     * @details Behavior is undefined (asserted in debug) when the span is empty.
     * @return  Reference to the last element.
     */
    constexpr reference back() const noexcept
    {
        assert(size_ > 0U);
        return ptr_[size_ - 1U];
    }

    // -------------------------------------------------------------------------
    // Iterators
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns an iterator to the first element.
     * @return  Pointer to the first element, or `nullptr` when empty.
     */
    constexpr iterator begin() const noexcept
    {
        return ptr_;
    }

    /**
     * @brief   Returns an iterator one past the last element.
     * @return  Pointer past the last element.
     */
    constexpr iterator end() const noexcept
    {
        return ptr_ + size_;
    }

    /**
     * @brief   Returns a const iterator to the first element.
     * @return  Const pointer to the first element.
     */
    constexpr const_iterator cbegin() const noexcept
    {
        return ptr_;
    }

    /**
     * @brief   Returns a const iterator one past the last element.
     * @return  Const pointer past the last element.
     */
    constexpr const_iterator cend() const noexcept
    {
        return ptr_ + size_;
    }

    // -------------------------------------------------------------------------
    // Slicing
    // -------------------------------------------------------------------------

    /**
     * @brief   Returns a compile-time-sized span of the first @p Count elements.
     * @details The returned span has static extent @p Count.
     *
     * @tparam  Count Number of elements to include; must not exceed @p Extent
     *                when @p Extent is not @ref dynamic_extent.
     * @return  Span<T, Count> viewing the first @p Count elements.
     */
    template<std::size_t Count>
    constexpr Span<T, Count> first() const noexcept
    {
        static_assert(Count != dynamic_extent, "Count must be a fixed value");
        static_assert(Extent == dynamic_extent || Count <= Extent,
                      "Count must not exceed the static extent");
        assert(Count <= size_);
        return Span<T, Count>(ptr_, Count);
    }

    /**
     * @brief   Returns a runtime-sized span of the first @p count elements.
     *
     * @param[in] count Number of elements; must not exceed `size()`.
     * @return  Span<T, dynamic_extent> viewing the first @p count elements.
     */
    constexpr Span<T, dynamic_extent> first(index_type count) const noexcept
    {
        assert(count <= size_);
        return Span<T, dynamic_extent>(ptr_, count);
    }

    /**
     * @brief   Returns a compile-time-sized span of the last @p Count elements.
     * @details The returned span has static extent @p Count.
     *
     * @tparam  Count Number of elements to include; must not exceed @p Extent
     *                when @p Extent is not @ref dynamic_extent.
     * @return  Span<T, Count> viewing the last @p Count elements.
     */
    template<std::size_t Count>
    constexpr Span<T, Count> last() const noexcept
    {
        static_assert(Count != dynamic_extent, "Count must be a fixed value");
        static_assert(Extent == dynamic_extent || Count <= Extent,
                      "Count must not exceed the static extent");
        assert(Count <= size_);
        return Span<T, Count>(ptr_ + (size_ - Count), Count);
    }

    /**
     * @brief   Returns a runtime-sized span of the last @p count elements.
     *
     * @param[in] count Number of elements; must not exceed `size()`.
     * @return  Span<T, dynamic_extent> viewing the last @p count elements.
     */
    constexpr Span<T, dynamic_extent> last(index_type count) const noexcept
    {
        assert(count <= size_);
        return Span<T, dynamic_extent>(ptr_ + (size_ - count), count);
    }

    /**
     * @brief   Returns a compile-time-sized sub-span starting at @p Offset.
     * @details When @p Count is @ref dynamic_extent the sub-span extends to the
     *          end of this span; the returned extent is deduced accordingly.
     *
     * @tparam  Offset Zero-based start index; must not exceed @p Extent when
     *                 @p Extent is not @ref dynamic_extent.
     * @tparam  Count  Number of elements, or @ref dynamic_extent to reach the end.
     * @return  Sub-span with a statically-computed extent where possible.
     */
    template<std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr auto subSpan() const noexcept
        -> Span<T, (Count != dynamic_extent
                        ? Count
                        : (Extent != dynamic_extent ? (Extent - Offset) : dynamic_extent))>
    {
        static_assert(Extent == dynamic_extent || Offset <= Extent,
                      "Offset out of range for static extent");
        static_assert(Count == dynamic_extent || Extent == dynamic_extent || Offset + Count <= Extent,
                      "Offset + Count out of range for static extent");

        constexpr std::size_t NewExtent =
            (Count != dynamic_extent)
                ? Count
                : (Extent != dynamic_extent ? (Extent - Offset) : dynamic_extent);

        assert(Offset <= size_);
        const index_type new_count = (Count == dynamic_extent) ? (size_ - Offset) : Count;
        assert(new_count <= size_ - Offset);

        return Span<T, NewExtent>(ptr_ + Offset, new_count);
    }

    /**
     * @brief   Returns a runtime-sized sub-span starting at @p offset.
     * @details When @p count is @ref dynamic_extent the sub-span extends to the
     *          end of this span.
     *
     * @param[in] offset Zero-based start index; must not exceed `size()`.
     * @param[in] count  Number of elements, or @ref dynamic_extent to reach the end.
     * @return  Span<T, dynamic_extent> viewing the requested sub-range.
     */
    constexpr Span<T, dynamic_extent> subSpan(index_type offset,
                                              index_type count = dynamic_extent) const noexcept
    {
        assert(offset <= size_);
        const index_type new_count = (count == dynamic_extent) ? (size_ - offset) : count;
        assert(new_count <= size_ - offset);
        return Span<T, dynamic_extent>(ptr_ + offset, new_count);
    }
};

// -----------------------------------------------------------------------------
// Deduction guides (C++17)
// -----------------------------------------------------------------------------

template<class T, std::size_t N>
Span(T (&)[N]) -> Span<T, N>;  // NOLINT(modernize-avoid-c-arrays)

template<class T, std::size_t N>
Span(std::array<T, N>&) -> Span<T, N>;

template<class T, std::size_t N>
Span(const std::array<T, N>&) -> Span<const T, N>;

// -----------------------------------------------------------------------------
// Factory helpers
// -----------------------------------------------------------------------------

/**
 * @brief   Creates a dynamic-extent span from a pointer and element count.
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T     Element type.
 * @param[in] ptr   Pointer to the first element.
 * @param[in] count Number of elements.
 * @return  Span<T, dynamic_extent>.
 */
template<class T>
constexpr auto makeSpan(T* ptr, std::size_t count) noexcept -> Span<T>
{
    return Span<T>(ptr, count);
}

/**
 * @brief   Creates a static-extent span from a C-style array.
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T   Element type.
 * @tparam  N   Array size, deduced automatically.
 * @param[in] arr Reference to the array.
 * @return  Span<T, N>.
 */
template<class T, std::size_t N>
constexpr auto makeSpan(T (&arr)[N]) noexcept -> Span<T, N>  // NOLINT(modernize-avoid-c-arrays)
{
    return Span<T, N>(arr);
}

/**
 * @brief   Creates a static-extent span from a mutable `std::array`.
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T   Element type.
 * @tparam  N   Array size, deduced automatically.
 * @param[in] arr Reference to the array.
 * @return  Span<T, N>.
 */
template<class T, std::size_t N>
constexpr auto makeSpan(std::array<T, N>& arr) noexcept -> Span<T, N>
{
    return Span<T, N>(arr);
}

/**
 * @brief   Creates a static-extent span from a const `std::array`.
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T   Element type.
 * @tparam  N   Array size, deduced automatically.
 * @param[in] arr Reference to the const array.
 * @return  Span<const T, N>.
 */
template<class T, std::size_t N>
constexpr auto makeSpan(const std::array<T, N>& arr) noexcept -> Span<const T, N>
{
    return Span<const T, N>(arr);
}

// -----------------------------------------------------------------------------
// Byte views
// -----------------------------------------------------------------------------

/**
 * @brief   Reinterprets a span as a read-only view of raw bytes.
 * @details The returned span covers exactly `s.size_bytes()` bytes.
 *
 * @note    MISRA C++ deviation: `reinterpret_cast` is used here to obtain a
 *          byte-level view of typed memory. This is the only standards-conforming
 *          way to implement this utility; the cast is always safe because
 *          `std::byte` aliasing is permitted by the C++ standard ([basic.lval]).
 *
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T   Element type of the source span.
 * @tparam  E   Extent of the source span.
 * @param[in] s Source span.
 * @return  Span<const std::byte> with size `s.size_bytes()`.
 */
template<class T, std::size_t E>
constexpr Span<const std::byte,
               (E == dynamic_extent ? dynamic_extent : (sizeof(T) * E))>
as_bytes(Span<T, E> s) noexcept
{
    // cppcheck-suppress [misra-c++2023-8.2.2]
    return {reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
}

/**
 * @brief   Reinterprets a span as a writable view of raw bytes.
 * @details Only available when @p T is not const-qualified.
 *          The returned span covers exactly `s.size_bytes()` bytes.
 *
 * @note    MISRA C++ deviation: see `as_bytes` for rationale.
 *
 * @ingroup HELIOS_SYSLIB_SPAN
 *
 * @tparam  T   Non-const element type of the source span.
 * @tparam  E   Extent of the source span.
 * @param[in] s Source span.
 * @return  Span<std::byte> with size `s.size_bytes()`.
 */
template<class T, std::size_t E>
constexpr Span<std::byte,
               (E == dynamic_extent ? dynamic_extent : (sizeof(T) * E))>
as_writable_bytes(Span<T, E> s) noexcept
{
    static_assert(!std::is_const<T>::value,
                  "as_writable_bytes requires a non-const element type");
    // cppcheck-suppress [misra-c++2023-8.2.2]
    return {reinterpret_cast<std::byte*>(s.data()), s.size_bytes()};
}

} // namespace hel

#endif // HELIOS_SYSLIB_SPAN_HPP_
