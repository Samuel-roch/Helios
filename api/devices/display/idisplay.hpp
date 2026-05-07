/**
 ******************************************************************************
 * @file    idisplay.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_DISPLAY
 * @brief   Display device interface.
 *
 * @details
 *   - Pixel-level drawing primitives: drawPixel, drawLine, drawRect, fillRect
 *   - Display-level control: power on/off, brightness, rotation
 *   - Explicit flush step for buffered (framebuffer) displays; async flush
 *     with completion notification via @ref DisplayCallback
 *   - On non-buffered displays flush() is a no-op that returns AnsweredRequest
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Colors are expressed as RGB888 packed into a @c uint32_t (0x00RRGGBB).
 *   The implementation converts to the display's native format (RGB565,
 *   monochrome, etc.).  For monochrome displays any non-zero color is treated
 *   as white (on).
 *   Coordinate (0, 0) is the top-left corner.  Coordinates are clipped silently
 *   to the display bounds; no error is returned for out-of-bounds drawing.
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_IDISPLAY_HPP_
#define HELIOS_DEV_IDISPLAY_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// DisplayRotation
// =============================================================================

/**
 * @enum  DisplayRotation
 * @brief Logical rotation of the display content relative to hardware orientation.
 * @ingroup HELIOS_DEV_DISPLAY
 */
enum class DisplayRotation : uint8_t
{
  Portrait0    = 0x00U, /*!< 0° — default hardware orientation.          */
  Landscape90  = 0x01U, /*!< 90° clockwise rotation.                     */
  Portrait180  = 0x02U, /*!< 180° rotation.                              */
  Landscape270 = 0x03U  /*!< 270° clockwise (90° counter-clockwise).     */
};

// =============================================================================
// DisplayEvent
// =============================================================================

/**
 * @enum  DisplayEvent
 * @brief Events reported via the registered @ref DisplayCallback.
 * @ingroup HELIOS_DEV_DISPLAY
 */
enum class DisplayEvent : uint8_t
{
  FlushComplete = 0x00U, /*!< Asynchronous framebuffer flush completed.   */
  ErrorFlush    = 0x01U  /*!< Asynchronous flush failed.                  */
};

// @formatter:on

// =============================================================================
// DisplayCallback
// =============================================================================

/**
 * @brief Callable type invoked when a display event occurs.
 *
 * @details Signature: `void handler(DisplayEvent event) noexcept`
 *
 * @note  The callback may fire from ISR or DMA-complete context; it must not
 *        block or allocate.
 */
using DisplayCallback = Callback<void(DisplayEvent)>;

// =============================================================================
// iDisplay
// =============================================================================

/**
 * @class  iDisplay
 * @brief  Hardware-agnostic display device interface.
 * @ingroup HELIOS_DEV_DISPLAY
 *
 * @details
 *   - Queries display geometry with @ref getWidth and @ref getHeight.
 *   - Draws individual pixels with @ref drawPixel, lines with @ref drawLine,
 *     hollow rectangles with @ref drawRect, and filled rectangles with
 *     @ref fillRect.
 *   - @ref clear fills the entire display (or framebuffer) with a single color.
 *   - For buffered displays, call @ref flush to push the framebuffer to hardware.
 *     For non-buffered displays @ref flush returns @ref ReturnCode::AnsweredRequest
 *     immediately without any transfer.
 *   - @ref flushAsync starts a DMA transfer and returns immediately; completion
 *     is reported via @ref DisplayCallback with @ref DisplayEvent::FlushComplete.
 *   - Brightness and power state are controlled with @ref setBrightness and
 *     @ref setOn; both return @ref ReturnCode::ErrorNotSupported on displays that
 *     do not expose these controls.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; display instances are singletons owned by the BSP layer.
 */
class iDisplay
{
public:
  virtual ~iDisplay() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the display and verify communication.
   * @details Sends the IC initialization sequence, configures the panel to a
   *   known state, and turns the display on.  Must be called before any other
   *   method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded.
   *   - @ref ReturnCode::ErrorWriteFailed    : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  /**
   * @brief     Power the display on or off.
   * @param[in] on  @c true to turn the panel on; @c false to turn it off.
   *   The framebuffer contents are preserved while the panel is off.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : State applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : Display does not support software power control.
   */
  [[nodiscard]]
  virtual ReturnCode setOn(bool on) noexcept = 0;

  /**
   * @brief     Set the display backlight or panel brightness.
   * @param[in] brightness  Brightness level [0, 255]; 0 = minimum, 255 = maximum.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : Brightness applied successfully.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed  : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : Display does not expose brightness control.
   */
  [[nodiscard]]
  virtual ReturnCode setBrightness(uint8_t brightness) noexcept = 0;

