/**
 ******************************************************************************
 * @file    stm32.hpp
 * @author  Samuel Almeida
 * @version 1.0.0
 * @date    Mar 25, 2026
 * @brief   
 *
 */

#ifndef HELIOS_PLATFORM_STM32_STM32_HPP_
#define HELIOS_PLATFORM_STM32_STM32_HPP_

#if __has_include("stm32_hal_legacy.h")

#include <cstdint>
#include <hel_string>

/**
 * @defgroup stm32
 * @brief STM32 platform support
 * @{
 */

/**
 * @brief Concatenates two tokens.
 *
 * This macro takes two parameters and concatenates them into a single token.
 *
 * @param a First token.
 * @param b Second token.
 */
#define HEL_STM32_CONCAT(a, b) a##b

/**
 * @brief Constructs the HAL header file name for the specified series.
 * @param _SERIE The series identifier for the target MCU.
 */
#define HEL_STM32_TARGET(_MCU_FAMILY) HEL_STM32_CONCAT(_MCU_FAMILY, _hal.h)

/**
 * @brief Constructs the HAL configuration header file name for the specified series.
 * @param _SERIE The series identifier for the target MCU.
 */
#define HEL_STM32_HAL_CONF(_MCU_FAMILY) HEL_STM32_CONCAT(_MCU_FAMILY, _hal_conf.h)

/**
 * @brief Converts a macro to a string.
 * @param x The macro to be converted to a string.
 */
#define HEL_STM32_XSTR(x) #x

/**
 * @brief Generates the path for HAL header files.
 * @param NAME The name of the target HAL file.
 */
#define HEL_STM32_INCLUDE(NAME) HEL_STM32_XSTR(NAME)

#ifndef HEL_STM32_MCU_FAMILY
/* H7 */
#if defined(STM32H743xx) || defined(STM32H753xx) || defined(STM32H750xx) || \
      defined(STM32H7A3xxQ) || defined(STM32H723xx) || defined(STM32H7xx)
#define HEL_STM32_MCU_FAMILY STM32H7xx

/* F4 */
#elif defined(STM32F401xE) || defined(STM32F405xx) || defined(STM32F407xx) || \
        defined(STM32F411xE) || defined(STM32F412Zx) || defined(STM32F429xx) || \
        defined(STM32F446xx) || defined(STM32F4xx) || defined(STM32F469xx)
    #define HEL_STM32_MCU_FAMILY STM32F4xx

  /* L4 */
  #elif defined(STM32L431xx) || defined(STM32L432xx) || defined(STM32L476xx) || \
        defined(STM32L496xx) || defined(STM32L4R5xx) || defined(STM32L4xx)
    #define HEL_STM32_MCU_FAMILY STM32L4xx

  /* Adicione outras famílias que realmente usa */
#elif defined(STM32WB5Mxx)
    #define HEL_STM32_MCU_FAMILY stm32wbxx
#else
    #error "Não foi possível inferir a família. Defina HEL_STM32_MCU_FAMILY manualmente ou amplie o mapeamento."
  #endif
#endif

// Include the appropriate HAL header files based on the target MCU.
#if defined(HEL_STM32_MCU_FAMILY)
#include HEL_STM32_INCLUDE(HEL_STM32_TARGET(HEL_STM32_MCU_FAMILY))
#include HEL_STM32_INCLUDE(HEL_STM32_HAL_CONF(HEL_STM32_MCU_FAMILY))
#endif

/**
 * @brief Type definition for I2C handle.
 *
 * This type is used to represent the I2C handle in the driver.
 */
#if defined(HAL_I2C_MODULE_ENABLED)
using I2C_handle = I2C_HandleTypeDef;
#endif // HAL_I2C_MODULE_ENABLED

    /**
     * @brief Type definition for SPI handle.
     *
     * This type is used to represent the SPI handle in the driver.
     */
#if defined(HAL_SPI_MODULE_ENABLED)
    using SPI_handle = SPI_HandleTypeDef;
#endif // HAL_SPI_MODULE_ENABLED

    /**
     * @brief Type definition for UART handle.
     *
     * This type is used to represent the UART handle in the driver.
     */
#if defined(HAL_UART_MODULE_ENABLED)
    using UART_handle = UART_HandleTypeDef;
#endif // HAL_UART_MODULE_ENABLED

    /**
     * @brief Type definition for TIM handle.
     *
     * This type is used to represent the TIM handle in the driver.
     */
#if defined(HAL_TIM_MODULE_ENABLED)
    using TIM_handle = TIM_HandleTypeDef;
#endif // HAL_TIM_MODULE_ENABLED

    /**
     * @brief Type definition for ADC handle.
     *
     * This type is used to represent the ADC handle in the driver.
     */
#if defined(HAL_ADC_MODULE_ENABLED)
    using ADC_handle = ADC_HandleTypeDef;
#endif // HAL_ADC_MODULE_ENABLED

    /**
     * @brief Type definition for FDCAN handle.
     *
     * This type is used to represent the FDCAN handle in the driver.
     */
