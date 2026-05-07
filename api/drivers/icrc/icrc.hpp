/**
 ******************************************************************************
 * @file    icrc.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_DRV_CRC
 * @brief   CRC calculation engine interface.
 *
 * @details
 *   - Single-call CRC computation over a contiguous byte buffer
 *   - Incremental accumulation across multiple calls without resetting
 *   - Explicit accumulator reset via @ref reset
 *   - No dynamic allocation; implementations map each method to the target HAL
 *
 * @note
 *   All methods are synchronous; the hardware CRC unit completes before returning.
 *   Methods are **not** thread-safe on the same instance; caller must serialize access.
 */

#ifndef HELIOS_DRV_ICRC_HPP_
#define HELIOS_DRV_ICRC_HPP_

#include <hel_target>
#include <hel_bytearray>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

/**
 * @class  iCrc
 * @brief  Hardware-agnostic CRC calculation engine interface.
 * @ingroup HELIOS_DRV_CRC
 *
 * @details
 *   - Single-call computation via @ref compute (resets accumulator internally)
 *   - Streaming computation via @ref accumulate (preserves accumulator state)
 *   - Explicit accumulator reset via @ref reset
 *   - Implementations shall map each virtual method to the target HAL
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access. Reentrant across distinct instances.
 *        Copy and move are deleted; CRC peripherals are singletons owned by the BSP
 *        layer and must not be duplicated or relocated.
 */
class iCrc
{
public:
  virtual ~iCrc() noexcept = default;

  // -------------------------------------------------------------------------
  // CRC computation
  // -------------------------------------------------------------------------

  /**
   * @brief      Compute the CRC over an entire buffer.
   * @details    Resets the internal accumulator before processing @p data,
   *             then writes the final CRC to @p crc.
   * @param[in]  data  View of bytes to process.
   * @param[out] crc   CRC result written on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Computation complete; @p crc is valid.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p crc is unspecified.
   */
  [[nodiscard]] virtual ReturnCode compute(
    ConstByteArray data, uint32_t& crc) noexcept = 0;

  /**
   * @brief      Accumulate the CRC over a buffer without resetting the accumulator.
   * @details    Feeds @p data into the running CRC state and writes the updated
   *             value to @p crc. Call @ref reset before the first chunk if starting
   *             a new sequence.
   * @param[in]  data  View of bytes to process.
   * @param[out] crc   Updated CRC result written on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Accumulation complete; @p crc is valid.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; @p crc is unspecified.
   */
  [[nodiscard]] virtual ReturnCode accumulate(
    ConstByteArray data, uint32_t& crc) noexcept = 0;

  // -------------------------------------------------------------------------
  // Accumulator control
  // -------------------------------------------------------------------------

  /**
   * @brief  Reset the internal CRC accumulator to its initial value.
   * @note   Call before the first @ref accumulate in a new sequence.
   */
  virtual void reset() noexcept = 0;

protected:
  iCrc& operator=(const iCrc&) = delete;
  iCrc& operator=(iCrc&&) = delete;
};

} // namespace hel

#endif // HELIOS_DRV_ICRC_HPP_
