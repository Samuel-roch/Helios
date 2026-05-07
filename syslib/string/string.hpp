/**
 ******************************************************************************
 * @file    string.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_SYSLIB_STRING
 * @brief   Fixed-capacity null-terminated mutable string.
 *
 * @details
 *   - @ref String     — non-template base class; holds a pointer to a buffer
 *     supplied by the derived class and implements all string operations.
 *   - @ref StringData — template derived class that owns a stack buffer of
 *     exactly `N` bytes; the only way to allocate a string.
 *   - No heap allocation occurs at any point.
 *   - Thread-safety: concurrent reads are safe. Concurrent write access on the
 *     same instance must be serialised by the caller.
 */

#ifndef HELIOS_SYSLIB_STRING_HPP_
#define HELIOS_SYSLIB_STRING_HPP_

#include <stddef.h>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <limits>
#include <type_traits>
#include "hel_bytearray"

namespace hel
{

// =============================================================================
// StrNumBase
// =============================================================================

/**
 * @brief   Numeric base or floating-point notation for @ref String::addNum.
 * @ingroup HELIOS_SYSLIB_STRING
 */
enum class StrNumBase : uint8_t
{
  Decimal, /*!< Base-10 decimal notation (default). */
  Scientific, /*!< Scientific notation (e.g. `1.23e+04`). Floating-point only. */
  HexFloat, /*!< Hexadecimal floating-point (e.g. `0x1.8p+1`). Floating-point only. */
  Hexadecimal, /*!< Base-16 with `0x` prefix (e.g. `0xFF`). Integer only. */
  Octal, /*!< Base-8 with `0` prefix (e.g. `0377`). Integer only. */
};

// =============================================================================
// String
// =============================================================================

/**
 * @class   String
 * @brief   Non-template base for fixed-capacity null-terminated mutable strings.
 * @ingroup HELIOS_SYSLIB_STRING
 *
 * @details
 *   - Inherits from @ref ByteArray, reinterpreting the byte buffer as character data.
 *   - Holds a pointer to a buffer owned by the derived @ref StringData<N> and
 *     implements all string operations against it.
 *   - Copy and move constructors are deleted — `String` is not independently
 *     instantiable. Use @ref StringData<N> instead.
 *   - Copy and move assignment operators copy the content into this instance's
 *     own buffer; they never rebind the buffer pointer.
 *   - All mutating operations silently clamp to the available capacity; no
 *     exception is thrown and no truncation error is returned.
 *   - Thread-safety: concurrent reads are safe. Concurrent write access on the
 *     same instance must be serialised by the caller.
 */
class String :
  public ByteArray
{
public:
  /** @brief Sentinel value returned by @ref find when no match is found. */
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  String() = default;

  String(const String&) = delete;

  String(String&&) = delete;

  /**
   * @brief   Protected constructor — initialises the base from a derived buffer.
   * @details Called exclusively by @ref StringData<N>. Sets `size` to 0 and
   *          null-terminates the buffer.
   *
   * @param[in] arr owned by the derived class; must not be `nullptr`.
   */
  // cppcheck-suppress [misra-c++2023-8.2.2]
  template<std::size_t N>
  constexpr String(char (&arr)[N]) noexcept
      :
      ByteArray(reinterpret_cast<uint8_t*>(arr), 0U, N)
  {
    static_assert(N > 1, "Buffer size must be greater than one to hold a null terminator");
    m_count = 0U;
    m_capacity = N;
  }

  explicit String(char* buf, std::size_t capacity) noexcept
      :
      ByteArray(reinterpret_cast<uint8_t*>(buf), 0U, capacity)
  {
    assert(capacity > 1U); // Must have space for at least one character + null terminator
    m_count = 0U;
    m_capacity = capacity;
  }

  /**
   * @brief   Construct a string view over a null-terminated C-string.
   * @details The view is read-only; mutating operations will clamp to the
   *          existing content and never exceed the original length.  The
   *          buffer is not owned by this instance and must remain valid for
   *          the lifetime of the view.
   *
   * @param[in] buf Null-terminated C-string; must not be `nullptr`.
   */
  explicit String(const char* buf) noexcept
      :
      ByteArray()
  {
    m_ptr = reinterpret_cast<uint8_t*>(const_cast<char*>(buf));
    m_capacity = boundedStrlen(buf, std::numeric_limits<std::size_t>::max() - 1U) + 1U;
    m_count = m_capacity - 1U;
  }

  explicit String(const char* buf, std::size_t count) noexcept
      :
      ByteArray()
  {
    assert(buf != nullptr);
    m_ptr = reinterpret_cast<uint8_t*>(const_cast<char*>(buf));
    m_capacity = count + 1U;
    m_count = count;
  }

  /**
   * @brief   Deleted constructor — disallow construction from `nullptr`. Use the
   *          default constructor for an empty string instead.
   */
  String(std::nullptr_t) = delete;

  // -------------------------------------------------------------------------
  // Observers
  // -------------------------------------------------------------------------

  /**
   * @brief   Returns a null-terminated pointer to the character data.
   * @return  Const pointer to the internal buffer; never `nullptr`.
   */
  [[nodiscard]] const char* c_str() const noexcept
  {
    return cptr();
  }

  /**
   * @brief   Returns a pointer to the first character.
   * @return  Const pointer to the internal buffer; never `nullptr`.
   */
  [[nodiscard]] const char* data() const noexcept
  {
    return cptr();
  }

  /**
   * @brief   Returns the maximum number of characters that can be stored.
   * @return  Total buffer size minus one (reserved for the null terminator).
   */
  [[nodiscard]] std::size_t capacity() const noexcept
  {
    return m_capacity - 1U;
  }

  /**
   * @brief   Returns `true` when no further characters can be appended.
   * @return  `size() == capacity()`.
   */
  [[nodiscard]] bool full() const noexcept
  {
    return m_count >= m_capacity - 1U;
  }

  /**
   * @brief   Returns the number of characters that can still be appended.
   * @return  `capacity() - size()`.
   */
  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return (m_capacity - 1U) - m_count;
  }