  /**
   * @brief     Set the logical rotation of the display.
   * @param[in] rotation  Desired @ref DisplayRotation.
   * @details   Affects the coordinate system for all subsequent drawing calls.
   *   @ref getWidth and @ref getHeight reflect the rotated dimensions.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Rotation applied.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode setRotation(DisplayRotation rotation) noexcept = 0;

  // -------------------------------------------------------------------------
  // Geometry
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the display width in pixels (after rotation).
   * @param[out] width  Display width in pixels.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p width is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode getWidth(uint16_t& width) const noexcept = 0;

  /**
   * @brief      Read the display height in pixels (after rotation).
   * @param[out] height  Display height in pixels.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p height is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode getHeight(uint16_t& height) const noexcept = 0;

  // -------------------------------------------------------------------------
  // Drawing
  // -------------------------------------------------------------------------

  /**
   * @brief     Fill the entire display (or framebuffer) with a single color.
   * @param[in] color  Fill color in RGB888 format (0x00RRGGBB).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Clear completed.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode clear(uint32_t color) noexcept = 0;

  /**
   * @brief     Draw a single pixel.
   * @param[in] x      Horizontal position in pixels from the left edge.
   * @param[in] y      Vertical position in pixels from the top edge.
   * @param[in] color  Pixel color in RGB888 format (0x00RRGGBB).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Pixel written.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode drawPixel(uint16_t x, uint16_t y, uint32_t color) noexcept = 0;

  /**
   * @brief     Draw a straight line between two points using Bresenham's algorithm.
   * @param[in] x0     Start horizontal position.
   * @param[in] y0     Start vertical position.
   * @param[in] x1     End horizontal position.
   * @param[in] y1     End vertical position.
   * @param[in] color  Line color in RGB888 format (0x00RRGGBB).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Line drawn.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode drawLine(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1,
                               uint32_t color) noexcept = 0;

  /**
   * @brief     Draw the outline of an axis-aligned rectangle.
   * @param[in] x      Left edge in pixels.
   * @param[in] y      Top edge in pixels.
   * @param[in] w      Width in pixels.
   * @param[in] h      Height in pixels.
   * @param[in] color  Outline color in RGB888 format (0x00RRGGBB).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Rectangle outline drawn.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode drawRect(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               uint32_t color) noexcept = 0;

  /**
   * @brief     Draw a filled axis-aligned rectangle.
   * @param[in] x      Left edge in pixels.
   * @param[in] y      Top edge in pixels.
   * @param[in] w      Width in pixels.
   * @param[in] h      Height in pixels.
   * @param[in] color  Fill color in RGB888 format (0x00RRGGBB).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Filled rectangle drawn.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   */
  [[nodiscard]]
  virtual ReturnCode fillRect(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               uint32_t color) noexcept = 0;

  // -------------------------------------------------------------------------
  // Framebuffer flush
  // -------------------------------------------------------------------------

  /**
   * @brief  Push the internal framebuffer to the display hardware (blocking).
   * @details On non-buffered displays (pixel-by-pixel write) this is a no-op
   *   that returns @ref ReturnCode::AnsweredRequest immediately.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Framebuffer transferred successfully.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::ErrorWriteFailed : Transfer to display hardware failed.
   *   - @ref ReturnCode::FunctionBusy     : An async flush is already in progress.
   */
  [[nodiscard]]
  virtual ReturnCode flush() noexcept = 0;

  /**
   * @brief  Begin an asynchronous DMA framebuffer flush.
   * @details Returns immediately. Completion is reported via @ref DisplayCallback
   *   with @ref DisplayEvent::FlushComplete or @ref DisplayEvent::ErrorFlush.
   *   On non-buffered displays this fires @ref DisplayEvent::FlushComplete
   *   immediately (or is a synchronous no-op, implementation-defined).
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest  : Async flush started.
   *   - @ref ReturnCode::NotInitialized   : begin() has not been called.
   *   - @ref ReturnCode::FunctionBusy     : A previous flush has not yet completed.
   *   - @ref ReturnCode::ErrorNotSupported : Implementation does not support async flush.
   */
  [[nodiscard]]
  virtual ReturnCode flushAsync() noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref DisplayEvent.
   *   Pass a default-constructed @ref DisplayCallback to unregister.
   * @note      The callback may be invoked from ISR or DMA-complete context;
   *   it must not block or allocate.
   */
  virtual void setCallback(DisplayCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — display instances are non-copyable singletons. */
  iDisplay& operator=(const iDisplay&) = delete;
  /** @brief Deleted — display instances are non-movable singletons. */
  iDisplay& operator=(iDisplay&&) = delete;

  /**
   * @brief     Internal event handler — called by the DMA or IC ISR dispatch.
   * @param[in] event  The display event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(DisplayEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_IDISPLAY_HPP_
