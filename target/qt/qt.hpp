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

#include <QElapsedTimer>
#include <QSerialPort>
#include <cstdint>


/**
 * @brief Type definition for UART handle.
 *
 * This type is used to represent the UART handle in the driver.
 */
using UART_handle = QSerialPort;

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

#endif // HELIOS_TARGET_QT_HPP_
