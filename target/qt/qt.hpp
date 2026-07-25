/**
 ******************************************************************************
 * @file    qt.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_TARGET_QT
 * @brief   Qt-based desktop/simulation target.
 *
 * @details
 *   - Provides Qt platform bindings for host-side testing and simulation.
 *   - Enabled when the build flag @c HeliosQt is defined.
 *

 */

#ifndef HELIOS_TARGET_QT_HPP_
#define HELIOS_TARGET_QT_HPP_


#if defined(QT_HELIOS_SIM) // Qt simulation mode

#include <QElapsedTimer>
#include <QSerialPort>
#include <cstdint>

#include <mutex.hpp>
#include <task.hpp>
#include <queue.hpp>
#include <semaphore.hpp>

/**
     * \brief GPIO pin configuration
     *
     * A logical representation of a pin's configuration.
     */
enum class IoPortPin : uint16_t
{
    GPIO_0 = 0U,
    GPIO_1 = 1U,
    GPIO_2 = 2U,
    GPIO_3 = 3U,
    GPIO_4 = 4U,
    GPIO_5 = 5U,
    GPIO_6 = 6U,
    GPIO_7 = 7U,
    GPIO_8 = 8U,
    GPIO_9 = 9U,
    GPIO_10 = 10U,
    GPIO_11 = 11U,
    GPIO_12 = 12U,
    GPIO_13 = 13U,
    GPIO_14 = 14U,
    GPIO_15 = 15U,
    GPIO_16 = 16U,
    GPIO_17 = 17U,
    GPIO_18 = 18U,
    GPIO_19 = 19U,

    NC = 0xFFFFU
};



/**
 * @brief  Monotonic millisecond tick since the first call.
 *
 * Host-side counterpart of the HAL tick counter; the clock starts on the first
 * call rather than at reset, so only differences between readings are meaningful.
 *
 * @return Milliseconds elapsed, wrapping every ~49 days like the 32-bit HAL tick.
 */
static inline uint32_t hel_qt_tick() noexcept
{
    static QElapsedTimer timer;
    if (!timer.isValid())
    {
        timer.start();
    }
    return static_cast<uint32_t>(timer.elapsed());
}


/**
 * @brief Millisecond tick source consumed by @ref hel::Pit.
 */
#define HEL_TARGET_TICK() hel_qt_tick()


#endif // defined(QT_HELIOS_SIM)
#endif // HELIOS_TARGET_QT_HPP_
