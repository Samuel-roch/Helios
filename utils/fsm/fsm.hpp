/**
 ******************************************************************************
 * @file    FSM.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-24
 * @ingroup HELIOS_UTILS_FSM
 * @brief   Generic finite-state machine (FSM) template.
 *
 * @details
 *   - States are @ref FSMState<T> objects that hold entry, state, and exit
 *     callbacks as pointer-to-member functions of type @p T.
 *   - Transitions are registered with @ref FSM::addTransition and stored in a
 *     fixed-capacity compile-time array — no heap allocation.
 *   - The initial state should be set explicitly with @ref FSM::setInitialState
 *     before the first call to @ref FSM::runMachine or @ref FSM::trigger.
 *     If @ref FSM::setInitialState is omitted, the first @p state_from passed
 *     to @ref FSM::addTransition is used as the implicit initial state (legacy
 *     compatibility).
 *   - @ref FSM::trigger searches the transition table for a matching
 *     (current_state, event) pair; if found it calls the exit callback of the
 *     leaving state, switches the current state, and calls the entry callback
 *     of the entering state.
 *   - @ref FSM::runMachine fires the entry callback once (on the first call),
 *     then the state callback on every subsequent call.
 *   - @ref FSM::reset restores the FSM to the initial state and clears the
 *     initialised flag so the entry callback fires again on the next
 *     @ref FSM::runMachine call.
 *
 * ### Example
 * @code
 *   hel::FSMState<MyClass> s_idle, s_run;
 *   s_idle.set(&MyClass::onIdle, &MyClass::onIdleEnter);
 *   s_run.set (&MyClass::onRun,  &MyClass::onRunEnter, &MyClass::onRunExit);
 *
 *   hel::FSM<MyClass, 4> FSM(&myObj);
 *   FSM.setInitialState(&s_idle);
 *   FSM.addTransition(&s_idle, &s_run,  Event::Start);
 *   FSM.addTransition(&s_run,  &s_idle, Event::Stop);
 *
 *   // In the main loop:
 *   FSM.runMachine();
 *
 *   // From anywhere:
 *   FSM.trigger(Event::Start);
 * @endcode
 */

#ifndef HELIOS_UTILS_FSM_FSM_HPP_
#define HELIOS_UTILS_FSM_FSM_HPP_

#include <cstdint>
#include <cstddef>

namespace hel
{

// =============================================================================
// State
// =============================================================================

/**
 * @struct FSMState
 * @brief  Single state in a finite-state machine.
 * @ingroup HELIOS_UTILS_FSM
 *
 * @tparam T  Class that owns the state-handler member functions.
 *
 * @details
 *   Holds three optional pointer-to-member callbacks:
 *   - @ref m_onEnter — called once when the FSM enters this state.
 *   - @ref m_onState — called on every @ref FSM::runMachine tick while active.
 *   - @ref m_onExit  — called once when the FSM leaves this state.
 *
 *   Use @ref set to assign all three in a single call.
 */
template<class T>
struct FSMState
{
  /** @brief Pointer-to-member-function type used for all three callbacks. */
  using Callback = void (T::*)();

  /**
   * @brief  Assign the entry, state, and exit callbacks in one call.
   * @param[in] on_enter  Called once on entering this state.
   * @param[in] on_state  Called each FSM tick while this state is active.
   * @param[in] on_exit   Called once on leaving  this state.
   */
  void set(Callback on_state,
    Callback on_enter = nullptr,
    Callback on_exit = nullptr) noexcept
  {
    m_onEnter = on_enter;
    m_onState = on_state;
    m_onExit = on_exit;
  }

  Callback m_onState = nullptr; /*!< Called each FSM tick while this state is active. */
  Callback m_onEnter = nullptr; /*!< Called once on entering this state.              */
  Callback m_onExit = nullptr; /*!< Called once on leaving  this state.              */
};

// =============================================================================
// FSM
// =============================================================================

/**
 * @class  FSM
 * @brief  Fixed-capacity finite-state machine template.
 * @ingroup HELIOS_UTILS_FSM
 *
 * @tparam T         Class that owns the state-handler member functions.
 * @tparam capacity  Maximum number of transitions (compile-time constant).
 */
template<class T, std::size_t capacity>
class FSM
{
public:
  // -------------------------------------------------------------------------
  // Construction
  // -------------------------------------------------------------------------

