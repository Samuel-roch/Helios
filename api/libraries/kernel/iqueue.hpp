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

namespace hel
{


template<class T>
class iQueue
{
public:

    virtual ~iQueue() noexcept = default;

    // -------------------------------------------------------------------------
    // Send
    // -------------------------------------------------------------------------

    /**
     * @brief  Post an item to the back of the queue from task context.
     * @param[in]  item        Item to copy into the queue.
     * @param[in]  timeout_ms  Maximum wait time in ticks if the queue is full.
     *                         Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorTimeout if the queue remained full for @p timeout_ms.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToBack(const T& item, TickType timeout_ms) noexcept = 0;

    /**
     * @brief  Post an item to the back of the queue from an ISR context.
     * @param[in]  item  Item to copy into the queue.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorQueueFull if the queue has no space.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToBackFromIsr(const T& item) noexcept = 0;

    /**
     * @brief  Post an item to the front of the queue from task context.
     * @param[in]  item        Item to copy into the queue.
     * @param[in]  timeout_ms  Maximum wait time in ticks if the queue is full.
     *                         Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorTimeout if the queue remained full for @p timeout_ms.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToFront(const T& item, TickType timeout_ms) noexcept = 0;

    /**
     * @brief  Post an item to the front of the queue from an ISR context.
     * @param[in]  item  Item to copy into the queue.
     * @return @ref ReturnCode::AnsweredRequest if the item was posted.
     * @return @ref ReturnCode::ErrorQueueFull if the queue has no space.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode sendToFrontFromIsr(const T& item) noexcept = 0;


    // -------------------------------------------------------------------------
    // Receive
    // -------------------------------------------------------------------------

    /**
     * @brief  Retrieve an item from the front of the queue, blocking until available.
     * @param[out] item        Buffer to copy the retrieved item into.
     * @param[in]  timeout_ms  Maximum wait time in ticks.
     *                         Pass the maximum value of @ref TickType to wait indefinitely.
     * @return @ref ReturnCode::AnsweredRequest if an item was retrieved.
     * @return @ref ReturnCode::ErrorTimeout if no item arrived within @p timeout_ms.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode receive(T& item, TickType timeout_ms) noexcept = 0;

    /**
     * @brief  Retrieve an item from the front of the queue from an ISR context.
     * @param[out] item  Buffer to copy the retrieved item into.
     * @return @ref ReturnCode::AnsweredRequest if an item was retrieved.
     * @return @ref ReturnCode::ErrorQueueEmpty if the queue is empty.
     * @return @ref ReturnCode::ErrorGeneral on failure.
     */
    [[nodiscard]]
    virtual ReturnCode receiveFromISR(T& item) noexcept = 0;

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
     * @details Equivalent to @ref reset(); provided for readability.
     */
    virtual void clear() noexcept = 0;

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
    virtual UBaseType countFromISR() const noexcept = 0;

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
    virtual UBaseType capacityFromISR() const noexcept = 0;

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
    virtual bool isEmptyFromISR() const noexcept = 0;

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
    virtual bool isFullFromISR() const noexcept = 0;

protected:

    iQueue() noexcept = default;

    iQueue(const iQueue&)             = delete;
    iQueue& operator=(const iQueue&)  = delete;
    iQueue(iQueue&&)                  = delete;
    iQueue& operator=(iQueue&&)       = delete;
};

} // namespace hel

#endif // HELIOS_KERNEL_IQUEUE_HPP_
