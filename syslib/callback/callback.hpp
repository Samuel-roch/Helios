/**
 ******************************************************************************
 * @file    callback.hpp
 * @author  Samuel Almeida Rocha 
 * @version 2.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_SYSLIB_CALLBACK
 * @brief   Allocation-free, type-erased callable wrapper.
 *

 */

#ifndef HELIOS_SYSLIB_CALLBACK_HPP_
#define HELIOS_SYSLIB_CALLBACK_HPP_

#include <stddef.h>
#include <new>
#include <type_traits>
#include <utility>

namespace hel
{

// =============================================================================
// detail — internal helpers
// =============================================================================

namespace detail
{

/**
 * @brief   Applies `std::launder` when available; otherwise returns the
 *          pointer unchanged.
 * @details Required to legally access an object created via placement new
 *          through the original storage pointer (C++17 and later).
 *
 * @tparam  T Pointed-to type.
 * @param[in] p Pointer to launder.
 * @return  Laundered pointer (or @p p unchanged on pre-C++17 compilers).
 */
template<class T>
constexpr T* launder_shim(T* p) noexcept
{
#if defined(__cpp_lib_launder) && (__cpp_lib_launder >= 201606L)
    return std::launder(p);
#else
  return p;
#endif
}

} // namespace detail

constexpr std::size_t StorageBytes = sizeof(void*) * 2U;

// =============================================================================
// Callback<Ret(Args...)> — primary template (incomplete) + partial specialisation
// =============================================================================

/** @brief Primary template — intentionally incomplete; use `Callback<Ret(Args...)>`. */
template<class Sig>
class Callback;

/**
 * @brief   Allocation-free, type-erased callable wrapper.
 * @details
 *   Binds any of the following without heap allocation:
 *   - Non-const member function: `void (U::*)(Args...)`.
 *   - Const member function:     `void (U::*)(Args...) const`.
 *   - Free function or captureless lambda: `void(*)(Args...)` (incl. noexcept).
 *   - Functor or capturing lambda **by pointer** (caller owns lifetime).
 *   - Trivially copyable functor **by value** via @ref set_by_value (SBO).
 *
 *   Calling an unset @ref Callback via `operator()` is a no-op. Use
 *   @ref try_invoke to distinguish between "called" and "not set".
 *
 *   The mutable-object and const-object pointers are stored in separate
 *   members to avoid `const_cast` (`const_cast` is prohibited by MISRA C++).
 *
 *   Thread-safety: a @ref Callback instance must not be modified concurrently
 *   with any call to `operator()` or `try_invoke`.
 *
 * @tparam Ret  Return type of the callable (typically `void`).
 * @tparam Args Argument types of the callable signature.
 *
 * @ingroup HELIOS_SYSLIB_CALLBACK
 */
template<class Ret, class ... Args>
class Callback<Ret(Args...)>
{
public:
  /**
   * @brief   Internal invoker function type.
   * @details Each invoker receives a mutable object pointer, a const object
   *          pointer, a mutable storage pointer, and the forwarded arguments.
   *          Only the parameters relevant to the bound callable kind are used;
   *          the rest are ignored.
   */
  static_assert(std::is_void<Ret>::value,
      "Callback<Ret(Args...)>: only void return type is supported");

  using Inv = void (*)(void*, const void*, void*, Args...) noexcept;

  /** @brief Size of the inline storage buffer in bytes. */
  static constexpr std::size_t kStorageBytes = StorageBytes;

  // -------------------------------------------------------------------------
  // Constructors / assignment
  // -------------------------------------------------------------------------

  /** @brief Default constructor — produces an unset (empty) callback. */
  constexpr Callback() noexcept = default;

  /** @brief Copy constructor — copies all members including inline storage. */
  Callback(const Callback&) noexcept = default;

  /** @brief Copy assignment — copies all members including inline storage. */
  Callback& operator=(const Callback&) noexcept = default;

  // -------------------------------------------------------------------------
  // Bind: non-const member function
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a non-const member function to this callback.
   * @details Both @p obj and @p fn must be non-null; if either is `nullptr`
   *          the callback is cleared and `false` is returned.
   *          The pointer-to-member-function must fit in @ref kStorageBytes.
   *
   * @tparam  U   Class type that owns the member function.
   * @param[in] obj Pointer to the target object; must outlive this callback.
   * @param[in] fn  Non-const member function pointer.
   * @return  `true` if binding succeeded, `false` if any argument is null.
   */
  template<class U>
  constexpr Callback(U* obj, void (U::*fn)(Args...)) noexcept
  {
    using PMF = void (U::*)(Args...);
    static_assert(std::is_trivially_destructible<PMF>::value,
        "PMF must be trivially destructible");
    static_assert(std::is_trivially_copyable<PMF>::value,
        "PMF must be trivially copyable");
    static_assert(sizeof(PMF) <= kStorageBytes,
        "PMF exceeds inline storage; increase StorageBytes");

    if ((obj == nullptr) || (fn == nullptr))
    {
      clear();
      return;
    }

    m_mut_obj = static_cast<void*>(obj);
    m_const_obj = nullptr;
    m_inv = &invoke_pmf<U, PMF>;
    (void) new (&m_storage) PMF(fn);
    m_engaged = true;
    return;
  }