  // Construct an FSM bound to the given T instance.
  explicit FSM(T* instance) noexcept;

  // Deleted — prevents accidental null-instance construction.
  FSM(std::nullptr_t) = delete;

  // -------------------------------------------------------------------------
  // Configuration
  // -------------------------------------------------------------------------

  // Set the state the FSM starts in (and returns to on reset()).
  void setInitialState(FSMState<T>* initial) noexcept;

  // Register a (from → to) transition triggered by event.
  bool addTransition(
    FSMState<T>* state_from, FSMState<T>* state_to, uint8_t event) noexcept;

  // -------------------------------------------------------------------------
  // Runtime
  // -------------------------------------------------------------------------

  // Execute one FSM tick: fires onEnter the first time, then onState each tick.
  void runMachine() noexcept;

  // Search the transition table and switch state if a match is found.
  void trigger(uint8_t event) noexcept;

  // Restore the FSM to the initial state.
  void reset() noexcept;

  // -------------------------------------------------------------------------
  // Accessors
  // -------------------------------------------------------------------------

  // Return a read-only pointer to the current active state.
  [[nodiscard]] const FSMState<T>* getCurrentState() const noexcept;

  // Return a read-only pointer to the state before the last transition.
  [[nodiscard]] const FSMState<T>* getPreviousState() const noexcept;

private:
  // -------------------------------------------------------------------------
  // Transition record
  // -------------------------------------------------------------------------

  /**
   * @brief Stores one (from, to, event) entry in the transition table.
   */
  struct Transition
  {
    FSMState<T>*state_from = nullptr; /*!< Source state pointer.      */
    FSMState<T>*state_to = nullptr; /*!< Destination state pointer. */
    uint8_t event = 0U; /*!< Triggering event value.    */
  };

  // -------------------------------------------------------------------------
  // Data members
  // -------------------------------------------------------------------------

