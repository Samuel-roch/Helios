/**
 ******************************************************************************
 * @file    rtos_types.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.1.0
 * @date    2026-05-07
 * @ingroup HELIOS_KERNEL
 * @brief   RTOS primitive type aliases for the Helios kernel interfaces.
 *
 * @details
 *   When FreeRTOS headers are on the include path the aliases map directly to
 *   the native FreeRTOS types (TickType_t, UBaseType_t, …).  Otherwise portable
 *   fallbacks based on standard fixed-width integers are used, allowing kernel
 *   interface headers to compile on any target without depending on a specific
 *   RTOS SDK.
 *
 *   Concrete implementation files that call FreeRTOS APIs directly must include
 *   <FreeRTOS.h> themselves — this header does not force that dependency.
 */

#ifndef HELIOS_KERNEL_RTOS_TYPES_HPP_
#define HELIOS_KERNEL_RTOS_TYPES_HPP_

#include <cstdint>

#if __has_include(<FreeRTOS.h>)
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <timers.h>
#endif

namespace hel
{

#if __has_include(<FreeRTOS.h>)

// -------------------------------------------------------------------------
// FreeRTOS mapping
// -------------------------------------------------------------------------

/** @brief Signed base integer type used by FreeRTOS return values. */
using BaseType       = BaseType_t;

/** @brief Unsigned base integer type used by FreeRTOS counters and sizes. */
using UBaseType      = UBaseType_t;

/** @brief Tick-counter type; width depends on @c configUSE_16_BIT_TICKS. */
using TickType       = TickType_t;

/** @brief Stack word element type (matches the MCU word width). */
using StackType      = StackType_t;

/** @brief Stack-depth argument type accepted by @c xTaskCreate. */
using StackDepthType = configSTACK_DEPTH_TYPE;

/** @brief Opaque handle to a FreeRTOS task. */
using TaskHandle     = TaskHandle_t;

/** @brief Opaque handle to a FreeRTOS queue. */
using QueueHandle    = QueueHandle_t;

/** @brief Opaque handle to a FreeRTOS semaphore or mutex. */
using SemHandle      = SemaphoreHandle_t;

/** @brief Opaque handle to a FreeRTOS software timer. */
using TimerHandle    = TimerHandle_t;

/** @brief Static-allocation control block for a task (no heap). */
using StaticTaskBuf  = StaticTask_t;

/** @brief Static-allocation control block for a queue (no heap). */
using StaticQueueBuf = StaticQueue_t;

/** @brief Static-allocation control block for a semaphore (no heap). */
using StaticSemBuf   = StaticSemaphore_t;

/** @brief Static-allocation control block for a software timer (no heap). */
using StaticTimerBuf = StaticTimer_t;

#else

// -------------------------------------------------------------------------
// Portable fallbacks (no RTOS SDK on the include path)
// -------------------------------------------------------------------------

/** @brief Signed base integer type (portable fallback). */
using BaseType       = int32_t;

/** @brief Unsigned base integer type (portable fallback). */
using UBaseType      = uint32_t;

/** @brief Tick-counter type; unit is implementation-defined RTOS ticks, not
 *         necessarily milliseconds (portable fallback). */
using TickType       = uint32_t;

/** @brief Stack word element type (portable fallback). */
using StackType      = uint32_t;

/** @brief Stack-depth type in words (portable fallback). */
using StackDepthType = uint32_t;

/** @brief Opaque task handle (portable fallback). */
using TaskHandle     = void*;

/** @brief Opaque queue handle (portable fallback). */
using QueueHandle    = void*;

/** @brief Opaque semaphore / mutex handle (portable fallback). */
using SemHandle      = void*;

/** @brief Opaque software timer handle (portable fallback). */
using TimerHandle    = void*;

#endif // __has_include(<FreeRTOS.h>)

} // namespace hel

#endif // HELIOS_KERNEL_RTOS_TYPES_HPP_