  // -------------------------------------------------------------------------
  // Bind: non-const member function (noexcept overload)
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a non-const noexcept member function to this callback.
   * @details In C++17 and later `noexcept` is part of the function type, so a
   *          `void (U::*)(Args...) noexcept` pointer does **not** implicitly
   *          convert to `void (U::*)(Args...)`. This overload handles that case
   *          so that ISR-safe (`noexcept`) member functions can be bound without
   *          requiring a const_cast or wrapper.
   *
   * @tparam  U   Class type that owns the member function.
   * @param[in] obj Pointer to the target object; must outlive this callback.
   * @param[in] fn  Non-const noexcept member function pointer.
   * @return  `true` if binding succeeded, `false` if any argument is null.
   */
  template<class U>
  constexpr Callback(U* obj, void (U::*fn)(Args...) noexcept) noexcept
  {
    using PMF = void (U::*)(Args...) noexcept;
    static_assert(std::is_trivially_destructible<PMF>::value,
        "PMF must be trivially destructible");
    static_assert(std::is_trivially_copyable<PMF>::value,
        "PMF must be trivially copyable");
    static_assert(sizeof(PMF) <= kStorageBytes,
        "PMF exceeds inline storage; increase StorageBytes");

    if ((obj == nullptr) || (fn == nullptr))
    {
      clear();
      return ;
    }

    m_mut_obj = static_cast<void*>(obj);
    m_const_obj = nullptr;
    m_inv = &invoke_pmf<U, PMF>;
    (void) new (&m_storage) PMF(fn);
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: const member function
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a const member function to this callback.
   * @details The const object pointer is stored separately from the mutable
   *          pointer to avoid `const_cast`. Both @p obj and @p fn must be
   *          non-null; otherwise the callback is cleared.
   *
   * @tparam  U   Class type that owns the member function.
   * @param[in] obj Const pointer to the target object; must outlive this callback.
   * @param[in] fn  Const member function pointer.
   * @return  `true` if binding succeeded, `false` if any argument is null.
   */
  template<class U>
  constexpr Callback(const U* obj, void (U::*fn)(Args...) const) noexcept
  {
    using PMF = void (U::*)(Args...) const;
    static_assert(std::is_trivially_destructible<PMF>::value,
        "PMF must be trivially destructible");
    static_assert(std::is_trivially_copyable<PMF>::value,
        "PMF must be trivially copyable");
    static_assert(sizeof(PMF) <= kStorageBytes,
        "PMF exceeds inline storage; increase StorageBytes");

    if ((obj == nullptr) || (fn == nullptr))
    {
      clear();
      return ;
    }

    m_mut_obj = nullptr;
    m_const_obj = static_cast<const void*>(obj);
    m_inv = &invoke_pmf_const<U, PMF>;
    (void) new (&m_storage) PMF(fn);
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: const noexcept member function
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a const noexcept member function to this callback.
   * @details In C++17 and later `noexcept` is part of the function type, so a
   *          `void (U::*)(Args...) const noexcept` pointer does not implicitly
   *          convert to `void (U::*)(Args...) const`. This overload handles
   *          that case.
   *
   * @tparam  U   Class type that owns the member function.
   * @param[in] obj Const pointer to the target object; must outlive this callback.
   * @param[in] fn  Const noexcept member function pointer.
   * @return  `true` if binding succeeded, `false` if any argument is null.
   */
  template<class U>
  constexpr Callback(const U* obj, void (U::*fn)(Args...) const noexcept) noexcept
  {
    using PMF = void (U::*)(Args...) const noexcept;
    static_assert(std::is_trivially_destructible<PMF>::value,
        "PMF must be trivially destructible");
    static_assert(std::is_trivially_copyable<PMF>::value,
        "PMF must be trivially copyable");
    static_assert(sizeof(PMF) <= kStorageBytes,
        "PMF exceeds inline storage; increase StorageBytes");

    if ((obj == nullptr) || (fn == nullptr))
    {
      clear();
      return;
    }

    m_mut_obj = nullptr;
    m_const_obj = static_cast<const void*>(obj);
    m_inv = &invoke_pmf_const<U, PMF>;
    (void) new (&m_storage) PMF(fn);
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: free function (possibly noexcept)
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a free function or captureless lambda (non-noexcept).
   * @details The function pointer is stored in the inline storage buffer.
   *          Must fit in @ref kStorageBytes.
   *
   * @param[in] fn Non-null function pointer.
   * @return  `true` if binding succeeded, `false` if @p fn is null.
   */
  constexpr Callback(void (*fn)(Args...)) noexcept
  {
    using F = void (*)(Args...);
    static_assert(std::is_trivially_copyable<F>::value,
        "Function pointer must be trivially copyable");
    static_assert(sizeof(F) <= kStorageBytes,
        "Function pointer exceeds inline storage; increase StorageBytes");

    if (fn == nullptr)
    {
      clear();
      return;
    }

    m_mut_obj = nullptr;
    m_const_obj = nullptr;
    m_inv = &invoke_free<F>;
    (void) new (&m_storage) F(fn);
    m_engaged = true;
  }

  /**
   * @brief   Binds a free function or captureless lambda (noexcept overload).
   * @details Identical to the non-noexcept overload; provided so that
   *          `noexcept` function pointers resolve without implicit conversion.
   *
   * @param[in] fn Non-null noexcept function pointer.
   * @return  `true` if binding succeeded, `false` if @p fn is null.
   */
  constexpr Callback(void (*fn)(Args...) noexcept) noexcept
  {
    using F = void (*)(Args...) noexcept;
    static_assert(std::is_trivially_copyable<F>::value,
        "Function pointer must be trivially copyable");
    static_assert(sizeof(F) <= kStorageBytes,
        "Function pointer exceeds inline storage; increase StorageBytes");

    if (fn == nullptr)
    {
      clear();
      return;
    }

    m_mut_obj = nullptr;
    m_const_obj = nullptr;
    m_inv = &invoke_free<F>;
    (void) new (&m_storage) F(fn);
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: functor / capturing lambda by pointer (mutable)
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a mutable functor or capturing lambda by pointer.
   * @details The functor object must outlive this callback. Its `operator()`
   *          is called directly via the stored pointer (no copy is made).
   *          This overload does not participate in overload resolution for
   *          function pointer types.
   *
   * @tparam  F   Functor type; must be callable as `F&(Args...) -> void`.
   * @param[in] fnobj Non-null pointer to the functor.
   * @return  `true` if binding succeeded, `false` if @p fnobj is null.
   */
  template<class F, typename = std::enable_if_t<
      !std::is_function<std::remove_pointer_t<F>>::value
          && std::is_invocable_r<void, F&, Args...>::value>>
  constexpr Callback(F* fnobj) noexcept
  {
    if (fnobj == nullptr)
    {
      clear();
      return;
    }

    m_mut_obj = static_cast<void*>(fnobj);
    m_const_obj = nullptr;
    m_inv = &invoke_functor<F>;
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: functor / capturing lambda by pointer (const)
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a const functor or capturing lambda by pointer.
   * @details The const pointer is stored in the dedicated const-object member
   *          to avoid `const_cast`. The functor object must outlive this
   *          callback.
   *
   * @tparam  F   Functor type; must be callable as `const F&(Args...) -> void`.
   * @param[in] fnobj Non-null const pointer to the functor.
   * @return  `true` if binding succeeded, `false` if @p fnobj is null.
   */
  template<class F, typename = std::enable_if_t<
      !std::is_function<std::remove_pointer_t<F>>::value
          && std::is_invocable_r<void, const F&, Args...>::value>>
  constexpr Callback(const F* fnobj) noexcept
  {
    if (fnobj == nullptr)
    {
      clear();
      return;
    }

    m_mut_obj = nullptr;
    m_const_obj = static_cast<const void*>(fnobj);
    m_inv = &invoke_functor_const<F>;
    m_engaged = true;
  }

  // -------------------------------------------------------------------------
  // Bind: SBO — trivially copyable functor by value
  // -------------------------------------------------------------------------

  /**
   * @brief   Binds a trivially copyable functor by value (small buffer optimisation).
   * @details A copy of @p fnobj is placed in the inline storage buffer.
   *          The original object does not need to outlive the callback.
   *          Only participates in overload resolution when:
   *          - @p F is trivially copyable,
   *          - `sizeof(F) <= kStorageBytes`,
   *          - @p F is callable as `F&(Args...) -> void` (supports mutable lambdas),
   *          - @p F is not a pointer type (pointer types have their own overload).
   *
   * @tparam  F   Trivially copyable functor type.
   * @param[in] fnobj Functor to copy into the inline buffer.
   * @return  Always `true`.
   */
  template<class F, typename = std::enable_if_t<
      std::is_trivially_copyable<F>::value && (sizeof(F) <= kStorageBytes)
          && std::is_invocable_r<void, F&, Args...>::value
          && !std::is_pointer<F>::value>>
  bool set_by_value(F fnobj) noexcept
  {
    static_assert(alignof(F) <= alignof(std::max_align_t),
        "Functor alignment exceeds storage alignment");
    (void) new (&m_storage) F(fnobj);
    m_mut_obj = nullptr;
    m_const_obj = nullptr;
    m_inv = &invoke_functor_inplace<F>;
    m_engaged = true;
    return true;
  }

  // -------------------------------------------------------------------------
  // State
  // -------------------------------------------------------------------------

  /**
   * @brief   Clears the binding, returning the callback to the unset state.
   * @details All stored types are trivially destructible; no destructor is
   *          called explicitly.
   */
  void clear() noexcept
  {
    m_engaged = false;
    m_mut_obj = nullptr;
    m_const_obj = nullptr;
    m_inv = nullptr;
  }

  /**
   * @brief   Returns `true` when a callable has been successfully bound.
   * @return  `true` if set, `false` if empty/cleared.
   */
  [[nodiscard]] bool is_set() const noexcept
  {
    return m_engaged;
  }

  /**
   * @brief   Boolean conversion — equivalent to @ref is_set.
   * @return  `true` if set.
   */
  explicit operator bool() const noexcept
  {
    return is_set();
  }

  // -------------------------------------------------------------------------
  // Invocation
  // -------------------------------------------------------------------------

  /**
   * @brief   Invokes the bound callable with @p args.
   * @details No-op when the callback is not set. Equivalent to calling
   *          @ref try_invoke and ignoring the return value.
   *
   * @param[in] args Arguments forwarded to the callable.
   */
  bool operator()(Args ... args) const noexcept
  {
    if (!m_engaged)
    {
      return false;
    }

    m_inv(m_mut_obj, m_const_obj, &m_storage, std::forward<Args>(args)...);

    return true;
  }


private:
  // -------------------------------------------------------------------------
  // Invoker implementations
  // -------------------------------------------------------------------------

  /**
   * @brief Invokes a non-const pointer-to-member-function stored in @p storage.
   */
  template<class U, class PMF>
  static void invoke_pmf(
    void* mut_obj, const void*, void* storage, Args ... args) noexcept
  {
    auto *p = static_cast<U*>(mut_obj);
    const auto *mptr = detail::launder_shim(static_cast<const PMF*>(storage));
    (p->**mptr)(std::forward<Args>(args)...);
  }

  /**
   * @brief Invokes a const pointer-to-member-function stored in @p storage.
   */
  template<class U, class PMF>
  static void invoke_pmf_const(
    void*, const void* const_obj, void* storage, Args ... args) noexcept
  {
    const auto *p = static_cast<const U*>(const_obj);
    const auto *mptr = detail::launder_shim(static_cast<const PMF*>(storage));
    (p->**mptr)(std::forward<Args>(args)...);
  }

  /**
   * @brief Invokes a free function or captureless lambda stored in @p storage.
   */
  template<class F>
  static void invoke_free(
    void*, const void*, void* storage, Args ... args) noexcept
  {
    const auto *f = detail::launder_shim(static_cast<const F*>(storage));
    (*f)(std::forward<Args>(args)...);
  }

  /**
   * @brief Invokes a mutable functor via the mutable object pointer.
   */
  template<class F>
  static void invoke_functor(
    void* mut_obj, const void*, void*, Args ... args) noexcept
  {
    auto *f = static_cast<F*>(mut_obj);
    (*f)(std::forward<Args>(args)...);
  }

  /**
   * @brief Invokes a const functor via the const object pointer.
   */
  template<class F>
  static void invoke_functor_const(
    void*, const void* const_obj, void*, Args ... args) noexcept
  {
    const auto *f = static_cast<const F*>(const_obj);
    (*f)(std::forward<Args>(args)...);
  }

  /**
   * @brief Invokes a trivially copyable functor stored by value in @p storage.
   * @details Receives a mutable `void*` to correctly support functors with a
   *          non-const `operator()` (e.g. mutable lambdas).
   */
  template<class F>
  static void invoke_functor_inplace(
    void*, const void*, void* storage, Args ... args) noexcept
  {
    auto *f = detail::launder_shim(static_cast<F*>(storage));
    (*f)(std::forward<Args>(args)...);
  }

  // -------------------------------------------------------------------------
  // Data members
  // -------------------------------------------------------------------------

  void *m_mut_obj = nullptr; /*!< Pointer to mutable target object or functor. */
  const void *m_const_obj = nullptr; /*!< Pointer to const target object or functor. */
  Inv m_inv = nullptr; /*!< Active invoker function. */
  alignas(std::max_align_t) mutable unsigned char m_storage[kStorageBytes] { }; /*!< Inline storage for PMF or free function pointer. */
  bool m_engaged = false; /*!< True when a callable is bound. */
};

} // namespace hel

#endif // HELIOS_SYSLIB_CALLBACK_HPP_
