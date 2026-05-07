# iWdg — Watchdog Timer Interface

`hel::iWdg` is the hardware-agnostic interface for an independent hardware watchdog. Once started, it must be refreshed within every configured timeout window or the MCU resets.

## API Summary

| Method | Description |
|---|---|
| `start(timeout_ms)` | Initialize and start the watchdog — irreversible on most targets |
| `refresh()` | Kick the watchdog to reset the countdown |

> **Warning**: On most MCUs (STM32 IWDG, ESP32) the watchdog cannot be stopped once started. If `start` is called, `refresh` must be called reliably from that point on.

---

## Implementing a Concrete Driver

### 1. Inherit from `iWdg`

```cpp
#include <hel_iwdg>

class Stm32Iwdg final : public hel::iWdg
{
public:
    explicit Stm32Iwdg(IWDG_HandleTypeDef& hiwdg) noexcept : m_hiwdg(hiwdg) {}

    hel::ReturnCode start(uint32_t timeout_ms) noexcept override;
    hel::ReturnCode refresh() noexcept override;

private:
    IWDG_HandleTypeDef& m_hiwdg;
    bool                m_started{false};
};
```

---

### 2. Implement `start`

```cpp
hel::ReturnCode Stm32Iwdg::start(uint32_t timeout_ms) noexcept
{
    if (m_started)
        return hel::ReturnCode::OperationRunning;

    if ((timeout_ms == 0U) || (timeout_ms > 32768U)) // IWDG max ~32.7 s on STM32
        return hel::ReturnCode::ErrorParam;

    // STM32 IWDG: LSI ~32 kHz, prescaler and reload derived from timeout
    // CubeMX calculates these at generation time; here we set them manually
    m_hiwdg.Init.Prescaler = IWDG_PRESCALER_64;          // 32000 / 64 = 500 Hz
    m_hiwdg.Init.Reload    = (timeout_ms * 500U / 1000U); // ticks = ms * 0.5
    m_hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;

    if (HAL_IWDG_Init(&m_hiwdg) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    m_started = true;
    return hel::ReturnCode::AnsweredRequest;
}
```

> The reload value formula depends on the LSI frequency and prescaler chosen. Verify against the actual LSI frequency of your target (may vary ±20 %).

---

### 3. Implement `refresh`

```cpp
hel::ReturnCode Stm32Iwdg::refresh() noexcept
{
    if (!m_started)
        return hel::ReturnCode::NotInitialized;

    return (HAL_IWDG_Refresh(&m_hiwdg) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}
```

`refresh` is IRQ-safe and may be called from any context (task or ISR).

---

## Usage Example

```cpp
Stm32Iwdg wdg(hiwdg);

// Start watchdog with 1-second timeout during init
wdg.start(1000U);

// In the main task loop (must run faster than 1000 ms)
while (true)
{
    wdg.refresh();
    doWork();
}
```

### RTOS pattern

When using an RTOS, create a dedicated watchdog task at the highest priority that kicks the WDG and monitors subordinate tasks via a flag or semaphore:

```cpp
void watchdogTask(void*)
{
    while (true)
    {
        wdg.refresh();
        vTaskDelay(pdMS_TO_TICKS(500U)); // kick at half the timeout period
    }
}
```

---

## Checklist for New Implementations

- [ ] `start` returns `OperationRunning` (not `ErrorGeneral`) if already started
- [ ] `start` returns `ErrorParam` for `timeout_ms == 0` or values exceeding the supported range
- [ ] `refresh` returns `NotInitialized` if `start` was never called
- [ ] `refresh` is safe to call from ISR context
- [ ] LSI frequency variation (±10–20 % on STM32) is accounted for — use a shorter effective timeout
- [ ] All public methods are `noexcept`