  /**
   * @brief   Returns the raw byte representation of the string data.
   * @details Useful for passing string content to byte-oriented APIs.
   * @return  Const pointer to character data reinterpreted as bytes.
   */
  [[nodiscard]] const uint8_t* c_data() const noexcept
  {
    return m_ptr;
  }

  // -------------------------------------------------------------------------
  // Element access
  // -------------------------------------------------------------------------

  /**
   * @brief   Returns the character at position @p index.
   * @details Asserts that @p index is within bounds.
   *
   * @param[in] index Zero-based character index; must be less than `size()`.
   * @return  Character value.
   */
  [[nodiscard]] char operator[](std::size_t index) const noexcept
  {
    assert(index < m_count);
    return static_cast<char>(m_ptr[index]);
  }

  /**
   * @brief   Returns the first character. Asserts that the string is not empty.
   * @return  First character.
   */
  [[nodiscard]] char front() const noexcept
  {
    assert(!empty());
    return static_cast<char>(m_ptr[0]);
  }

  /**
   * @brief   Returns the last character. Asserts that the string is not empty.
   * @return  Last character.
   */
  [[nodiscard]] char back() const noexcept
  {
    assert(!empty());
    return static_cast<char>(m_ptr[m_count - 1U]);
  }

  // -------------------------------------------------------------------------
  // Iterators
  // -------------------------------------------------------------------------

  /** @brief Const begin iterator. */
  [[nodiscard]] const char* begin() const noexcept
  {
    return cptr();
  }
  /** @brief Const end iterator. */
  [[nodiscard]] const char* end() const noexcept
  {
    return cptr() + m_count;
  }
  /** @brief Const begin iterator. */
  [[nodiscard]] const char* cbegin() const noexcept
  {
    return cptr();
  }
  /** @brief Const end iterator. */
  [[nodiscard]] const char* cend() const noexcept
  {
    return cptr() + m_count;
  }

  // -------------------------------------------------------------------------
  // Search
  // -------------------------------------------------------------------------

  /**
   * @brief   Finds the first occurrence of character @p c at or after @p from.
   *
   * @param[in] c    Character to search for.
   * @param[in] from Starting search index; defaults to 0.
   * @return  Index of the first match, or @ref npos if not found.
   */
  [[nodiscard]] std::size_t find(char c, std::size_t from = 0U) const noexcept
  {
    const char *const p = cptr();
    for ( std::size_t i = from; i < m_count; ++i )
    {
      if (p[i] == c)
      {
        return i;
      }
    }
    return npos;
  }

  /**
   * @brief   Finds the first occurrence of the null-terminated sub-string @p sub
   *          at or after @p from.
   *
   * @param[in] sub  Null-terminated sub-string to search for; if `nullptr` or
   *                 empty, returns @p from.
   * @param[in] from Starting search index; defaults to 0.
   * @return  Index of the first match, or @ref npos if not found.
   */
  [[nodiscard]] std::size_t find(
    const char* sub, std::size_t from = 0U) const noexcept
  {
    if (sub == nullptr || *sub == '\0')
    {
      return from;
    }
    const std::size_t sub_len = boundedStrlen(sub, m_count + 1U);
    if (sub_len > m_count)
    {
      return npos;
    }
    const std::size_t limit = m_count - sub_len;
    for ( std::size_t i = from; i <= limit; ++i )
    {
      if (memcmp(cptr() + i, sub, sub_len) == 0)
      {
        return i;
      }
    }
    return npos;
  }