  T *m_instance = nullptr; /*!< Owner object for PMF dispatch.           */
  FSMState<T>*m_initial_state = nullptr; /*!< State restored by reset().               */
  FSMState<T>*m_current_state = nullptr; /*!< Active state pointer.                    */
  FSMState<T>*m_previous_state = nullptr; /*!< State before the last transition.        */
  Transition m_transitions[capacity]; /*!< Fixed-size transition table.             */
  std::size_t m_num_transitions = 0U; /*!< Number of registered transitions.        */
  bool m_initialized = false; /*!< True after the first runMachine() call.  */
};

// =============================================================================
// FSM — method definitions
// =============================================================================

/**
 * @brief Construct an FSM bound to the given T instance.
 * @param[in] instance  Non-null pointer to the owner object. Passing `nullptr`
 *                      is ill-formed (the `std::nullptr_t` overload is deleted).
 */
template<class T, std::size_t capacity>
FSM<T, capacity>::FSM(T* instance) noexcept
    :
    m_instance(instance)
{
}

/**
 * @brief Set the initial state of the FSM.
 * @details The initial state is the state the FSM starts in and the one
 *          @ref reset restores.  If the current state has not been set yet
 *          (before any @ref addTransition call) this method also activates the
 *          provided state immediately.
 *
 * @param[in] initial  Pointer to the @ref State to use as the initial state.
 *                     Ignored if `nullptr`.
 */
template<class T, std::size_t capacity>
void FSM<T, capacity>::setInitialState(FSMState<T>* initial) noexcept
{
  if (initial == nullptr)
  {
    return;
  }

  m_initial_state = initial;

  if (m_current_state == nullptr)
  {
    m_current_state = initial;
  }
}

/**
 * @brief Register a transition in the FSM.
 * @details Adds the (@p state_from → @p state_to, @p event) tuple to the
 *          transition table.  If @ref setInitialState was never called, the
 *          first @p state_from is used as the implicit initial state for legacy
 *          compatibility.
 *
 * @param[in] state_from  Source state.  Must not be `nullptr`.
 * @param[in] state_to    Destination state.  Must not be `nullptr`.
 * @param[in] event       Event value that triggers this transition.
 * @return `true` if the transition was added; `false` if either state pointer
 *         is `nullptr` or the transition table is full.
 */
template<class T, std::size_t capacity>
bool FSM<T, capacity>::addTransition(
  FSMState<T>* state_from, FSMState<T>* state_to, uint8_t event) noexcept
{
  if (state_from == nullptr || state_to == nullptr)
  {
    return false;
  }
  if (m_num_transitions >= capacity)
  {
    return false;
  }

  // Implicit initial state: use the first state_from if setInitialState was not called
  if (m_initial_state == nullptr)
  {
    m_initial_state = state_from;
    m_current_state = state_from;
  }

  m_transitions[m_num_transitions].state_from = state_from;
  m_transitions[m_num_transitions].state_to = state_to;
  m_transitions[m_num_transitions].event = event;
  ++m_num_transitions;

  return true;
}

/**
 * @brief Execute one FSM tick.
 * @details On the very first call after construction or @ref reset, invokes
 *          @ref State::m_onEnter of the current state (if set).  On every call
 *          (including the first) invokes @ref State::m_onState of the current
 *          state (if set).
 */
template<class T, std::size_t capacity>
void FSM<T, capacity>::runMachine() noexcept
{
  if (!m_initialized)
  {
    m_initialized = true;

    if (m_current_state != nullptr && m_current_state->m_onEnter != nullptr)
    {
      (m_instance->*m_current_state->m_onEnter)();
    }
  }

  if (m_current_state != nullptr && m_current_state->m_onState != nullptr)
  {
    (m_instance->*m_current_state->m_onState)();
  }
}

/**
 * @brief Fire an event and execute the matching transition (if any).
 * @details Iterates the transition table looking for an entry whose
 *          @p state_from matches the current state and whose @p event matches
 *          @p event.  When found:
 *          1. @ref State::m_onExit of the leaving state is called (if set).
 *          2. The current state pointer is updated and the previous state is saved.
 *          3. @ref State::m_onEnter of the entering state is called (if set).
 *
 *          If no matching transition is found the FSM state is unchanged.
 *
 * @param[in] event  Event value to match against the transition table.
 */
template<class T, std::size_t capacity>
void FSM<T, capacity>::trigger(uint8_t event) noexcept
{
  for ( std::size_t i = 0U; i < m_num_transitions; ++i )
  {
    if (m_transitions[i].state_from == m_current_state
        && m_transitions[i].event == event)
    {
      // Exit callback of the leaving state
      if (m_current_state->m_onExit != nullptr)
      {
        (m_instance->*m_current_state->m_onExit)();
      }

      // Transition
      m_previous_state = m_current_state;
      m_current_state = m_transitions[i].state_to;

      // Entry callback of the entering state
      if (m_current_state->m_onEnter != nullptr)
      {
        (m_instance->*m_current_state->m_onEnter)();
      }

      return;
    }
  }
}

/**
 * @brief Reset the FSM to the initial state.
 * @details Restores @ref m_current_state to @ref m_initial_state (set by
 *          @ref setInitialState or implicitly by the first @ref addTransition),
 *          clears the previous-state record, and clears the initialised flag so
 *          the entry callback fires again on the next @ref runMachine call.
 *          If no initial state has been configured this method does nothing.
 */
template<class T, std::size_t capacity>
void FSM<T, capacity>::reset() noexcept
{
  if (m_initial_state == nullptr)
  {
    return;
  }

  m_current_state = m_initial_state;
  m_previous_state = nullptr;
  m_initialized = false;
}

/**
 * @brief Return a read-only pointer to the current active state.
 * @return Pointer to the active @ref State, or `nullptr` if the FSM has not
 *         been configured yet.
 */
template<class T, std::size_t capacity>
const FSMState<T>* FSM<T, capacity>::getCurrentState() const noexcept
{
  return m_current_state;
}

/**
 * @brief Return a read-only pointer to the state before the last transition.
 * @return Pointer to the previous @ref State, or `nullptr` if no transition
 *         has occurred since construction or @ref reset.
 */
template<class T, std::size_t capacity>
const FSMState<T>* FSM<T, capacity>::getPreviousState() const noexcept
{
  return m_previous_state;
}

} // namespace hel

#endif // HELIOS_UTILS_FSM_FSM_HPP_
