/**
 ******************************************************************************
 * @file    itouch_controller.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-05-07
 * @ingroup HELIOS_DEV_TOUCH
 * @brief   Capacitive touch controller device interface.
 *
 * @details
 *   - Multi-touch support: up to the IC maximum number of simultaneous fingers
 *   - Per-touch-point state: Pressed, Moved, Released
 *   - Optional gesture detection (swipe directions, zoom)
 *   - Data-ready notification via @ref TouchCallback
 *   - No dynamic allocation; implementations map each method to the underlying IC
 *
 * @note
 *   Coordinates are in pixels relative to the top-left corner of the display.
 *   The orientation must match the display rotation; the application is
 *   responsible for coordinate remapping if rotations differ.
 *   Methods are not thread-safe on the same instance; caller must serialize
 *   concurrent access.
 */

#ifndef HELIOS_DEV_ITOUCH_CONTROLLER_HPP_
#define HELIOS_DEV_ITOUCH_CONTROLLER_HPP_

#include <hel_callback>
#include <hel_return_code>
#include <cstdint>

namespace hel
{

// @formatter:off

// =============================================================================
// TouchPointState
// =============================================================================

/**
 * @enum  TouchPointState
 * @brief Lifecycle state of a single touch point.
 * @ingroup HELIOS_DEV_TOUCH
 */
enum class TouchPointState : uint8_t
{
  Pressed  = 0x00U, /*!< Finger just made contact with the surface.    */
  Moved    = 0x01U, /*!< Finger is in contact and has moved.           */
  Released = 0x02U  /*!< Finger was lifted from the surface.           */
};

// =============================================================================
// TouchGesture
// =============================================================================

/**
 * @enum  TouchGesture
 * @brief Gesture recognised by the touch controller IC.
 * @ingroup HELIOS_DEV_TOUCH
 * @details Not all gestures are supported by every IC; unsupported gestures are
 *   never generated.  @ref None is returned when no gesture is active.
 */
enum class TouchGesture : uint8_t
{
  None        = 0x00U, /*!< No gesture detected.                        */
  SwipeUp     = 0x01U, /*!< Single-finger swipe towards the top edge.   */
  SwipeDown   = 0x02U, /*!< Single-finger swipe towards the bottom edge. */
  SwipeLeft   = 0x03U, /*!< Single-finger swipe towards the left edge.  */
  SwipeRight  = 0x04U, /*!< Single-finger swipe towards the right edge. */
  ZoomIn      = 0x05U, /*!< Two-finger pinch-out (zoom in).             */
  ZoomOut     = 0x06U  /*!< Two-finger pinch-in (zoom out).             */
};

// =============================================================================
// TouchEvent
// =============================================================================

/**
 * @enum  TouchEvent
 * @brief Events reported via the registered @ref TouchCallback.
 * @ingroup HELIOS_DEV_TOUCH
 */
enum class TouchEvent : uint8_t
{
  DataReady        = 0x00U, /*!< New touch data is available; call @ref iTouchController::getTouchPoint. */
  GestureDetected  = 0x01U  /*!< A gesture was recognised; call @ref iTouchController::getGesture.      */
};

// @formatter:on

// =============================================================================
// TouchPoint
// =============================================================================

/**
 * @struct TouchPoint
 * @brief  Data for a single active touch point.
 * @ingroup HELIOS_DEV_TOUCH
 */
struct TouchPoint
{
  uint16_t       x;      /*!< Horizontal position in pixels from the left edge.   */
  uint16_t       y;      /*!< Vertical position in pixels from the top edge.      */
  uint8_t        id;     /*!< Touch identifier; stable across Pressed→Moved→Released. */
  TouchPointState state; /*!< Lifecycle state of this touch point.                */
};

// =============================================================================
// TouchCallback
// =============================================================================

/**
 * @brief Callable type invoked when a touch event occurs.
 *
 * @details Signature: `void handler(TouchEvent event) noexcept`
 *
 * @note  The callback may fire from ISR context; it must not block or allocate.
 */
using TouchCallback = Callback<void(TouchEvent)>;

// =============================================================================
// iTouchController
// =============================================================================

/**
 * @class  iTouchController
 * @brief  Hardware-agnostic capacitive touch controller interface.
 * @ingroup HELIOS_DEV_TOUCH
 *
 * @details
 *   - Reads the number of active touch points via @ref getTouchCount.
 *   - Reads individual touch point data via @ref getTouchPoint(index, point).
 *   - Optional gesture recognition via @ref getGesture; returns
 *     @ref ReturnCode::ErrorNotSupported on controllers without a gesture engine.
 *   - Asynchronous data-ready and gesture events delivered via @ref TouchCallback.
 *   - No dynamic allocation; implementations map each method to the underlying IC.
 *
 * @note
 *   Thread-safety: individual methods are **not** thread-safe on the same instance;
 *   caller must serialize concurrent access.  Reentrant across distinct instances.
 *   Copy and move are deleted; touch controller instances are singletons owned
 *   by the BSP layer.
 */
class iTouchController
{
public:
  virtual ~iTouchController() noexcept = default;

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief  Initialize the touch controller and verify communication.
   * @details Configures the IC, loads calibration data if available, and
   *   confirms the device is responsive.  Must be called before any other method.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest     : Initialization successful.
   *   - @ref ReturnCode::ErrorDeviceNotFound : No device responded at the expected address.
   *   - @ref ReturnCode::ErrorReadFailed     : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode begin() noexcept = 0;