  /**
   * @brief   Finds the first occurrence of the sub-string @p sub at or after @p from.
   *
   * @param[in] sub  Sub-string to search for.
   * @param[in] from Starting search index; defaults to 0.
   * @return  Index of the first match, or @ref npos if not found.
   */
  [[nodiscard]] std::size_t find(
    const String& sub, std::size_t from = 0U) const noexcept
  {
    if (sub.m_count == 0U)
    {
      return from;
    }
    if (sub.m_count > m_count)
    {
      return npos;
    }
    const std::size_t limit = m_count - sub.m_count;
    for ( std::size_t i = from; i <= limit; ++i )
    {
      if (memcmp(cptr() + i, sub.cptr(), sub.m_count) == 0)
      {
        return i;
      }
    }
    return npos;
  }

  /**
   * @brief   Parses the string according to the given format and arguments, like `sscanf`.
   * @details
   *   - Behaves like `sscanf(m_ptr, format, ...)` to extract values from the string.
   *   - The return value is the number of successfully parsed items, or a negative
   *     value if @p format is `nullptr`.
   *
   * @param[in] format  Null-terminated format string (must not be `nullptr`).
   * @param[in] ...     Additional arguments as required by the format string.
   * @return  Number of items successfully parsed, or -1 if @p format is `nullptr`.
   */
  [[nodiscard]]
  int scanf(const char* format, ...) const noexcept
  {
    if (format == nullptr)
    {
      return -1;
    }
    va_list args;
    va_start(args, format);
    const char *cptr = this->cptr();
    int result = vsscanf(cptr, format, args);
    va_end(args);
    return result;
  }

  /**
   * @brief   Formats content into the string using printf-style formatting.
   * @details
   *   - Behaves like `snprintf(buf, capacity+1, format, ...)`.
   *   - Clears the string before formatting.
   *
   * @param[in] format  Null-terminated format string (must not be `nullptr`).
   * @param[in] ...     Additional arguments as required by the format string.
   * @return  Number of characters written (excluding null terminator), or -1 on error.
   */
  int printf(const char* format, ...) noexcept
  {
    if (format == nullptr)
    {
      return -1;
    }

    clear();

    va_list args;
    va_start(args, format);
    const int result = vsnprintf(cptr(), m_capacity, format, args);

    if (result < 0)
    {
      m_count = 0U;
      m_ptr[0] = 0U;
    }
    else
    {
      sync();
    }

    va_end(args);
    return result;
  }

  /**
   * @brief   Returns `true` when this string contains @p sub as a sub-string.
   * @param[in] sub Null-terminated sub-string to search for.
   * @return  `find(sub) != npos`.
   */
  [[nodiscard]]
  bool contains(const char* sub) const noexcept
  {
    return find(sub) != npos;
  }

  /**
   * @brief   Returns `true` when this string contains @p sub as a sub-string.
   * @param[in] sub Sub-string to search for.
   * @return  `find(sub) != npos`.
   */
  [[nodiscard]]
  bool contains(const String& sub) const noexcept
  {
    return find(sub) != npos;
  }

  /**
   * @brief   Returns `true` when this string begins with @p prefix.
   * @param[in] prefix Null-terminated prefix to test; `nullptr` always matches.
   * @return  `true` if the first `strlen(prefix)` characters match.
   */
  [[nodiscard]] bool startsWith(const char* prefix) const noexcept
  {
    if (prefix == nullptr || *prefix == '\0')
    {
      return true;
    }
    const std::size_t len = boundedStrlen(prefix, m_count + 1U);
    if (len > m_count)
    {
      return false;
    }
    return memcmp(cptr(), prefix, len) == 0;
  }

  /**
   * @brief   Returns `true` when this string begins with @p prefix.
   * @param[in] prefix Prefix to test.
   * @return  `true` if the first `prefix.size()` characters match.
   */
  [[nodiscard]] bool startsWith(const String& prefix) const noexcept
  {
    if (prefix.m_count > m_count)
    {
      return false;
    }
    return memcmp(cptr(), prefix.cptr(), prefix.m_count) == 0;
  }

  /**
   * @brief   Returns `true` when this string ends with @p suffix.
   * @param[in] suffix Null-terminated suffix to test; `nullptr` always matches.
   * @return  `true` if the last `strlen(suffix)` characters match.
   */
  [[nodiscard]] bool endsWith(const char* suffix) const noexcept
  {
    if (suffix == nullptr || *suffix == '\0')
    {
      return true;
    }
    const std::size_t len = boundedStrlen(suffix, m_count + 1U);
    if (len > m_count)
    {
      return false;
    }
    return memcmp(cptr() + (m_count - len), suffix, len) == 0;
  }

