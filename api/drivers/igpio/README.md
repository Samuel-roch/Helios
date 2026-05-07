# iGpio — GPIO Interface

`hel::iGpio` is the hardware-agnostic interface for a single general-purpose digital I/O pin. Each instance represents one pin and is owned by the BSP.

## API Summary

| Method | Description |
|---|---|
| `read(state)` | Read the current logical level of the pin |
| `write(state)` | Drive the pin high (`true`) or low (`false`) |
| `toggle()` | Invert the current output level |
| `enableInterrupt(mode, callback)` | Arm the EXTI interrupt with a trigger mode and callback |

The `GpioCallback` signature is `void handler(GpioIrqMode cause) noexcept` and fires from ISR context.

---

## Implementing a Concrete Driver

### 1. Inherit from `iGpio`

```cpp
#include <hel_igpio>

class Stm32Gpio final : public hel::iGpio
{
public:
    Stm32Gpio(GPIO_TypeDef* port, uint16_t pin) noexcept
        : m_port(port), m_pin(pin) {}

    hel::ReturnCode read(bool& state) const noexcept override;
    hel::ReturnCode write(bool state) noexcept override;
    hel::ReturnCode toggle() noexcept override;
    void enableInterrupt(hel::GpioIrqMode mode,
                         hel::GpioCallback callback) noexcept override;

protected:
    void handleIrq(hel::GpioIrqMode cause) noexcept override;

private:
    GPIO_TypeDef*     m_port;
    uint16_t          m_pin;
    hel::GpioCallback m_callback{};
};
```

> **Singleton rule**: GPIO pin objects are owned by the BSP. The deleted `operator=` prevents copying or moving at compile time.

---

### 2. Implement `read`, `write`, `toggle`

```cpp
hel::ReturnCode Stm32Gpio::read(bool& state) const noexcept
{
    state = (HAL_GPIO_ReadPin(m_port, m_pin) == GPIO_PIN_SET);
    return hel::ReturnCode::AnsweredRequest;
}

hel::ReturnCode Stm32Gpio::write(bool state) noexcept
{
    HAL_GPIO_WritePin(m_port, m_pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return hel::ReturnCode::AnsweredRequest;
}

hel::ReturnCode Stm32Gpio::toggle() noexcept
{
    HAL_GPIO_TogglePin(m_port, m_pin);
    return hel::ReturnCode::AnsweredRequest;
}
```

---

### 3. Implement `enableInterrupt`

The NVIC must already be enabled by the BSP (CubeMX or manual config) before calling this method.

```cpp
void Stm32Gpio::enableInterrupt(hel::GpioIrqMode mode,
                                 hel::GpioCallback callback) noexcept
{
    m_callback = callback;
    // Passing GpioIrqMode::Disabled clears events without touching the callback.
    // Dynamic EXTI reconfiguration at runtime is target-specific;
    // on most STM32 families the trigger mode is fixed at init time by CubeMX.
    (void)mode;
}
```

---

### 4. Implement `handleIrq` and wire the ISR

```cpp
void Stm32Gpio::handleIrq(hel::GpioIrqMode cause) noexcept
{
    m_callback(cause);
}

// In your BSP (stm32xx_it.cpp):
extern Stm32Gpio g_button;

// F4 — single callback, edge indistinguishable:
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_13)
        g_button.handleIrq(hel::GpioIrqMode::Change);
}

// H7 / L4 — separate rising / falling callbacks:
void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_13)
        g_button.handleIrq(hel::GpioIrqMode::Rising);
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin)
{
    if (pin == GPIO_PIN_13)
        g_button.handleIrq(hel::GpioIrqMode::Falling);
}
```

> `handleIrq` is `protected` — only ISR dispatch functions should call it.

---

## Usage Example

```cpp
Stm32Gpio led(GPIOA, GPIO_PIN_5);
Stm32Gpio button(GPIOC, GPIO_PIN_13);

led.write(true);   // LED on
led.toggle();      // LED off

bool pressed{};
button.read(pressed);

// Arm falling-edge interrupt
button.enableInterrupt(hel::GpioIrqMode::Falling,
    hel::GpioCallback(&handler, &MyClass::onPress));
```

---

## Checklist for New Implementations

- [ ] `read` returns `NotInitialized` if the pin was never initialized at BSP level
- [ ] `write` and `toggle` have no effect on input-configured pins, or return `ErrorGeneral`
- [ ] `handleIrq` is called only from ISR dispatch, never from the public API
- [ ] Callback receives the actual edge that fired, or `Change` when indistinguishable (e.g. F4)
- [ ] `enableInterrupt` with `GpioIrqMode::Disabled` stops events without clearing the stored callback
- [ ] All public methods are `noexcept`
