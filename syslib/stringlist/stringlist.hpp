/**
 ******************************************************************************
 * @file    stringlist.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.1.0
 * @date    2026-04-19
 * @ingroup HELIOS_SYSLIB_STRINGLIST
 * @brief   Fixed-m_capacity list of null-terminated strings in a single flat buffer.
 *
 * @details
 *   - All strings are stored contiguously in one internal `char[N]`
 *     buffer, each separated by a null terminator.
 *   - When a string is removed, every string after it is shifted left,
 *     compacting the buffer immediately — no fragmentation.
 *   - N is the total byte budget (characters + null terminators).
 *   - No dynamic allocation; no exceptions; operations return `bool` or
 *     `npos` on failure.
 *   - All string parameters accept @ref syslib::String, so
 *     @ref syslib::String<N> objects and `const char*` literals may be
 *     passed interchangeably without explicit conversion.
 *
 * @note
 *   Iterator invalidation: any mutating operation (append, insert, remove,
 *   replace, clear) invalidates all pointers previously returned by @ref at,
 *   @ref first, and @ref last.
 */

#ifndef HELIOS_SYSLIB_STRINGLIST_HPP_
#define HELIOS_SYSLIB_STRINGLIST_HPP_

#include <cstddef>
#include <cstring>
#include <limits>

#include <hel_string>

namespace hel
{



/**
 * @brief   Fixed-N list of null-terminated strings in a flat buffer.
 * @details
 *   - Strings are packed contiguously: `"foo\0bar\0baz\0"`.
 *   - @ref remove() shifts subsequent strings left; the freed bytes are zeroed.
 *   - @ref replace() validates that the replacement fits before mutating state.
 *   - @ref at() returns `nullptr` for out-of-range indices.
 *   - All string parameters are @ref String, which accepts `const char*`,
 *     `String<N>`, and any other `String` without explicit conversion.
 *
 * @tparam T  Total byte budget for all string data including null
 *                   terminators (must be > 0).
 *
 * @ingroup HELIOS_SYSLIB_STRINGLIST
 *
 * @note  Thread-safety: concurrent reads are safe. Any write requires external
 *        synchronisation.
 */
class StringList
{

public:

  /** @brief Sentinel value returned by @ref indexOf when no match is found. */
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  // -------------------------------------------------------------------------
  // Constructor
  // -------------------------------------------------------------------------


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
  constexpr StringList(char (&arr)[N]) noexcept
      : m_buf(arr),  // Armazena ponteiro para o array original
        m_capacity(N)
  {
      static_assert(N > 0, "Buffer size must be greater than zero");
      m_count = 0;
      m_used = 0;
  }

  // -------------------------------------------------------------------------
  // Modifiers
  // -------------------------------------------------------------------------

  /**
   * @brief   Append a string at the end of the list.
   * @details Copies @p sv (including a null terminator) into the buffer
   *          immediately after the last existing entry.
   *          Accepts `const char*`, @ref String<N>, or any @ref String.
   *
   * @param[in] sv  String to copy; `sv.data()` must not be `nullptr`.
   * @return    `true` on success.
   * @return    `false` if `sv.data()` is `nullptr` or the buffer has
   *            insufficient space for `sv.size() + 1` bytes.
   */
  bool append(String& sv) noexcept
  {
    if (sv.data() == nullptr)
    {
      return false;
    }
    const std::size_t len = sv.size();
    if ((m_used + len + 1u) > m_capacity)
    {
      return false;
    }
    std::memcpy(m_buf + m_used, sv.data(), len);
    m_buf[m_used + len] = '\0';
    m_used += len + 1u;
    ++m_count;
    return true;
  }

  bool split(char delimiter) noexcept
  {
    if (m_used == 0)
    {
      return false; // Nothing to split
    }

    std::size_t newCount = 0;
    for (std::size_t i = 0; i < m_used; ++i)
    {
      if (m_buf[i] == delimiter)
      {
        m_buf[i] = '\0'; // Replace delimiter with null terminator
        ++newCount;
      }
    }
    if (m_buf[m_used - 1] != '\0')
    {
      ++newCount; // Account for the last entry if it doesn't end with a delimiter
    }
    m_count = newCount;
    return true;
  }

