/**
 ******************************************************************************
 * @file    iqueue.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.2.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL_QUEUE
 * @brief   Inter-task message queue interface.
 */

#ifndef HELIOS_KERNEL_IQUEUE_HPP_
#define HELIOS_KERNEL_IQUEUE_HPP_

#include <hel_return_code>
#include "rtos_types.hpp"
#include <type_traits>

namespace hel
{

/**
 * @class  iQueue
 * @brief  RTOS-agnostic inter-task message queue interface.
 * @ingroup HELIOS_KERNEL_QUEUE
 *
 * @tparam T  Item type stored in the queue. Must be trivially copyable:
 *            RTOS backends copy items into a statically-allocated buffer
 *            with a raw byte copy, so @p T must not manage any resource
 *            that requires a non-trivial copy, move, or destructor.
 */
template<class T>
class iQueue
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "iQueue<T> requires T to be trivially copyable: the RTOS "
                  "backend copies items into a static buffer with a raw byte copy.");

public:

    virtual ~iQueue() noexcept = default;

    // -------------------------------------------------------------------------
    // Send
    // -------------------------------------------------------------------------

    /**
     * @brief  Post an item to the back of the queue from task context.
     * @param[in]  item           Item to copy into the queue.
     * @param[in]  timeout_ticks  Maximum wait time in RTOS ticks if the queue is full.
     *                            Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorTimeout if the queue remained full for @p timeout_ticks.
     * @return @ref ReturnCode::ErrorQueueSendFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToBack(const T& item, TickType timeout_ticks) noexcept = 0;

    /**
     * @brief  Post an item to the back of the queue from an ISR context.
     * @param[in]  item  Item to copy into the queue.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorQueueFull if the queue has no space.
     * @return @ref ReturnCode::ErrorQueueSendFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToBackFromIsr(const T& item) noexcept = 0;

    /**
     * @brief  Post an item to the front of the queue from task context.
     * @param[in]  item           Item to copy into the queue.
     * @param[in]  timeout_ticks  Maximum wait time in RTOS ticks if the queue is full.
     *                            Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorTimeout if the queue remained full for @p timeout_ticks.
     * @return @ref ReturnCode::ErrorQueueSendFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToFront(const T& item, TickType timeout_ticks) noexcept = 0;

    /**
     * @brief  Post an item to the front of the queue from an ISR context.
     * @param[in]  item  Item to copy into the queue.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorQueueFull if the queue has no space.
     * @return @ref ReturnCode::ErrorQueueSendFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToFrontFromIsr(const T& item) noexcept = 0;


    // -------------------------------------------------------------------------
    // Receive
    // -------------------------------------------------------------------------

    /**
     * @brief  Retrieve an item from the front of the queue, blocking until available.
     * @param[out] item           Buffer to copy the retrieved item into.
     * @param[in]  timeout_ticks  Maximum wait time in RTOS ticks.
     *                            Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if an item was retrieved.
     * @return @ref ReturnCode::ErrorTimeout if no item arrived within @p timeout_ticks.
     * @return @ref ReturnCode::ErrorQueueReceiveFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode receive(T& item, TickType timeout_ticks) noexcept = 0;

    /**
     * @brief  Retrieve an item from the front of the queue from an ISR context.
     * @param[out] item  Buffer to copy the retrieved item into.
     * @return @ref ReturnCode::AnsweredRequest if an item was retrieved.
     * @return @ref ReturnCode::ErrorQueueEmpty if the queue is empty.
     * @return @ref ReturnCode::ErrorQueueReceiveFailed on any other failure.
     */
    [[nodiscard]]
    virtual ReturnCode receiveFromIsr(T& item) noexcept = 0;

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------

    /**
     * @brief  Reset the queue, discarding all items without freeing memory.
     * @details Safe to call from task context only.
     */
    virtual void reset() noexcept = 0;

    /**
     * @brief  Remove all items from the queue.
     * @details Alias for @ref reset(), provided for readability at call sites;
     *          not a separate virtual entry point.
     */
    void clear() noexcept { reset(); }

    // -------------------------------------------------------------------------
    // State queries
    // -------------------------------------------------------------------------

    /**
     * @brief  Return the number of items currently in the queue.
     * @return Item count.
     */
    [[nodiscard]]
    virtual UBaseType count() const noexcept = 0;

    /**
     * @brief  Return the number of items currently in the queue from an ISR context.
     * @return Item count.
     */
    [[nodiscard]]
    virtual UBaseType countFromIsr() const noexcept = 0;

    /**
     * @brief  Return the maximum number of items the queue can hold.
     * @return Queue capacity in items.
     */
    [[nodiscard]]
    virtual UBaseType capacity() const noexcept = 0;

    /**
     * @brief  Return the maximum number of items the queue can hold from an ISR context.
     * @return Queue capacity in items.
     */
    [[nodiscard]]
    virtual UBaseType capacityFromIsr() const noexcept = 0;

    /**
     * @brief  Check whether the queue contains no items.
     * @return @c true if the queue is empty.
     */
    [[nodiscard]]
    virtual bool isEmpty() const noexcept = 0;

    /**
     * @brief  Check whether the queue is empty from an ISR context.
     * @return @c true if the queue is empty.
     */
    [[nodiscard]]
    virtual bool isEmptyFromIsr() const noexcept = 0;

    /**
     * @brief  Check whether the queue has no remaining space.
     * @return @c true if the queue is full.
     */
    [[nodiscard]]
    virtual bool isFull() const noexcept = 0;

    /**
     * @brief  Check whether the queue is full from an ISR context.
     * @return @c true if the queue is full.
     */
    [[nodiscard]]
    virtual bool isFullFromIsr() const noexcept = 0;

protected:

    iQueue() noexcept = default;

    iQueue(const iQueue&)             = delete;
    iQueue& operator=(const iQueue&)  = delete;
    iQueue(iQueue&&)                  = delete;
    iQueue& operator=(iQueue&&)       = delete;
};

} // namespace hel

#endif // HELIOS_KERNEL_IQUEUE_HPP_
