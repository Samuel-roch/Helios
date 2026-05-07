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

#include <QSerialPort>

/**
 * @brief Type definition for UART handle.
 *
 * This type is used to represent the UART handle in the driver.
 */
using hel_uart_handle = QSerialPort;

#endif // HELIOS_TARGET_QT_HPP_
