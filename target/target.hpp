#ifndef HELIOS_TARGET_HPP_
#define HELIOS_TARGET_HPP_


#if __has_include("esp32.h")
#include "esp32/esp32.hpp"
#endif

#if __has_include("linux.h")
#include "linux/linux.hpp"
#endif

#if __has_include("nxp.h")
#include "nxp/nxp.hpp"
#endif

#if __has_include("silabs.h")
#include "silabs/silabs.hpp"
#endif

#if __has_include("stm32_hal_legacy.h")
#include "stm32/stm32.hpp"
#endif


#if __has_include(<FreeRTOS.h>)
#if __has_include(<freertos/task.hpp>)
#include <freertos/task.hpp>
#include <freertos/mutex.hpp>
#include <freertos/queue.hpp>
#include <freertos/semaphore.hpp>
#endif
#elif __has_include(<threadx.h>)
#endif

#if HeliosQt
#include "qt/qt.hpp"
#endif


#endif // HELIOS_TARGET_HPP_