  /**
   * @brief   Insert a string at position @p index, shifting later entries right.
   * @details Strings at positions [@p index, count) are shifted right by
   *          `sv.size() + 1` bytes to make room for the new entry.
   *          Inserting at `index == count()` is equivalent to @ref append().
   *          Accepts `const char*`, @ref String<N>, or any @ref String.
   *
   * @param[in] index  Zero-based insertion position (0 ≤ index ≤ count()).
   * @param[in] sv     String to copy; `sv.data()` must not be `nullptr`.
   * @return    `true` on success.
   * @return    `false` if @p index > count(), `sv.data()` is `nullptr`, or
   *            the buffer has no space.
   */
  bool insert(std::size_t index, String& sv) noexcept
  {
    if (sv.data() == nullptr)
    {
      return false;
    }
    if (index > m_count)
    {
      return false;
    }
    if (index == m_count)
    {
      return append(sv);
    }

    const std::size_t len = sv.size();
    if ((m_used + len + 1u) > m_capacity)
    {
      return false;
    }

    char *const p = ptrAt(index);
    const std::size_t tail = m_used - static_cast<std::size_t>(p - m_buf);

    std::memmove(p + len + 1u, p, tail);
    std::memcpy(p, sv.data(), len);
    p[len] = '\0';
    m_used += len + 1u;
    ++m_count;
    return true;
  }

  /**
   * @brief   Remove the string at position @p index, shifting later entries left.
   * @details Strings at positions (@p index, count) are shifted left by
   *          `strlen(removed) + 1` bytes. The freed tail bytes are zeroed.
   *
   * @param[in] index  Zero-based index of the string to remove.
   * @return    `true` on success.
   * @return    `false` if @p index >= count().
   */
  bool remove(std::size_t index) noexcept
  {
    if (index >= m_count)
    {
      return false;
    }

    char *const p = ptrAt(index);
    const std::size_t len = std::strlen(p) + 1u;
    const std::size_t tail = m_used - static_cast<std::size_t>(p - m_buf) - len;

    std::memmove(p, p + len, tail);
    m_used -= len;
    --m_count;
    std::memset(m_buf + m_used, 0, len); // zero freed bytes
    return true;
  }

  /**
   * @brief   Replace the string at @p index with @p sv.
   * @details Validates that the replacement fits before modifying state.
   *          Internally performs a remove-then-insert, so later entries are
   *          shifted by the difference in length.
   *          Accepts `const char*`, @ref String<N>, or any @ref String.
   *
   * @param[in] index  Zero-based index of the string to replace.
   * @param[in] sv     Replacement string; `sv.data()` must not be `nullptr`.
   * @return    `true` on success.
   * @return    `false` if @p index >= count(), `sv.data()` is `nullptr`, or
   *            the replacement does not fit in the buffer.
   */
  bool replace(std::size_t index, String sv) noexcept
  {
    if ((sv.data() == nullptr) || (index >= m_count))
    {
      return false;
    }

    const std::size_t oldLen = std::strlen(ptrAt(index)) + 1u;
    const std::size_t newLen = sv.size() + 1u;

    // After removing oldLen bytes, free space = m_capacity - (m_used - oldLen).
    if (newLen > (m_capacity - m_used + oldLen))
    {
      return false;
    }

    (void) remove(index);
    return insert(index, sv);
  }

  /**
   * @brief   Remove all strings and reset the buffer to zero.
   */
  void clear() noexcept
  {
    std::memset(m_buf, 0, m_capacity);
    m_count = 0u;
    m_used = 0u;
  }

  // -------------------------------------------------------------------------
  // Element access — raw pointer
  // -------------------------------------------------------------------------

  /**
   * @brief   Return a pointer to the string at position @p index.
   * @details The returned pointer is valid until the next mutating call.
   *
   * @param[in] index  Zero-based index.
   * @return    Pointer to the null-terminated string, or `nullptr` if
   *            @p index >= count().
   */
  [[nodiscard]]
  const char* at(std::size_t index) const noexcept
  {
    return ptrAt(index);
  }