  /**
   * @brief   Returns `true` when this string ends with @p suffix.
   * @param[in] suffix Suffix to test.
   * @return  `true` if the last `suffix.size()` characters match.
   */
  [[nodiscard]] bool endsWith(const String& suffix) const noexcept
  {
    if (suffix.m_count > m_count)
    {
      return false;
    }
    return memcmp(cptr() + (m_count - suffix.m_count), suffix.cptr(),
        suffix.m_count) == 0;
  }

  /**
   * @brief   Removes characters from the start and end of the string that match
   *          any character in @p chars.
   *
   * @param[in] chars Null-terminated set of characters to trim; if `nullptr` or
   *                  empty, no trimming occurs.
   * @return  `true` if any characters were removed, `false` otherwise.
   */
  [[nodiscard]] bool trim(const char* chars) noexcept
  {
    if (chars == nullptr || *chars == '\0')
    {
      return false;
    }
    char *const p = cptr();
    std::size_t start = 0U;
    while (start < m_count && strchr(chars, p[start]) != nullptr)
    {
      ++start;
    }
    std::size_t end = m_count;
    while (end > start && strchr(chars, p[end - 1U]) != nullptr)
    {
      --end;
    }
    if (start == 0U && end == m_count)
    {
      return false;
    }
    const std::size_t new_size = end - start;
    if (new_size > 0U)
    {
      memmove(p, p + start, new_size);
    }
    m_count = new_size;
    m_ptr[m_count] = 0U;
    return true;
  }

  String section(std::size_t start, std::size_t length) const noexcept
  {
    if (start >= m_count)
    {
      return String();
    }
    const std::size_t max_len = m_count - start;
    const std::size_t len = (length < max_len) ? length : max_len;
    return String(cptr() + start, len);
  }

  String substring(const char* start_delim, const char* end_delim) const noexcept
  {
    const std::size_t start_pos = find(start_delim);
    if (start_pos == npos)
    {
      return String();
    }
    const std::size_t start_end = start_pos + boundedStrlen(start_delim, m_count - start_pos + 1U);
    if (start_end > m_count)
    {
      return String();
    }
    const std::size_t end_pos = find(end_delim, start_end);
    if (end_pos == npos)
    {
      return String();
    }
    return String(cptr() + start_end, end_pos - start_end);
  }
  // -------------------------------------------------------------------------
  // Numeric parsing
  // -------------------------------------------------------------------------

  /**
   * @brief   Parses the string as a signed 32-bit decimal integer.
   * @details
   *   - Accepts an optional leading `+` or `-` sign followed by ASCII digits.
   *   - No leading whitespace is consumed.
   *   - Leaves @p out unchanged on any invalid character or overflow.
   *
   * @param[out] out Receives the parsed value on success.
   * @return  `true` on success, `false` on parse error or overflow.
   */
  [[nodiscard]] bool toInt(int32_t& out) const noexcept
  {
    if (m_ptr == nullptr || m_count == 0U)
    {
      return false;
    }
    const char *const p = cptr();
    std::size_t i = 0U;
    bool negative = false;
    if (p[0] == '-')
    {
      negative = true;
      ++i;
    }
    else if (p[0] == '+')
    {
      ++i;
    }
    if (i >= m_count)
    {
      return false;
    }
    int64_t value = 0;
    for ( ; i < m_count; ++i )
    {
      if (p[i] < '0' || p[i] > '9')
      {
        return false;
      }
      value = value * 10 + static_cast<int64_t>(p[i] - '0');
      if (value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1)
      {
        return false;
      }
    }
    if (negative)
    {
      if (value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1)
      {
        return false;
      }
      out = static_cast<int32_t>(-value);
    }
    else
    {
      if (value > std::numeric_limits<int32_t>::max())
      {
        return false;
      }
      out = static_cast<int32_t>(value);
    }
    return true;
  }

  /**
   * @brief   Parses the string as an unsigned 32-bit decimal integer.
   * @details
   *   - Accepts an optional leading `+` sign followed by ASCII digits.
   *   - Returns `false` on any invalid character or overflow.
   *
   * @param[out] out Receives the parsed value on success.
   * @return  `true` on success, `false` on parse error or overflow.
   */
  [[nodiscard]] bool toUInt(uint32_t& out) const noexcept
  {
    if (m_ptr == nullptr || m_count == 0U)
    {
      return false;
    }
    const char *const p = cptr();
    std::size_t i = 0U;
    if (p[0] == '+')
    {
      ++i;
    }
    if (i >= m_count)
    {
      return false;
    }
    uint64_t value = 0U;
    for ( ; i < m_count; ++i )
    {
      if (p[i] < '0' || p[i] > '9')
      {
        return false;
      }
      value = value * 10U + static_cast<uint64_t>(p[i] - '0');
      if (value > std::numeric_limits<uint32_t>::max())
      {
        return false;
      }
    }
    out = static_cast<uint32_t>(value);
    return true;
  }

