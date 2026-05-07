/**
 ******************************************************************************
 * @file    itask.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.2.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL_TASK
 * @brief   Task/thread interface.
 */

#ifndef HELIOS_KERNEL_ITASK_HPP_
#define HELIOS_KERNEL_ITASK_HPP_

#include <hel_return_code>
#include <hel_string>
#include "rtos_types.hpp"

namespace hel
{

/**
 * @enum  TaskPriority
 * @brief Logical priority levels for task scheduling.
 * @ingroup HELIOS_KERNEL_TASK
 */
enum class TaskPriority : uint8_t
{
    None          = 0U,        /*!< No priority (not initialized). */
    Idle          = 1U,        /*!< Reserved for idle thread. */
    Low           = 8U,        /*!< Low priority. */
    Low1          = 9U,        /*!< Low + 1. */
    Low2          = 10U,       /*!< Low + 2. */
    Low3          = 11U,       /*!< Low + 3. */
    Low4          = 12U,       /*!< Low + 4. */
    Low5          = 13U,       /*!< Low + 5. */
    Low6          = 14U,       /*!< Low + 6. */
    Low7          = 15U,       /*!< Low + 7. */
    BelowNormal   = 16U,       /*!< Below normal priority. */
    BelowNormal1  = 17U,       /*!< Below normal + 1. */
    BelowNormal2  = 18U,       /*!< Below normal + 2. */
    BelowNormal3  = 19U,       /*!< Below normal + 3. */
    BelowNormal4  = 20U,       /*!< Below normal + 4. */
    BelowNormal5  = 21U,       /*!< Below normal + 5. */
    BelowNormal6  = 22U,       /*!< Below normal + 6. */
    BelowNormal7  = 23U,       /*!< Below normal + 7. */
    Normal        = 24U,       /*!< Normal priority. */
    Normal1       = 25U,       /*!< Normal + 1. */
    Normal2       = 26U,       /*!< Normal + 2. */
    Normal3       = 27U,       /*!< Normal + 3. */
    Normal4       = 28U,       /*!< Normal + 4. */
    Normal5       = 29U,       /*!< Normal + 5. */
    Normal6       = 30U,       /*!< Normal + 6. */
    Normal7       = 31U,       /*!< Normal + 7. */
    AboveNormal   = 32U,       /*!< Above normal priority. */
    AboveNormal1  = 33U,       /*!< Above normal + 1. */
    AboveNormal2  = 34U,       /*!< Above normal + 2. */
    AboveNormal3  = 35U,       /*!< Above normal + 3. */
    AboveNormal4  = 36U,       /*!< Above normal + 4. */
    AboveNormal5  = 37U,       /*!< Above normal + 5. */
    AboveNormal6  = 38U,       /*!< Above normal + 6. */
    AboveNormal7  = 39U,       /*!< Above normal + 7. */
    High          = 40U,       /*!< High priority. */
    High1         = 41U,       /*!< High + 1. */
    High2         = 42U,       /*!< High + 2. */
    High3         = 43U,       /*!< High + 3. */
    High4         = 44U,       /*!< High + 4. */
    High5         = 45U,       /*!< High + 5. */
    High6         = 46U,       /*!< High + 6. */
    High7         = 47U,       /*!< High + 7. */
    Realtime      = 48U,       /*!< Realtime priority. */
    Realtime1     = 49U,       /*!< Realtime + 1. */
    Realtime2     = 50U,       /*!< Realtime + 2. */
    Realtime3     = 51U,       /*!< Realtime + 3. */
    Realtime4     = 52U,       /*!< Realtime + 4. */
    Realtime5     = 53U,       /*!< Realtime + 5. */
    Realtime6     = 54U,       /*!< Realtime + 6. */
    Realtime7     = 55U,       /*!< Realtime + 7. */
    ISR           = 56U,       /*!< Reserved for ISR deferred thread. */
};

/**
 * @class  iTask
 * @brief  RTOS-agnostic task/thread abstraction.
 * @ingroup HELIOS_KERNEL_TASK
 *
 * @details Derive from this class and implement @ref run() to define the task
 *          body. Call @ref start() to create and launch the RTOS task.
 *
 *          The @ref run() method executes in the context of the created RTOS
 *          task and must not return (it should contain an infinite loop).
 *
 * @note  Copy and move are deleted; tasks own RTOS resources and must not be
 *        duplicated or relocated.
 */
class iTask
{
public:

