/**
 ******************************************************************************
 * @file    isemaphore.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.2.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL_SEMAPHORE
 * @brief   Semaphore interface.
 */

#ifndef HELIOS_KERNEL_ISEMAPHORE_HPP_
#define HELIOS_KERNEL_ISEMAPHORE_HPP_

#include <hel_return_code>
#include "rtos_types.hpp"

namespace hel
{

/**
 * @class  iSemaphore
 * @brief  RTOS-agnostic counting and binary semaphore interface.
 * @ingroup HELIOS_KERNEL_SEMAPHORE
 *
 * @details Supports both binary and counting semaphore semantics depending on
 *          the maximum count configured at construction in the derived class.
 *
 *          @ref give() and @ref giveFromIsr() are used to signal availability.
 *          @ref take() blocks until the semaphore count is greater than zero.
 *
 * @note  Copy and move are deleted; semaphores own RTOS resources and must not
 *        be duplicated or relocated.
 */
class iSemaphore
{
public:

    virtual ~iSemaphore() noexcept = default;

    // -------------------------------------------------------------------------
    // Give (signal)
    // -------------------------------------------------------------------------

    /**
     * @brief  Increment the semaphore count from task context.
     * @details Unblocks the highest-priority task waiting on this semaphore,
     *          if any.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::ErrorQueueFull if the count is already at maximum.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode give() noexcept = 0;

    /**
     * @brief  Increment the semaphore count from an ISR context.
     * @details Safe to call from an interrupt service routine.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::ErrorQueueFull if the count is already at maximum.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode giveFromIsr() noexcept = 0;

    // -------------------------------------------------------------------------
    // Take (wait)
    // -------------------------------------------------------------------------

    /**
     * @brief  Decrement the semaphore count, blocking until available or timeout elapses.
     * @param[in]  timeout_ms  Maximum wait time in ticks.
     *                         Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the semaphore was taken.
     * @return @ref ReturnCode::ErrorTimeout if @p timeout_ms elapsed.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode take(TickType timeout_ms) noexcept = 0;

    // -------------------------------------------------------------------------
    // State query
    // -------------------------------------------------------------------------

    /**
     * @brief  Return the current semaphore count.
     * @return Number of available tokens (0 for a taken binary semaphore).
     */
    [[nodiscard]]
    virtual UBaseType count() const noexcept = 0;

protected:

    iSemaphore() noexcept = default;

    iSemaphore(const iSemaphore&)             = delete;
    iSemaphore& operator=(const iSemaphore&)  = delete;
    iSemaphore(iSemaphore&&)                  = delete;
    iSemaphore& operator=(iSemaphore&&)       = delete;
};

} // namespace hel

#endif // HELIOS_KERNEL_ISEMAPHORE_HPP_
