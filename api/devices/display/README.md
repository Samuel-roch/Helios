# Display

> Interface for display devices — drawing primitives, rotation, brightness, and framebuffer flush.

<!--
  Suggested image: block diagram showing the application → iDisplay interface →
  framebuffer (optional) → DMA/SPI → panel IC → LCD/OLED.
  Recommended size: 900×320 px.

  ![Display block diagram](../../../docs/img/display_diagram.png)
-->

---

## Features

- Drawing primitives: pixel, line, rectangle (outline and filled), full-screen clear
- Display control: power on/off, brightness, rotation
- Blocking and asynchronous DMA framebuffer flush
- Rotation-aware geometry — `getWidth`/`getHeight` reflect the current rotation
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_idisplay>
```

---

## Types

### `DisplayRotation`

| Value | Description |
|---|---|
| `Portrait0` | 0° — default hardware orientation |
| `Landscape90` | 90° clockwise |
| `Portrait180` | 180° |
| `Landscape270` | 270° clockwise |

### `DisplayEvent`

| Value | Description |
|---|---|
| `FlushComplete` | Asynchronous flush finished successfully |
| `ErrorFlush` | Asynchronous flush failed |

### Color format

All color arguments are `uint32_t` in **RGB888** format: `0x00RRGGBB`.

| Example | Color |
|---|---|
| `0x00FF0000` | Red |
| `0x0000FF00` | Green |
| `0x000000FF` | Blue |
| `0x00FFFFFF` | White |
| `0x00000000` | Black |

The implementation converts to the panel's native format (RGB565, monochrome, etc.).

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC, send init sequence, turn display on |
| `setOn(on)` | Power display on or off |
| `setBrightness(brightness)` | Backlight level [0, 255] (optional) |
| `setRotation(rotation)` | Logical rotation; updates width/height |

### Geometry

| Method | Description |
|---|---|
| `getWidth(width)` | Display width in pixels (rotation-aware) |
| `getHeight(height)` | Display height in pixels (rotation-aware) |

### Drawing

| Method | Description |
|---|---|
| `clear(color)` | Fill entire display with one color |
| `drawPixel(x, y, color)` | Draw a single pixel |
| `drawLine(x0, y0, x1, y1, color)` | Draw a line (Bresenham) |
| `drawRect(x, y, w, h, color)` | Draw a hollow rectangle |
| `fillRect(x, y, w, h, color)` | Draw a filled rectangle |

Out-of-bounds coordinates are clipped silently; no error is returned.

### Flush

| Method | Description |
|---|---|
| `flush()` | Blocking: push framebuffer to hardware |
| `flushAsync()` | Non-blocking: start DMA transfer, fires `FlushComplete` callback |

On non-buffered displays both methods return `AnsweredRequest` immediately.

---

## Usage examples

### Initializing and drawing

```cpp
#include <hel_idisplay>

display.begin();
display.setRotation(hel::DisplayRotation::Landscape90);
display.setBrightness(200U);

uint16_t w, h;
display.getWidth(w);
display.getHeight(h);

display.clear(0x00000000U);                    // black background
display.fillRect(10U, 10U, 100U, 50U, 0x00FF0000U);  // red box
display.drawRect(10U, 10U, 100U, 50U, 0x00FFFFFFU);  // white outline
display.flush();
```

### Double-buffered rendering with async flush

```cpp
display.setCallback(hel::DisplayCallback(this, &Renderer::onDisplayEvent));

// render frame into framebuffer...
display.fillRect(0U, 0U, width, height, 0x00000000U);
drawScene();

// kick off DMA transfer
display.flushAsync();

void Renderer::onDisplayEvent(hel::DisplayEvent event) noexcept
{
    if (event == hel::DisplayEvent::FlushComplete)
    {
        // safe to start rendering the next frame
        m_frameReady = true;
    }
}
```

---

## Implementing for a new target

```cpp
class Ili9341 : public hel::iDisplay
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // send ILI9341 initialization command sequence over SPI
    }

    hel::ReturnCode fillRect(uint16_t x, uint16_t y,
                              uint16_t w, uint16_t h,
                              uint32_t color) noexcept override
    {
        // set column/row address window, then bulk-write pixels
        // convert RGB888 → RGB565: r5 g6 b5
    }

    hel::ReturnCode flushAsync() noexcept override
    {
        // start DMA transfer of the internal framebuffer over SPI
        // call handleEvent(DisplayEvent::FlushComplete) from DMA ISR
    }

    // ... remaining methods

protected:
    void handleEvent(hel::DisplayEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::DisplayCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- Blocking methods (`flush`, drawing primitives) must **not** be called while an async flush is in progress.
- `handleEvent` is intended for DMA-complete or ISR context only — `noexcept`, must not block.
- Callbacks fire from ISR/DMA context; they must not disable interrupts for extended periods.
