/**
 ******************************************************************************
 * @file    isdio.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    2026-04-17
 * @ingroup HELIOS_DRV_SDIO
 * @brief   SD/MMC card interface.
 *

 */

#ifndef HELIOS_DRV_ISDIO_HPP_
#define HELIOS_DRV_ISDIO_HPP_

#include <cstdint>

namespace hel
{

/**
 * @brief   SD/MMC card interface.
 * @details
 *   - Hardware-agnostic SDIO abstraction.
 *   - Implement for each target platform.
 * @ingroup HELIOS_DRV_SDIO
 */
class iSdio
{
public:
};

} // namespace hel

#endif // HELIOS_DRV_ISDIO_HPP_