  // -------------------------------------------------------------------------
  // Touch data
  // -------------------------------------------------------------------------

  /**
   * @brief      Read the number of currently active touch points.
   * @param[out] count  Number of fingers in contact with the surface [0, max].
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p count is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTouchCount(uint8_t& count) noexcept = 0;

  /**
   * @brief      Read the data for a single touch point by index.
   * @param[in]  index  Touch point index [0, count − 1] as returned by
   *   @ref getTouchCount.
   * @param[out] point  Populated with the position, ID and state of the touch
   *   point on success.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : @p point is valid.
   *   - @ref ReturnCode::NotInitialized  : begin() has not been called.
   *   - @ref ReturnCode::ErrorParam      : @p index is out of range.
   *   - @ref ReturnCode::ErrorReadFailed : Communication with the IC failed.
   */
  [[nodiscard]]
  virtual ReturnCode getTouchPoint(uint8_t index, TouchPoint& point) noexcept = 0;

  /**
   * @brief      Read the gesture recognised by the touch controller.
   * @param[out] gesture  Set to the active @ref TouchGesture; set to
   *   @ref TouchGesture::None when no gesture is active.
   * @return     ReturnCode
   *   - @ref ReturnCode::AnsweredRequest   : @p gesture is valid.
   *   - @ref ReturnCode::NotInitialized    : begin() has not been called.
   *   - @ref ReturnCode::ErrorReadFailed   : Communication with the IC failed.
   *   - @ref ReturnCode::ErrorNotSupported : IC does not have a gesture engine.
   */
  [[nodiscard]]
  virtual ReturnCode getGesture(TouchGesture& gesture) noexcept = 0;

  // -------------------------------------------------------------------------
  // Event notification
  // -------------------------------------------------------------------------

  /**
   * @brief     Register or replace the asynchronous event callback.
   * @param[in] callback  Callable invoked on each @ref TouchEvent.
   *   Pass a default-constructed @ref TouchCallback to unregister.
   * @note      The callback may be invoked from ISR context; it must not block
   *   or allocate.
   */
  virtual void setCallback(TouchCallback callback) noexcept = 0;

protected:
  /** @brief Deleted — touch controller instances are non-copyable singletons. */
  iTouchController& operator=(const iTouchController&) = delete;
  /** @brief Deleted — touch controller instances are non-movable singletons. */
  iTouchController& operator=(iTouchController&&) = delete;

  /**
   * @brief     Internal event handler — called by the driver's ISR dispatch.
   * @param[in] event  The touch event that occurred.
   * @warning   Must only be called from the concrete implementation or ISR context.
   *            Not part of the public API.
   */
  virtual void handleEvent(TouchEvent event) noexcept = 0;
};

} // namespace hel

#endif // HELIOS_DEV_ITOUCH_CONTROLLER_HPP_
