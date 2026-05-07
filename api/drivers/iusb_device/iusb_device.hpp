/**
 ******************************************************************************
 * @file    iusb_device.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_DRV_USB_DEVICE
 * @brief   USB device-mode stack interface.
 *

 */

#ifndef HELIOS_DRV_IUSB_DEVICE_HPP_
#define HELIOS_DRV_IUSB_DEVICE_HPP_

#include <cstdint>

namespace hel
{

/**
 * @brief   USB device-mode stack interface.
 * @details
 *   - Hardware-agnostic USB device abstraction.
 *   - Implement for each target platform.
 * @ingroup HELIOS_DRV_USB_DEVICE
 */
class iUsbDevice
{
public:
};

} // namespace hel

#endif // HELIOS_DRV_IUSB_DEVICE_HPP_