  /**
   * @brief   Parses the string as a single-precision floating-point number.
   * @details
   *   - Accepts any format recognised by `strtof`.
   *   - Returns `false` when the string is longer than 32 characters or when
   *     `strtof` cannot parse a value.
   *
   * @param[out] out Receives the parsed value on success.
   * @return  `true` on success, `false` on parse error.
   */
  [[nodiscard]] bool toFloat(float& out) const noexcept
  {
    if (m_ptr == nullptr || m_count == 0U)
    {
      return false;
    }
    constexpr std::size_t kMaxLen = 32U;
    if (m_count >= kMaxLen)
    {
      return false;
    }
    char buf[kMaxLen + 1U];
    memcpy(buf, cptr(), m_count);
    buf[m_count] = '\0';
    char *end = nullptr;
    const float val = strtof(buf, &end);
    if (end == buf)
    {
      return false;
    }
    out = val;
    return true;
  }

  // -------------------------------------------------------------------------
  // Assignment
  // -------------------------------------------------------------------------

  /**
   * @brief   Copy assignment — copies content into this instance's own buffer.
   * @details Content is truncated to `capacity()` if the source is larger.
   *          The buffer pointer is never rebound.
   *
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& operator=(const String& other) noexcept
  {
    if (this != &other)
    {
      clear();
      append(other.data(), other.m_count);
      m_precision = other.m_precision;
    }
    return *this;
  }

  /**
   * @brief   Move assignment — equivalent to copy assignment.
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& operator=(String&& other) noexcept
  {
    return *this = static_cast<const String&>(other);
  }

  /**
   * @brief   Assigns (replaces) content from a null-terminated C string.
   * @param[in] str Source; if `nullptr` the string is cleared.
   * @return  Reference to this string.
   */
  String& operator=(const char* str) noexcept
  {
    clear();
    return append(str);
  }

  // -------------------------------------------------------------------------
  // Modifiers
  // -------------------------------------------------------------------------

  /**
   * @brief   Clears the string content.
   * @details After this call `size() == 0` and `c_str()` returns `""`.
   */
  void clear() noexcept
  {
    while (m_count > 0U)
    {
      --m_count;
      m_ptr[m_count] = 0U;
    }
  }

  /**
   * @brief   Replaces the entire content with @p str.
   * @details Truncates to `capacity()` if necessary.
   *
   * @param[in] str Null-terminated source; if `nullptr` the string is cleared.
   * @return  Reference to this string.
   */
  String& assign(const char* str) noexcept
  {
    clear();
    return append(str);
  }

  /**
   * @brief   Replaces the entire content with @p other.
   * @details Truncates to `capacity()` if necessary.
   *
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& assign(const String& other) noexcept
  {
    return *this = other;
  }

  /**
   * @brief   Appends a single character.
   * @details The character is silently discarded when the buffer is full.
   *
   * @param[in] c Character to append.
   * @return  Reference to this string.
   */
  String& append(char c) noexcept
  {
    if (m_count < m_capacity - 1U)
    {
      m_ptr[m_count] = static_cast<uint8_t>(c);
      ++m_count;
      m_ptr[m_count] = 0U;
    }
    return *this;
  }

  /**
   * @brief   Appends characters from a null-terminated C string.
   * @details Truncates to `remaining()` characters if necessary.
   *
   * @param[in] str Source; if `nullptr` this is a no-op.
   * @return  Reference to this string.
   */
  String& append(const char* str) noexcept
  {
    if (str == nullptr)
    {
      return *this;
    }
    const std::size_t avail = m_capacity - 1U - m_count;
    const std::size_t len = boundedStrlen(str, avail + 1U);
    return append(str, len);
  }

  /**
   * @brief   Appends @p len characters from @p str.
   * @details Truncates to `remaining()` characters if necessary.
   *
   * @param[in] str Pointer to source characters; must not be `nullptr`.
   * @param[in] len Number of characters to copy.
   * @return  Reference to this string.
   */
  String& append(const char* str, std::size_t len) noexcept
  {
    const std::size_t avail = m_capacity - 1U - m_count;
    const std::size_t to_copy = (len < avail) ? len : avail;
    if (to_copy == 0U)
    {
      return *this;
    }
    memcpy(cptr() + m_count, str, to_copy);
    m_count += to_copy;
    m_ptr[m_count] = 0U;
    return *this;
  }

