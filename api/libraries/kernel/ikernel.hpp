/**
 ******************************************************************************
 * @file    ikernel.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.2.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL
 * @brief   RTOS kernel abstraction interface.
 *
 * @note  Unlike the other kernel interfaces (@ref iTask, @ref iMutex,
 *        @ref iSemaphore, @ref iQueue), this header declares free functions
 *        rather than an abstract class: the scheduler is a process-wide
 *        singleton, not an instantiable per-driver resource. Each target
 *        provides exactly one definition of every function declared here.
 */

#ifndef HELIOS_KERNEL_IKERNEL_HPP_
#define HELIOS_KERNEL_IKERNEL_HPP_

#include <hel_return_code>
#include "rtos_types.hpp"

namespace hel
{

// -------------------------------------------------------------------------
// Scheduler lifecycle
// -------------------------------------------------------------------------

/**
 * @brief  Start the RTOS scheduler.
 * @details On most RTOS implementations this call does not return if the
 *          scheduler starts successfully. It returns only on failure.
 * @return @ref ReturnCode::ErrorInvalidState if the scheduler is already running.
 * @return @ref ReturnCode::ErrorGeneral if the scheduler failed to start.
 */
[[nodiscard]]
ReturnCode start() noexcept;

/**
 * @brief  Stop the RTOS scheduler.
 * @return @ref ReturnCode::AnsweredRequest on success.
 * @return @ref ReturnCode::NotInitialized if the scheduler is not running.
 * @return @ref ReturnCode::ErrorGeneral if the scheduler failed to stop.
 */
[[nodiscard]]
ReturnCode stop() noexcept;

// -------------------------------------------------------------------------
// Task control
// -------------------------------------------------------------------------

/**
 * @brief  Delay the current task for the given number of ticks.
 * @details Yields execution and resumes after at least @p ticks RTOS ticks.
 *          Must not be called from an ISR context.
 * @param[in]  ticks  Delay duration in RTOS ticks (see @ref TickType). Zero yields immediately.
 */
void sleep(TickType ticks) noexcept;

/**
 * @brief  Yield the current task, allowing the scheduler to run other tasks.
 * @details Must not be called from an ISR context.
 */
void yield() noexcept;

// -------------------------------------------------------------------------
// System tick
// -------------------------------------------------------------------------

/**
 * @brief  Return the system tick counter.
 * @details The counter starts at zero when the scheduler is started and
 *          wraps around after reaching the maximum value of @ref TickType.
 *          The unit is RTOS ticks, not milliseconds; convert using the
 *          target's configured tick rate if a wall-clock duration is needed.
 * @return Current tick count.
 */
[[nodiscard]]
TickType getTick() noexcept;

/**
 * @brief  Return the system tick counter from an ISR context.
 * @details Equivalent to @ref getTick() but safe to call from an
 *          interrupt service routine.
 * @return Current tick count.
 */
[[nodiscard]]
TickType getTickFromIsr() noexcept;

// -------------------------------------------------------------------------
// Critical sections
// -------------------------------------------------------------------------

/**
 * @brief  Enter a critical section (disable task preemption / interrupts).
 * @details Critical sections must be kept as short as possible.
 *          Must be paired with a matching @ref exitCritical() call.
 *          Must not be called from an ISR context.
 */
void enterCritical() noexcept;

/**
 * @brief  Exit a critical section (re-enable task preemption / interrupts).
 * @details Must be paired with a preceding @ref enterCritical() call.
 */
void exitCritical() noexcept;

/**
 * @brief  Enter a critical section from an ISR context.
 * @details Saves and returns the previous interrupt mask.
 * @return Saved interrupt mask to be passed to @ref exitCriticalFromIsr().
 */
[[nodiscard]]
UBaseType enterCriticalFromIsr() noexcept;

/**
 * @brief  Exit a critical section from an ISR context.
 * @param[in]  savedMask  Interrupt mask returned by @ref enterCriticalFromIsr().
 */
void exitCriticalFromIsr(UBaseType savedMask) noexcept;

} // namespace hel

#endif // HELIOS_KERNEL_IKERNEL_HPP_
