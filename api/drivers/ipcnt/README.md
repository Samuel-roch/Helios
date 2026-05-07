# iPcnt — Pulse Counter Interface

`hel::iPcnt` is the hardware-agnostic interface for a hardware pulse counter. It wraps a timer peripheral configured in encoder or external-clock mode.

## API Summary

| Method | Description |
|---|---|
| `start()` | Start counting pulses |
| `stop()` | Stop counting; count value is preserved |
| `reset()` | Reset the counter to zero (safe while running) |
| `read(count)` | Read the current counter value |
| `setThreshold(threshold)` | Set the count that triggers the callback (0 = disabled) |
| `setCallback(cb)` | Register the threshold/overflow event callback |

The `PcntCallback` signature is `void handler(ReturnCode code, uint32_t count) noexcept`.

---

## Implementing a Concrete Driver

### 1. Inherit from `iPcnt`

```cpp
#include <hel_ipcnt>

class Stm32Pcnt final : public hel::iPcnt
{
public:
    explicit Stm32Pcnt(TIM_HandleTypeDef& htim) noexcept : m_htim(htim) {}

    hel::ReturnCode start() noexcept override;
    hel::ReturnCode stop() noexcept override;
    hel::ReturnCode reset() noexcept override;
    hel::ReturnCode read(uint32_t& count) noexcept override;
    hel::ReturnCode setThreshold(uint32_t threshold) noexcept override;
    void setCallback(hel::PcntCallback callback) noexcept override;

protected:
    void handleEvent(hel::ReturnCode code, uint32_t count) noexcept override;

private:
    TIM_HandleTypeDef& m_htim;
    hel::PcntCallback  m_callback{};
    uint32_t           m_threshold{0U};
};
```

---

### 2. Implement control methods

```cpp
hel::ReturnCode Stm32Pcnt::start() noexcept
{
    if (HAL_TIM_GetState(&m_htim) == HAL_TIM_STATE_BUSY)
        return hel::ReturnCode::OperationRunning;

    return (HAL_TIM_Base_Start_IT(&m_htim) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}

hel::ReturnCode Stm32Pcnt::stop() noexcept
{
    if (HAL_TIM_GetState(&m_htim) != HAL_TIM_STATE_BUSY)
        return hel::ReturnCode::ErrorInvalidState;

    return (HAL_TIM_Base_Stop_IT(&m_htim) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}

hel::ReturnCode Stm32Pcnt::reset() noexcept
{
    __HAL_TIM_SET_COUNTER(&m_htim, 0U);
    return hel::ReturnCode::AnsweredRequest;
}

hel::ReturnCode Stm32Pcnt::read(uint32_t& count) noexcept
{
    count = __HAL_TIM_GET_COUNTER(&m_htim);
    return hel::ReturnCode::AnsweredRequest;
}
```

---

### 3. Implement `setThreshold`

Configure the timer's auto-reload register (ARR) to trigger an update event at the threshold value.

```cpp
hel::ReturnCode Stm32Pcnt::setThreshold(uint32_t threshold) noexcept
{
    m_threshold = threshold;
    if (threshold == 0U)
    {
        // Disable update interrupt
        __HAL_TIM_DISABLE_IT(&m_htim, TIM_IT_UPDATE);
    }
    else
    {
        __HAL_TIM_SET_AUTORELOAD(&m_htim, threshold - 1U);
        __HAL_TIM_ENABLE_IT(&m_htim, TIM_IT_UPDATE);
    }
    return hel::ReturnCode::AnsweredRequest;
}
```

---

### 4. Implement `setCallback` and `handleEvent`, wire ISR

```cpp
void Stm32Pcnt::setCallback(hel::PcntCallback callback) noexcept
{
    m_callback = callback;
}

void Stm32Pcnt::handleEvent(hel::ReturnCode code, uint32_t count) noexcept
{
    m_callback(code, count);
}

// BSP (stm32xx_it.cpp):
extern Stm32Pcnt g_encoder;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM2)
    {
        const uint32_t cnt = __HAL_TIM_GET_COUNTER(htim);
        g_encoder.handleEvent(hel::ReturnCode::AnsweredRequest, cnt);
    }
}
```

---

## Usage Example

```cpp
Stm32Pcnt encoder(htim2);

encoder.setCallback(hel::PcntCallback(&handler, &MyClass::onPulse));
encoder.setThreshold(1000U); // fire every 1000 pulses
encoder.start();

// Poll count in task
uint32_t pulses{};
encoder.read(pulses);

// Reset without stopping
encoder.reset();
```

---

## Checklist for New Implementations

- [ ] `start` returns `OperationRunning` (not `ErrorGeneral`) when already running
- [ ] `stop` returns `ErrorInvalidState` when not running
- [ ] `reset` is atomic and safe to call while counting
- [ ] `setThreshold(0)` disables threshold events
- [ ] `handleEvent` is called only from ISR dispatch
- [ ] Overflow events are reported with `ReturnCode::AnsweredRequest` (not as errors)
- [ ] All public methods are `noexcept`