  /**
   * @brief   Appends the content of another @ref String.
   * @details Truncates to `remaining()` characters if necessary.
   *
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& append(const String& other) noexcept
  {
    return append(other.data(), other.m_count);
  }

  /**
   * @brief   Appends a null-terminated C string (streaming operator).
   * @param[in] str Source.
   * @return  Reference to this string.
   */
  String& operator+=(const char* str) noexcept
  {
    return append(str);
  }

  /**
   * @brief   Appends another @ref String (streaming operator).
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& operator+=(const String& other) noexcept
  {
    return append(other);
  }

  /**
   * @brief   Appends a null-terminated C string (stream syntax).
   * @param[in] str Source.
   * @return  Reference to this string.
   */
  String& operator<<(const char* str) noexcept
  {
    return append(str);
  }

  /**
   * @brief   Appends another @ref String (stream syntax).
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& operator<<(char* str) noexcept
  {
    return append(str);
  }

  /**
   * @brief   Appends another @ref String (stream syntax).
   * @param[in] other Source string.
   * @return  Reference to this string.
   */
  String& operator<<(const String& other) noexcept
  {
    return append(other);
  }

  // -------------------------------------------------------------------------
  // Number formatting
  // -------------------------------------------------------------------------

  /**
   * @brief   Sets the decimal precision used for floating-point formatting.
   * @details Only affects @ref addNum when @p T is a floating-point type.
   *          Clamped to the range [0, 15].
   *
   * @param[in] p Desired number of digits after the decimal point.
   * @return  Reference to this string.
   */
  String& setPrecision(uint8_t p) noexcept
  {
    m_precision = (p <= 15U) ? p : 15U;
    return *this;
  }

  /**
   * @brief   Appends the numeric representation of @p n.
   * @details
   *   - For integral types, @p f selects the base.
   *   - For floating-point types, @p f selects the notation and
   *     @ref setPrecision controls digits after the decimal point.
   *   - Output is truncated to `remaining()` characters if necessary.
   *
   * @tparam  T   Integral or floating-point type.
   * @param[in] n  Value to format.
   * @param[in] f  Numeric base / notation; defaults to @ref StrNumBase::Decimal.
   * @return  Reference to this string.
   */
  template<typename T>
  String& addNum(T n, StrNumBase f = StrNumBase::Decimal) noexcept
  {
    static_assert(
        std::is_floating_point<T>::value || std::is_integral<T>::value,
        "addNum requires an integral or floating-point type");

    const std::size_t avail = m_capacity - m_count;
    if (avail <= 1U)
    {
      return *this;
    }
    const int cap = static_cast<int>(avail);
    char *const pos = cptr() + m_count;
    int len = 0;

    if constexpr (std::is_floating_point_v<T>)
    {
      char fmt[10];
      const char *base_fmt = getFormat<T>(f);
      snprintf(fmt, sizeof(fmt), "%%.%d%s", static_cast<int>(m_precision),
          base_fmt + 1U);
      len = snprintf(pos, static_cast<std::size_t>(cap), fmt, n);
    }
    else
    {
      len = snprintf(pos, static_cast<std::size_t>(cap), getFormat<T>(f),
          n);
    }

    if (len > 0)
    {
      const std::size_t written =
          (static_cast<std::size_t>(len) < static_cast<std::size_t>(cap - 1)) ?
              static_cast<std::size_t>(len) : static_cast<std::size_t>(cap - 1);
      m_count += written;
    }
    return *this;
  }

  /**
   * @brief   Appends the numeric representation of @p n (stream syntax).
   * @details Uses @ref StrNumBase::Decimal. Use @ref addNum for other bases.
   *
   * @tparam  T   Integral or floating-point type.
   * @param[in] n  Value to append.
   * @return  Reference to this string.
   */
  template<typename T>
  String& operator<<(T n) noexcept
  {
    return addNum(n);
  }

  // -------------------------------------------------------------------------
  // In-place transformations
  // -------------------------------------------------------------------------

  /**
   * @brief   Converts all ASCII lowercase letters to uppercase in place.
   * @return  Reference to this string.
   */
  String& toUpper() noexcept
  {
    char *const p = cptr();
    for ( std::size_t i = 0U; i < m_count; ++i )
    {
      p[i] = static_cast<char>(toupper(static_cast<unsigned char>(p[i])));
    }
    return *this;
  }