  /**
   * @brief   Return a pointer to the string at @p index (unchecked).
   * @details Behaviour is undefined if @p index >= count().
   *          Prefer @ref at() for safe access.
   */
  [[nodiscard]]
  const char* operator[](std::size_t index) const noexcept
  {
    return ptrAt(index);
  }

  /**
   * @brief   Return a pointer to the first string.
   * @return  Pointer to the first null-terminated string, or `nullptr` if
   *          the list is empty.
   */
  [[nodiscard]]
  const char* first() const noexcept
  {
    return (m_count > 0u) ? m_buf : nullptr;
  }

  /**
   * @brief   Return a pointer to the last string.
   * @return  Pointer to the last null-terminated string, or `nullptr` if
   *          the list is empty.
   */
  [[nodiscard]]
  const char* last() const noexcept
  {
    return (m_count > 0u) ? ptrAt(m_count - 1u) : nullptr;
  }

  // -------------------------------------------------------------------------
  // Element access — String
  // -------------------------------------------------------------------------

  /**
   * @brief   Return a @ref String over the string at position @p index.
   * @details More efficient than `at()` when the caller needs the length,
   *          since `strlen` is avoided on subsequent use.
   *          The view is valid until the next mutating call.
   *
   * @param[in] index  Zero-based index.
   * @return    @ref String over the entry, or an empty view (data==nullptr)
   *            if @p index >= count().
   */
  [[nodiscard]]
  String atView(std::size_t index) const noexcept
  {
    const char *const p = ptrAt(index);
    return (p != nullptr) ? String(p, std::strlen(p)) : String();
  }

  /**
   * @brief   Return a @ref String over the first string.
   * @return  View over the first entry, or an empty view if the list is empty.
   */
  [[nodiscard]]
  String firstView() const noexcept
  {
    return (m_count > 0u) ? String(m_buf, std::strlen(m_buf)) : String();
  }

  /**
   * @brief   Return a @ref String over the last string.
   * @return  View over the last entry, or an empty view if the list is empty.
   */
  [[nodiscard]]
  String lastView() const noexcept
  {
    if (m_count == 0u)
    {
      return String();
    }
    const char *const p = ptrAt(m_count - 1u);
    return String(p, std::strlen(p));
  }

  // -------------------------------------------------------------------------
  // State queries
  // -------------------------------------------------------------------------

  /** @brief Return the number of strings currently in the list. */
  [[nodiscard]]
  std::size_t count() const noexcept
  {
    return m_count;
  }

  /** @brief Return the number of bytes currently occupied (data + null terminators). */
  [[nodiscard]]
  std::size_t bytesUsed() const noexcept
  {
    return m_used;
  }

  /** @brief Return the number of bytes still available in the buffer. */
  [[nodiscard]]
  std::size_t bytesFree() const noexcept
  {
    return m_capacity - m_used;
  }

  /** @brief Return `true` when the list contains no strings. */
  [[nodiscard]]
  bool isEmpty() const noexcept
  {
    return m_count == 0u;
  }

  /**
   * @brief   Return `true` if a string of @p strLen characters can be appended.
   * @details Accounts for the required null terminator byte.
   *
   * @param[in] strLen  Number of characters (excluding null terminator).
   * @return    `true` when `strLen + 1 <= bytesFree()`.
   */
  [[nodiscard]]
  bool canFit(std::size_t strLen) const noexcept
  {
    return (strLen + 1u) <= (m_capacity - m_used);
  }