#if defined(HAL_FDCAN_MODULE_ENABLED)
using FDCAN_handle = FDCAN_HandleTypeDef;
#endif // HAL_FDCAN_MODULE_ENABLED

    /**
     * \brief GPIO pin configuration
     *
     * A logical representation of a pin's configuration.
     */
    enum class IoPortPin : uint16_t {
#if defined(GPIOA)
    PA0 = 0U, PA1 = 1U, PA2 = 2U, PA3 = 3U, PA4 = 4U, PA5 = 5U, PA6 = 6U,
    PA7 = 7U, PA8 = 8U, PA9 = 9U, PA10 = 10U, PA11 = 11U, PA12 = 12U,
    PA13 = 13U, PA14 = 14U, PA15 = 15U,
#endif
#if defined(GPIOB)
    PB0 = 16U, PB1 = 17U, PB2 = 18U, PB3 = 19U, PB4 = 20U, PB5 = 21U, PB6 = 22U,
    PB7 = 23U, PB8 = 24U, PB9 = 25U, PB10 = 26U, PB11 = 27U, PB12 = 28U,
    PB13 = 29U, PB14 = 30U, PB15 = 31U,
#endif
#if defined(GPIOC)
    PC0 = 32U, PC1 = 33U, PC2 = 34U, PC3 = 35U, PC4 = 36U, PC5 = 37U, PC6 = 38U,
    PC7 = 39U, PC8 = 40U, PC9 = 41U, PC10 = 42U, PC11 = 43U, PC12 = 44U,
    PC13 = 45U, PC14 = 46U, PC15 = 47U,
#endif
#if defined(GPIOD)
    PD0 = 48U, PD1 = 49U, PD2 = 50U, PD3 = 51U, PD4 = 52U, PD5 = 53U, PD6 = 54U,
    PD7 = 55U, PD8 = 56U, PD9 = 57U, PD10 = 58U, PD11 = 59U, PD12 = 60U,
    PD13 = 61U, PD14 = 62U, PD15 = 63U,
#endif
#if defined(GPIOE)
    PE0 = 64U, PE1 = 65U, PE2 = 66U, PE3 = 67U, PE4 = 68U, PE5 = 69U, PE6 = 70U,
    PE7 = 71U, PE8 = 72U, PE9 = 73U, PE10 = 74U, PE11 = 75U, PE12 = 76U,
    PE13 = 77U, PE14 = 78U, PE15 = 79U,
#endif
#if defined(GPIOF)
    PF0 = 80U, PF1 = 81U, PF2 = 82U, PF3 = 83U, PF4 = 84U, PF5 = 85U, PF6 = 86U,
    PF7 = 87U, PF8 = 88U, PF9 = 89U, PF10 = 90U, PF11 = 91U, PF12 = 92U,
    PF13 = 93U, PF14 = 94U, PF15 = 95U,
#endif
#if defined(GPIOG)
    PG0 = 96U, PG1 = 97U, PG2 = 98U, PG3 = 99U, PG4 = 100U, PG5 = 101U,
    PG6 = 102U, PG7 = 103U, PG8 = 104U, PG9 = 105U, PG10 = 106U, PG11 = 107U,
    PG12 = 108U, PG13 = 109U, PG14 = 110U, PG15 = 111U,
#endif
#if defined(GPIOH)
    PH0 = 112U, PH1 = 113U, PH2 = 114U, PH3 = 115U, PH4 = 116U, PH5 = 117U,
    PH6 = 118U, PH7 = 119U, PH8 = 120U, PH9 = 121U, PH10 = 122U, PH11 = 123U,
    PH12 = 124U, PH13 = 125U, PH14 = 126U, PH15 = 127U,
#endif
#if defined(GPIOI)
    PI0  = 128U,
    PI1  = 129U,
    PI2  = 130U,
    PI3  = 131U,
    PI4  = 132U,
    PI5  = 133U,
    PI6  = 134U,
    PI7  = 135U,
    PI8  = 136U,
    PI9  = 137U,
    PI10 = 138U,
    PI11 = 139U,
    PI12 = 140U,
    PI13 = 141U,
    PI14 = 142U,
    PI15 = 143U,
#endif

    NC = 0xFFFFU, };

enum UartPorts : uint8_t
{
#if defined(USART1)
    HEL_USART1 = 0U,
#endif
#if defined(USART2)
    HEL_USART2 = 1U,
#endif
#if defined(USART3)
    HEL_USART3 = 2U,
#endif
#if defined(UART4)
    HEL_UART4 = 3U,
#endif
#if defined(UART5)
    HEL_UART5 = 4U,
#endif
#if defined(USART6)
    HEL_USART6 = 5U,
#endif
#if defined(UART7)
    HEL_UART7 = 6U,
#endif
#if defined(UART8)
    HEL_UART8 = 7U,
#endif
#if defined(USART9)
    HEL_USART9 = 8U,
#endif
#if defined(USART10)
    HEL_USART10 = 9U,
#endif
#if defined(LPUART1)
    HEL_LPUART1 = 10U,
#endif
#if defined(LPUART2)
    HEL_LPUART2 = 10U,
#endif
    _HEL_UART_COUNT
};

#define HEL_TARGET_TICK() HAL_GetTick()


static inline const uint32_t* GetUniqueId() noexcept
{
    static uint32_t uid[3];
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
    return uid;
}

#endif // __has_include("stm32_hal_legacy.h")
#endif // HELIOS_PLATFORM_STM32_STM32_HPP_