    virtual ~iTask() noexcept = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief  Create and start the RTOS task.
     * @param[in]  name        Task name used for debugging.
     * @param[in]  stackWords  Stack size in words (@ref StackDepthType).
     * @param[in]  priority    Scheduling priority level.
     * @return @ref ReturnCode::AnsweredRequest if the task was created and started.
     * @return @ref ReturnCode::ErrorInvalidState if the task is already running.
     * @return @ref ReturnCode::ErrorParam if @p stackWords is zero.
     * @return @ref ReturnCode::ErrorOutOfMemory if the RTOS could not allocate resources.
     * @return @ref ReturnCode::ErrorGeneral on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode start(const String&  name,
                             StackDepthType stackWords,
                             TaskPriority   priority) noexcept = 0;

    /**
     * @brief  Return the current lifecycle state of the task.
     * @return @ref ReturnCode::OperationRunning if the task is active.
     * @return @ref ReturnCode::NotInitialized if the task has not been started.
     * @return @ref ReturnCode::OperationIdle if the task is suspended.
     */
    [[nodiscard]]
    virtual ReturnCode status() const noexcept = 0;

    /**
     * @brief  Suspend the task, preventing it from being scheduled.
     * @details Can be called from another task or from the task itself.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the task has not been started.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode suspend() noexcept = 0;

    /**
     * @brief  Resume a suspended task.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the task has not been started.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode resume() noexcept = 0;

    /**
     * @brief  Delete the task and free its RTOS resources.
     * @details After this call the object may be re-started via @ref start().
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the task was not running.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode destroy() noexcept = 0;

    // -------------------------------------------------------------------------
    // State queries
    // -------------------------------------------------------------------------

    /**
     * @brief  Check whether the task is currently running (not suspended or deleted).
     * @return @c true if the task is scheduled for execution.
     */
    [[nodiscard]]
    virtual bool isRunning() const noexcept = 0;

    /**
     * @brief  Copy the task name assigned at @ref start() into an output string.
     * @param[out] name  Destination string; receives the task name.
     * @return @ref ReturnCode::AnsweredRequest on success.
     * @return @ref ReturnCode::NotInitialized if the task has not been started.
     */
    [[nodiscard]]
    virtual ReturnCode getName(String& name) const noexcept = 0;

    /**
     * @brief  Notify the task, unblocking it if it is waiting on a notification.
     * @details Can be called from another task or from an ISR.
     */
    virtual void notify() noexcept = 0;

    /**
     * @brief  Notify the task from an ISR context.
     * @return @c true if a higher-priority task was woken by this notification.
     */
    [[nodiscard]]
    virtual bool notifyFromISR() noexcept = 0;

    /**
     * @brief  Wait for a notification, optionally with a timeout.
     * @param[in]  timeoutMs  Maximum time to wait in ticks, or the maximum
     *                        value of @ref TickType to wait indefinitely.
     * @return @c true if the task was notified before the timeout expired.
     */
    [[nodiscard]]
    virtual bool notifyTake(TickType timeoutMs) noexcept = 0;

protected:

    iTask() noexcept = default;

    /**
     * @brief  Task entry point — implement the task body here.
     * @details Called by the RTOS in the context of the created task.
     *          This method must not return; it should contain an infinite loop.
     */
    virtual void run() noexcept = 0;

    iTask(const iTask&)             = delete;
    iTask& operator=(const iTask&)  = delete;
    iTask(iTask&&)                  = delete;
    iTask& operator=(iTask&&)       = delete;
};

} // namespace hel

#endif // HELIOS_KERNEL_ITASK_HPP_
