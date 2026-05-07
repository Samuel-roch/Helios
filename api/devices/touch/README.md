# Touch Controller

> Interface for capacitive touch controllers — multi-touch point tracking and optional gesture recognition.

<!--
  Suggested image: block diagram showing the touch panel, FPC connector,
  touch IC (FT5336/GT911), I2C bus, and the data path to iTouchController.
  Recommended size: 800×280 px.

  ![Touch controller block diagram](../../../docs/img/touch_diagram.png)
-->

---

## Features

- Multi-touch support up to the IC maximum number of simultaneous fingers
- Per-touch-point state tracking: Pressed, Moved, Released
- Optional gesture recognition: swipe (four directions), zoom in/out
- Asynchronous data-ready notification via `TouchCallback`
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_itouch_controller>
```

---

## Types

### `TouchPointState`

| Value | Description |
|---|---|
| `Pressed` | Finger just made contact |
| `Moved` | Finger is in contact and moved |
| `Released` | Finger was lifted |

### `TouchGesture`

| Value | Description |
|---|---|
| `None` | No gesture active |
| `SwipeUp` | Single-finger upward swipe |
| `SwipeDown` | Single-finger downward swipe |
| `SwipeLeft` | Single-finger leftward swipe |
| `SwipeRight` | Single-finger rightward swipe |
| `ZoomIn` | Two-finger pinch-out |
| `ZoomOut` | Two-finger pinch-in |

### `TouchEvent`

| Value | Description |
|---|---|
| `DataReady` | New touch data is available |
| `GestureDetected` | A gesture was recognised |

### `TouchPoint`

| Field | Type | Description |
|---|---|---|
| `x` | `uint16_t` | Horizontal position from left edge in pixels |
| `y` | `uint16_t` | Vertical position from top edge in pixels |
| `id` | `uint8_t` | Touch identifier — stable across Pressed → Moved → Released |
| `state` | `TouchPointState` | Lifecycle state |

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC, load calibration, verify communication |

### Touch data

| Method | Description |
|---|---|
| `getTouchCount(count)` | Number of active touch points |
| `getTouchPoint(index, point)` | Data for a single touch point by index |
| `getGesture(gesture)` | Active gesture (optional — `ErrorNotSupported` if absent) |

---

## Usage examples

### Polling touch points

```cpp
#include <hel_itouch_controller>

touch.begin();

uint8_t count;
touch.getTouchCount(count);
for (uint8_t i = 0U; i < count; ++i)
{
    hel::TouchPoint pt;
    touch.getTouchPoint(i, pt);
    // pt.x, pt.y, pt.id, pt.state
}
```

### Interrupt-driven with callback

```cpp
touch.setCallback(hel::TouchCallback(this, &Ui::onTouchEvent));

void Ui::onTouchEvent(hel::TouchEvent event) noexcept
{
    if (event == hel::TouchEvent::DataReady)
    {
        uint8_t count;
        m_touch.getTouchCount(count);
        if (count > 0U)
        {
            hel::TouchPoint pt;
            m_touch.getTouchPoint(0U, pt);
            m_screen.dispatch(pt);
        }
    }
    else if (event == hel::TouchEvent::GestureDetected)
    {
        hel::TouchGesture g;
        m_touch.getGesture(g);
        if (g == hel::TouchGesture::SwipeLeft) { m_screen.nextPage(); }
    }
}
```

---

## Implementing for a new target

```cpp
class Ft5336 : public hel::iTouchController
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify chip ID register over I2C
    }

    hel::ReturnCode getTouchCount(uint8_t& count) noexcept override
    {
        // read TD_STATUS register (lower 4 bits = touch count)
    }

    hel::ReturnCode getTouchPoint(uint8_t index, hel::TouchPoint& point) noexcept override
    {
        // read TOUCH_DATA registers for the given index
        // parse XH/XL, YH/YL, touch ID, event flag
    }

    hel::ReturnCode getGesture(hel::TouchGesture&) noexcept override
    {
        return hel::ReturnCode::ErrorNotSupported;  // FT5336 basic mode has no gesture engine
    }

    // ... remaining methods

protected:
    void handleEvent(hel::TouchEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::TouchCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not disable interrupts for extended periods.
- Coordinates are in the display pixel space at the time of the event; rotation remapping is the caller's responsibility.
