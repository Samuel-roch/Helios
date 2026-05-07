/**
 ******************************************************************************
 * @file    iadc.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_ADC
 * @brief   ADC analog-to-digital converter interface.
 *
 * @details
 *   - Blocking single-channel conversion with millisecond timeout
 *   - Asynchronous multi-channel DMA conversion via @ref startDMA / @ref stopDMA
 *   - Event notification via @ref AdcCallback on DMA completion or error
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   Blocking methods must not be called from an ISR context.
 *   The DMA callback fires from ISR or driver-task context (implementation-defined).
 */

#ifndef HELIOS_DRV_IADC_HPP_
#define HELIOS_DRV_IADC_HPP_

#include <hel_target>
#include <hel_span>
#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

enum class AdcEvent : uint8_t
{
  ConversionComplete,  /*!< DMA conversion completed successfully; buffer is valid. */
  Error,               /*!< DMA conversion failed; buffer contents are unspecified. */
};

/**
 * @brief Callable type used to receive asynchronous ADC events.
 *
 * @details Signature: `void handler(AdcEvent event, uint16_t count) noexcept`
 *   - @p event  — event kind (@ref AdcEvent::ConversionComplete or @ref AdcEvent::Error).
 *   - @p count  — number of conversions completed (0 on error).
 */
using AdcCallback = Callback<void(AdcEvent, uint16_t)>;

/**
 * @class  iAdc
 * @brief  Hardware-agnostic ADC analog-to-digital converter interface.
 * @ingroup HELIOS_DRV_ADC
 *
 * @details
 *   - Blocking single-channel conversion with configurable millisecond timeout
 *   - DMA-based multi-channel scan via @ref startDMA and @ref stopDMA
 *   - Single event callback registered via @ref setCallback; fires on DMA
 *     completion or error
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Blocking methods must not be called from an ISR context.
 *        Copy and move are deleted; ADC peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iAdc
{
public:
  virtual ~iAdc() noexcept = default;

  // -------------------------------------------------------------------------
  // Blocking conversion
  // -------------------------------------------------------------------------

  // TODO: consider adding method for configuring ADC parameters (resolution, sampling time, etc.)

  /**
   * @brief      Perform a single-channel conversion and block until done or timeout.
   * @param[in]  channel     Peripheral channel index (implementation-defined range).
   * @param[out] value       Raw conversion result written on success.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Conversion complete; @p value is valid.
   *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed; @p value is unspecified.
   *   - @ref ReturnCode::FunctionBusy   : A DMA conversion is active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p value is unspecified.
   * @note  Must not be called from an ISR context.
   */
  [[nodiscard]]
  virtual ReturnCode read(
    uint32_t channel, uint16_t& value, uint32_t timeout_ms) noexcept = 0;

  // -------------------------------------------------------------------------
  // Asynchronous DMA conversion
  // -------------------------------------------------------------------------

  /**
   * @brief     Start an asynchronous multi-channel DMA conversion.
   * @details   The peripheral fills @p buffer with one raw sample per enabled
   *            channel per scan. The registered @ref AdcCallback fires on
   *            completion or error.
   * @param[in] buffer  Writable span to receive raw samples; must remain valid
   *                    until the callback fires or @ref stopDMA is called.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : DMA started; callback will fire.
   *   - @ref ReturnCode::FunctionBusy   : A conversion is already active; nothing started.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; nothing started.
   * @note  Completion or error is reported through the registered @ref AdcCallback.
   */
  [[nodiscard]]
  virtual ReturnCode startDMA(Span<uint16_t> buffer) noexcept = 0;

  /**
   * @brief  Stop an ongoing DMA conversion.
   * @details The registered @ref AdcCallback is invoked after the peripheral stops;
   *          the sample count delivered is implementation-defined.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Stop issued; callback will fire.
   *   - @ref ReturnCode::NotInitialized  : No active DMA conversion; nothing stopped.
   */
  [[nodiscard]]
  virtual ReturnCode stopDMA() noexcept = 0;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief     Register (or replace) the asynchronous event callback.
   * @param[in] callback  Callable invoked on every DMA event (@ref AdcCallback).
   *                      Pass a default-constructed @ref AdcCallback to clear.
   * @note  Safe to call at any time; takes effect for the next DMA conversion.
   */
  virtual void setCallback(AdcCallback callback) noexcept = 0;

protected:
  iAdc& operator=(const iAdc&) = delete;
  iAdc& operator=(iAdc&&) = delete;

  /**
   * @brief     Internal handler for ADC events, called by the driver/ISR.
   * @param[in] event  Kind of event that occurred (@ref AdcEvent).
   * @param[in] count  Number of conversions completed (0 on error).
   * @warning   Must only be called from the driver implementation or ISR; not part of the public API.
   */
  virtual void handleEvent(AdcEvent event, uint16_t count) = 0;
};

} // namespace hel

#endif // HELIOS_DRV_IADC_HPP_
