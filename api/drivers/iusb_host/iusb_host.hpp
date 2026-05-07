/**
 ******************************************************************************
 * @file    iusb_host.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_DRV_USB_HOST
 * @brief   USB host-mode stack interface.
 *

 */

#ifndef HELIOS_DRV_IUSB_HOST_HPP_
#define HELIOS_DRV_IUSB_HOST_HPP_

#include <cstdint>

namespace hel
{

/**
 * @brief   USB host-mode stack interface.
 * @details
 *   - Hardware-agnostic USB host abstraction.
 *   - Implement for each target platform.
 * @ingroup HELIOS_DRV_USB_HOST
 */
class iUsbHost
{
public:
};

} // namespace hel

#endif // HELIOS_DRV_IUSB_HOST_HPP_
