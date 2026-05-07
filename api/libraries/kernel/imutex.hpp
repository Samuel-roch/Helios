/**
 ******************************************************************************
 * @file    imutex.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.2.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL_MUTEX
 * @brief   Mutual exclusion lock interface.
 */

#ifndef HELIOS_KERNEL_IMUTEX_HPP_
#define HELIOS_KERNEL_IMUTEX_HPP_

#include <hel_return_code>
#include "rtos_types.hpp"

namespace hel
{

/**
 * @class  iMutex
 * @brief  RTOS-agnostic mutual exclusion lock interface.
 * @ingroup HELIOS_KERNEL_MUTEX
 *
 * @details Provides a binary mutex for protecting shared resources between
 *          tasks. Supports priority inheritance when the underlying RTOS
 *          supports it.
 *
 *          Mutex operations must not be called from an ISR context.
 *
 * @note  Copy and move are deleted; mutexes own RTOS resources and must not be
 *        duplicated or relocated.
 */
class iMutex
{
public:

    virtual ~iMutex() noexcept = default;

    // -------------------------------------------------------------------------
    // Lock / unlock
    // -------------------------------------------------------------------------

    /**
     * @brief  Acquire the mutex, blocking until it becomes available or timeout elapses.
     * @param[in]  timeout_ms  Maximum wait time in ticks.
     *                         Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the mutex was acquired.
     * @return @ref ReturnCode::ErrorTimeout if @p timeout_ms elapsed before acquisition.
     * @return @ref ReturnCode::ErrorGeneral on failure (e.g. deadlock detection).
     */
    [[nodiscard]]
    virtual ReturnCode lock(TickType timeout_ms) noexcept = 0;

    /**
     * @brief  Attempt to acquire the mutex without blocking.
     * @details Equivalent to calling @ref lock() with a timeout of zero.
     * @return @ref ReturnCode::AnsweredRequest if the mutex was acquired immediately.
     * @return @ref ReturnCode::ErrorTimeout if the mutex is currently held.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode tryLock() noexcept = 0;

    /**
     * @brief  Release the mutex.
     * @details Must be called by the same task that acquired the mutex.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::ErrorGeneral if the mutex is not owned by the caller.
     */
    [[nodiscard]]
    virtual ReturnCode unlock() noexcept = 0;

protected:

    iMutex() noexcept = default;

    iMutex(const iMutex&)             = delete;
    iMutex& operator=(const iMutex&)  = delete;
    iMutex(iMutex&&)                  = delete;
    iMutex& operator=(iMutex&&)       = delete;
};

} // namespace hel

#endif // HELIOS_KERNEL_IMUTEX_HPP_