  /**
   * @brief   Return the index of the first occurrence of @p sv.
   * @details Comparison is case-sensitive and exact.
   *          Accepts `const char*`, @ref String<N>, or any @ref String.
   *
   * @param[in] sv  String to search for; `sv.data()` must not be `nullptr`.
   * @return    Zero-based index of the first match, or @ref npos if not found
   *            or `sv.data()` is `nullptr`.
   */
  [[nodiscard]]
  std::size_t indexOf(String& sv) const noexcept
  {
    if (sv.data() == nullptr)
    {
      return npos;
    }
    for ( std::size_t i = 0u; i < m_count; ++i )
    {
      const char *const entry = ptrAt(i);
      const std::size_t elen = std::strlen(entry);
      if ((elen == sv.size())
          && (std::memcmp(entry, sv.data(), sv.size()) == 0))
      {
        return i;
      }
    }
    return npos;
  }

  /**
   * @brief   Return `true` if @p sv is present in the list.
   * @details Equivalent to `indexOf(sv) != npos`.
   *          Accepts `const char*`, @ref String<N>, or any @ref String.
   *
   * @param[in] sv  String to search for; `sv.data()` must not be `nullptr`.
   */
  [[nodiscard]]
  bool contains(String sv) const noexcept
  {
    return indexOf(sv) != npos;
  }

  /**
   * @brief   Join all strings into @p dest separated by @p sep.
   * @details Writes `str0 sep str1 sep ... strN-1 \0` into @p dest.
   *          When @p sep is `'\0'` the strings are concatenated without
   *          any separator (but each occupies exactly its character count).
   *
   * @param[out] dest      Destination buffer; must not be `nullptr`.
   * @param[in]  destSize  Size of @p dest in bytes (including null terminator).
   * @param[in]  sep       Separator character inserted between strings.
   * @return     Number of characters written (excluding the final null
   *             terminator), or @ref npos if @p dest is `nullptr` or the
   *             buffer is too small.
   */
  [[nodiscard]]
  std::size_t join(
    char* dest, std::size_t destSize, char sep = ' ') const noexcept
  {
    if ((dest == nullptr) || (destSize == 0u))
    {
      return npos;
    }

    std::size_t written = 0u;

    for ( std::size_t i = 0u; i < m_count; ++i )
    {
      const char *const s = ptrAt(i);
      const std::size_t len = std::strlen(s);

      // Need: len chars + optional separator + final null terminator
      const bool lastEntry = (i == m_count - 1u);
      const std::size_t need = len + (lastEntry ? 1u : 2u);

      if ((written + need) > destSize)
      {
        return npos;
      }

      std::memcpy(dest + written, s, len);
      written += len;

      if (!lastEntry && (sep != '\0'))
      {
        dest[written] = sep;
        ++written;
      }
    }

    dest[written] = '\0';
    return written;
  }

private:

  // -------------------------------------------------------------------------
  // Internal helpers
  // -------------------------------------------------------------------------

  /**
   * @brief   Return a const pointer to the string at @p index.
   * @details Walks the buffer one null-terminated entry at a time.
   *          Returns `nullptr` when @p index >= m_count.
   */
  const char* ptrAt(std::size_t index) const noexcept
  {
    if (index >= m_count)
    {
      return nullptr;
    }
    const char *p = m_buf;
    for ( std::size_t i = 0u; i < index; ++i )
    {
      p += std::strlen(p) + 1u;
    }
    return p;
  }

  /**
   * @brief   Return a mutable pointer to the string at @p index.
   * @details Returns `nullptr` when @p index >= m_count.
   */
  char* ptrAt(std::size_t index) noexcept
  {
    if (index >= m_count)
    {
      return nullptr;
    }
    char *p = m_buf;
    for ( std::size_t i = 0u; i < index; ++i )
    {
      p += std::strlen(p) + 1u;
    }
    return p;
  }

  // -------------------------------------------------------------------------
  // Data members
  // -------------------------------------------------------------------------

  char * m_buf { }; /*!< Flat storage: "str0\0str1\0...strN\0"  */
  std::size_t m_count { 0u }; /*!< Number of strings stored.              */
  std::size_t m_used { 0u }; /*!< Bytes occupied (chars + null terminators). */
  std::size_t m_capacity { 0 }; /*!< Total buffer size in bytes.             */
};

} // namespace hel

#endif // HELIOS_SYSLIB_STRINGLIST_HPP_
