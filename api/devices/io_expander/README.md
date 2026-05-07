# IO Expander

> Interface for GPIO expander ICs — per-pin direction, atomic port I/O, and interrupt-on-change.

<!--
  Suggested image: block diagram showing MCU I2C bus → MCP23017/PCF8574 →
  16 GPIO pins connected to LEDs, buttons, and other peripherals.
  Recommended size: 900×300 px.

  ![IO expander block diagram](../../../docs/img/io_expander_diagram.png)
-->

---

## Features

- Per-pin mode: `Input`, `InputPullUp`, `Output`
- Individual pin read/write
- Atomic port-level read/write using a `uint16_t` bitmask (up to 16 pins)
- Per-pin interrupt-on-change with callback notification
- 8-pin expanders use indices [0, 7]; 16-pin expanders use [0, 15]
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_iio_expander>
```

---

## Types

### `IoExpanderPinMode`

| Value | Description |
|---|---|
| `Input` | High-impedance digital input |
| `InputPullUp` | Digital input with internal pull-up |
| `Output` | Push-pull digital output |

### `IoExpanderEvent`

| Value | Description |
|---|---|
| `InputChanged` | One or more enabled input pins changed state |

### Port bitmask convention

Bit N in the `uint16_t` port mask corresponds to pin N:

| Bit | Pin |
|---|---|
| 0 | Pin 0 |
| 1 | Pin 1 |
| … | … |
| 15 | Pin 15 |

For 8-pin ICs the upper byte of the mask is ignored on write and reads as `0x00`.

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC, configure all pins as inputs, verify communication |

### Pin configuration

| Method | Description |
|---|---|
| `setPinMode(pin, mode)` | Set direction and pull for a single pin |
| `getPinMode(pin, mode)` | Read the current mode of a single pin |

### Pin-level I/O

| Method | Description |
|---|---|
| `writePin(pin, value)` | Write a logic level to a single output pin |
| `readPin(pin, value)` | Read the logic level of a single pin |

### Port-level I/O

| Method | Description |
|---|---|
| `writePort(value)` | Write all output pins atomically |
| `readPort(value)` | Read all pin levels atomically |

### Interrupt on change

| Method | Description |
|---|---|
| `setInterruptEnabled(pin, enabled)` | Enable/disable interrupt for a pin (optional) |

Returns `ErrorNotSupported` on ICs without interrupt capability.

---

## Usage examples

### Configuring and using output pins

```cpp
#include <hel_iio_expander>

expander.begin();
expander.setPinMode(0U, hel::IoExpanderPinMode::Output);
expander.setPinMode(1U, hel::IoExpanderPinMode::Output);

expander.writePin(0U, true);   // pin 0 high
expander.writePin(1U, false);  // pin 1 low

// Atomic: set pin 0 high, pin 1 high, all others low
expander.writePort(0x0003U);
```

### Reading a button bank

```cpp
expander.setPinMode(4U, hel::IoExpanderPinMode::InputPullUp);
expander.setPinMode(5U, hel::IoExpanderPinMode::InputPullUp);

bool btn4;
expander.readPin(4U, btn4);  // false = pressed (active-low with pull-up)

// Read all pins at once
uint16_t port;
expander.readPort(port);
const bool btn5 = ((port & (1U << 5U)) == 0U);  // active-low
```

### Interrupt-on-change

```cpp
expander.setPinMode(8U, hel::IoExpanderPinMode::InputPullUp);
expander.setInterruptEnabled(8U, true);
expander.setCallback(hel::IoExpanderCallback(this, &Panel::onExpanderEvent));

void Panel::onExpanderEvent(hel::IoExpanderEvent event) noexcept
{
    if (event == hel::IoExpanderEvent::InputChanged)
    {
        uint16_t port;
        m_expander.readPort(port);
        // check which bits changed by comparing with a cached previous state
    }
}
```

---

## Implementing for a new target

```cpp
class Mcp23017 : public hel::iIoExpander
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // write IODIR registers: all inputs (0xFF) over I2C
    }

    hel::ReturnCode writePort(uint16_t value) noexcept override
    {
        // write OLAT registers (OLATA = low byte, OLATB = high byte)
    }

    hel::ReturnCode readPort(uint16_t& value) noexcept override
    {
        // read GPIO registers (GPIOA = low byte, GPIOB = high byte)
    }

    hel::ReturnCode setInterruptEnabled(uint8_t pin, bool enabled) noexcept override
    {
        // set/clear the bit in GPINTEN register for the given pin
    }

    // ... remaining methods

protected:
    void handleEvent(hel::IoExpanderEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::IoExpanderCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` is intended for ISR context only — `noexcept`, must not block.
- Callbacks fire from ISR context; they must not disable interrupts for extended periods.
- `readPort` inside a callback is safe because the INT line remains asserted until the port is read (on most ICs); reading clears the interrupt.