  /**
   * @brief   Converts all ASCII uppercase letters to lowercase in place.
   * @return  Reference to this string.
   */
  String& toLower() noexcept
  {
    char *const p = cptr();
    for ( std::size_t i = 0U; i < m_count; ++i )
    {
      p[i] = static_cast<char>(tolower(static_cast<unsigned char>(p[i])));
    }
    return *this;
  }

  /**
   * @brief   Removes leading and trailing ASCII whitespace characters in place.
   * @return  Reference to this string.
   */
  String& trim() noexcept
  {
    char *const p = cptr();
    std::size_t start = 0U;
    while (start < m_count
        && isspace(static_cast<unsigned char>(p[start])) != 0)
    {
      ++start;
    }
    std::size_t end = m_count;
    while (end > start
        && isspace(static_cast<unsigned char>(p[end - 1U])) != 0)
    {
      --end;
    }
    m_count = end - start;
    if (start > 0U)
    {
      memmove(p, p + start, m_count);
    }
    m_ptr[m_count] = 0U;
    return *this;
  }

  /**
   * @brief   Reverses the string content in place.
   * @return  Reference to this string.
   */
  String& reverse() noexcept
  {
    if (m_count >= 2U)
    {
      for ( std::size_t i = 0U, j = m_count - 1U; i < j; ++i, --j )
      {
        const uint8_t tmp = m_ptr[i];
        m_ptr[i] = m_ptr[j];
        m_ptr[j] = tmp;
      }
    }
    return *this;
  }

  /**
   * @brief   Replaces every occurrence of character @p from with @p to in place.
   *
   * @param[in] from Character to search for.
   * @param[in] to   Replacement character.
   * @return  Reference to this string.
   */
  String& replace(char from, char to) noexcept
  {
    char *const p = cptr();
    for ( std::size_t i = 0U; i < m_count; ++i )
    {
      if (p[i] == from)
      {
        p[i] = to;
      }
    }
    return *this;
  }

  // -------------------------------------------------------------------------
  // Hex dump
  // -------------------------------------------------------------------------

  /**
   * @brief   Appends a hex dump of @p len bytes from @p data.
   * @details
   *   - Each byte is formatted as two uppercase hex digits.
   *   - When @p sep is not `'\0'`, it is inserted between consecutive bytes.
   *   - Output is truncated when the buffer is full.
   *
   * @param[in] data Pointer to byte data; must not be `nullptr` when `len > 0`.
   * @param[in] len  Number of bytes to format.
   * @param[in] sep  Separator character between bytes; use `'\0'` for none.
   * @return  Reference to this string.
   */
  String& appendHex(
    const uint8_t* data, std::size_t len, char sep = ' ') noexcept
  {
    static const char kHex[] = "0123456789ABCDEF";
    for ( std::size_t i = 0U; i < len; ++i )
    {
      if (i > 0U && sep != '\0')
      {
        append(sep);
      }
      if (remaining() < 2U)
      {
        break;
      }
      append(kHex[(data[i] >> 4U) & 0x0FU]);
      append(kHex[data[i] & 0x0FU]);
    }
    return *this;
  }

  // -------------------------------------------------------------------------
  // Raw buffer access
  // -------------------------------------------------------------------------

  /**
   * @brief   Returns a mutable pointer to the internal buffer.
   * @details Allows in-place modification via external C APIs, e.g.:
   *          `snprintf(s.raw(), s.capacity() + 1, ...)`.
   *          Call @ref sync after writing to keep `size()` consistent.
   * @return  Mutable pointer to the internal buffer of `capacity() + 1` bytes.
   */
  [[nodiscard]] char* raw() noexcept
  {
    return cptr();
  }

  /**
   * @brief   Recomputes `size()` after external writes via @ref raw.
   * @details Scans the buffer for the first null terminator up to `capacity()`
   *          characters and updates `m_count`. Always ensures the buffer is
   *          null-terminated within bounds.
   */
  void sync() noexcept
  {
    m_count = 0U;
    const std::size_t cap = m_capacity - 1U;
    const char *const p = cptr();
    while (m_count < cap && p[m_count] != '\0')
    {
      ++m_count;
    }
    m_ptr[m_count] = 0U;
  }

  static std::size_t boundedStrlen(
    const char* str, std::size_t max_len) noexcept
  {
    std::size_t n = 0U;
    while (n < max_len && str[n] != '\0')
    {
      ++n;
    }
    return n;
  }

private:
  uint8_t m_precision { 6 }; /*!< Floating-point precision for addNum (0–15). */



  char* cptr() noexcept
  {
    // cppcheck-suppress [misra-c++2023-8.2.2]
    return reinterpret_cast<char*>(m_ptr);
  }

  const char* cptr() const noexcept
  {
    // cppcheck-suppress [misra-c++2023-8.2.2]
    return reinterpret_cast<const char*>(m_ptr);
  }

