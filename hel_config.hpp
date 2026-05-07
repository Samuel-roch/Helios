/**
 ******************************************************************************
 * @file    hel_congif.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-18
 * @ingroup HELIOS
 * @brief   Feature detection macros for conditional compilation based on available
 */

#ifndef HELIOS_HEL_CONFIG_HPP_
#define HELIOS_HEL_CONFIG_HPP_

#if __has_include(<FreeRTOS.h>)

#define HELIOS_USE_RTOS
#elif __has_include(<threadx.h>)
#endif

#if defined(HELIOS_USE_RTOS)
static inline constexpr bool hel_use_rtos = true;
#else
static inline constexpr bool hel_use_rtos = false;
#endif


#endif // HELIOS_HEL_CONFIG_HPP_