  template<typename T>
  static constexpr const char* getFormat(StrNumBase f) noexcept
  {
    if constexpr (std::is_floating_point_v<T>)
    {
      if (f == StrNumBase::Scientific)
      {
        return "%e";
      }
      if (f == StrNumBase::HexFloat)
      {
        return "%a";
      }
      return "%f";
    }
    else if constexpr (std::is_signed_v<T>)
    {
      if (f == StrNumBase::Hexadecimal)
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "0x%" PRIX8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "0x%" PRIX16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "0x%" PRIX32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "0x%" PRIX64;
        }
      }
      else if (f == StrNumBase::Octal)
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "0%" PRIo8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "0%" PRIo16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "0%" PRIo32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "0%" PRIo64;
        }
      }
      else
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "%" PRId8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "%" PRId16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "%" PRId32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "%" PRId64;
        }
      }
      return "%" PRId64;
    }
    else
    {
      if (f == StrNumBase::Hexadecimal)
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "0x%" PRIX8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "0x%" PRIX16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "0x%" PRIX32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "0x%" PRIX64;
        }
      }
      else if (f == StrNumBase::Octal)
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "0%" PRIo8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "0%" PRIo16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "0%" PRIo32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "0%" PRIo64;
        }
      }
      else
      {
        if constexpr (sizeof(T) == 1U)
        {
          return "%" PRIu8;
        }
        if constexpr (sizeof(T) == 2U)
        {
          return "%" PRIu16;
        }
        if constexpr (sizeof(T) == 4U)
        {
          return "%" PRIu32;
        }
        if constexpr (sizeof(T) == 8U)
        {
          return "%" PRIu64;
        }
      }
      return "%" PRIu64;
    }
  }
};

// =============================================================================
// Comparison operators for String
// =============================================================================

/**
 * @brief   Lexicographic equality comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `true` if both strings have the same length and identical content.
 */
inline bool operator==(const String& lhs, const String& rhs) noexcept
{
  if (lhs.size() != rhs.size())
  {
    return false;
  }
  return memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

/**
 * @brief   Equality comparison with a null-terminated C string.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Null-terminated right-hand string; `nullptr` matches empty.
 * @return  `true` if content is identical.
 */
inline bool operator==(const String& lhs, const char* rhs) noexcept
{
  if (rhs == nullptr)
  {
    return lhs.empty();
  }
  const std::size_t rlen = String::boundedStrlen(rhs, lhs.size() + 1U);
  if (lhs.size() != rlen)
  {
    return false;
  }
  return memcmp(lhs.data(), rhs, rlen) == 0;
}

/** @brief Symmetric overload. */
inline bool operator==(const char* lhs, const String& rhs) noexcept
{
  return rhs == lhs;
}

/**
 * @brief   Lexicographic inequality comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `!(lhs == rhs)`.
 */
inline bool operator!=(const String& lhs, const String& rhs) noexcept
{
  return !(lhs == rhs);
}

/** @brief Inequality with null-terminated C string. */
inline bool operator!=(const String& lhs, const char* rhs) noexcept
{
  return !(lhs == rhs);
}

/** @brief Inequality with null-terminated C string (symmetric). */
inline bool operator!=(const char* lhs, const String& rhs) noexcept
{
  return !(rhs == lhs);
}

/**
 * @brief   Lexicographic less-than comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `true` if @p lhs is lexicographically less than @p rhs.
 */
inline bool operator<(const String& lhs, const String& rhs) noexcept
{
  const std::size_t min_len =
      (lhs.size() < rhs.size()) ? lhs.size() : rhs.size();
  const int cmp = memcmp(lhs.data(), rhs.data(), min_len);
  if (cmp != 0)
  {
    return cmp < 0;
  }
  return lhs.size() < rhs.size();
}

/**
 * @brief   Lexicographic greater-than comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `rhs < lhs`.
 */
inline bool operator>(const String& lhs, const String& rhs) noexcept
{
  return rhs < lhs;
}

/**
 * @brief   Lexicographic less-than-or-equal comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `!(rhs < lhs)`.
 */
inline bool operator<=(const String& lhs, const String& rhs) noexcept
{
  return !(rhs < lhs);
}

/**
 * @brief   Lexicographic greater-than-or-equal comparison.
 * @ingroup HELIOS_SYSLIB_STRING
 * @param[in] lhs Left-hand string.
 * @param[in] rhs Right-hand string.
 * @return  `!(lhs < rhs)`.
 */
inline bool operator>=(const String& lhs, const String& rhs) noexcept
{
  return !(lhs < rhs);
}


} // namespace hel

#endif // HELIOS_SYSLIB_STRING_HPP_
