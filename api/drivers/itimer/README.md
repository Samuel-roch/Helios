# iTimer — Hardware Timer Interface

`hel::iTimer` is the hardware-agnostic interface for hardware timers. It supports microsecond-resolution one-shot and periodic timing with an ISR-fired callback.

## API Summary

| Method | Description |
|---|---|
| `start(period_us, mode)` | Start (or restart) the timer with the given period and mode |
| `stop()` | Freeze the counter immediately; no callback fires |
| `reset()` | Reset the counter to zero without stopping |
| `isActive()` | Returns `true` while counting |
| `count()` | Returns elapsed microseconds since last `start` or `reset` |
| `setCallback(cb)` | Register the period-elapsed callback |

### Timer Modes

| `TimerMode` | Behaviour |
|---|---|
| `OneShot` | Counter stops after the first expiry; callback fires once |
| `Periodic` | Counter reloads automatically; callback fires every period |

The `TimerCallback` signature is `void handler() noexcept` and fires from ISR or driver-task context.

---

## Implementing a Concrete Driver

### 1. Inherit from `iTimer`

```cpp
#include <hel_itimer>

class Stm32Timer final : public hel::iTimer
{
public:
    explicit Stm32Timer(TIM_HandleTypeDef& htim) noexcept : m_htim(htim) {}

    hel::ReturnCode start(uint32_t period_us,
                          hel::TimerMode mode) noexcept override;
    hel::ReturnCode stop() noexcept override;
    hel::ReturnCode reset() noexcept override;
    bool            isActive() const noexcept override;
    uint64_t        count() noexcept override;
    void            setCallback(hel::TimerCallback callback) noexcept override;
    void            handleEvent() noexcept override;

private:
    TIM_HandleTypeDef& m_htim;
    hel::TimerCallback m_callback{};
    hel::TimerMode     m_mode{hel::TimerMode::OneShot};
    bool               m_active{false};
};
```

---

### 2. Implement `start`

Calculate PSC and ARR from the timer input clock to achieve the requested period.

```cpp
hel::ReturnCode Stm32Timer::start(uint32_t period_us,
                                   hel::TimerMode mode) noexcept
{
    if (period_us == 0U)
        return hel::ReturnCode::ErrorParam;

    // Derive period register: timer_clk / 1e6 ticks per microsecond
    const uint32_t timer_clk = HAL_RCC_GetPCLK1Freq() * 2U; // adjust per APB
    const uint32_t ticks     = (timer_clk / 1'000'000U) * period_us;

    // Fit into 16-bit ARR with prescaler
    const uint32_t psc = (ticks / 0xFFFFU);
    const uint32_t arr = (ticks / (psc + 1U)) - 1U;

    m_htim.Init.Prescaler   = psc;
    m_htim.Init.Period      = arr;
    m_htim.Init.CounterMode = TIM_COUNTERMODE_UP;

    if (HAL_TIM_Base_Init(&m_htim) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    m_mode   = mode;
    m_active = true;

    return (HAL_TIM_Base_Start_IT(&m_htim) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}
```

---

### 3. Implement `stop`, `reset`, `isActive`, `count`

```cpp
hel::ReturnCode Stm32Timer::stop() noexcept
{
    if (!m_active)
        return hel::ReturnCode::NotInitialized;

    HAL_TIM_Base_Stop_IT(&m_htim);
    m_active = false;
    return hel::ReturnCode::AnsweredRequest;
}

hel::ReturnCode Stm32Timer::reset() noexcept
{
    if (!m_active)
        return hel::ReturnCode::NotInitialized;

    __HAL_TIM_SET_COUNTER(&m_htim, 0U);
    return hel::ReturnCode::AnsweredRequest;
}

bool Stm32Timer::isActive() const noexcept { return m_active; }

uint64_t Stm32Timer::count() noexcept
{
    if (!m_active)
        return 0U;

    // Convert counter ticks back to microseconds
    const uint32_t timer_clk = HAL_RCC_GetPCLK1Freq() * 2U;
    const uint32_t ticks_per_us = timer_clk / 1'000'000U;
    const uint32_t cnt = __HAL_TIM_GET_COUNTER(&m_htim);
    const uint32_t psc = __HAL_TIM_GET_PRESCALER(&m_htim) + 1U;
    return static_cast<uint64_t>(cnt) * psc / ticks_per_us;
}
```

---

### 4. Wire the ISR callback

```cpp
void Stm32Timer::setCallback(hel::TimerCallback cb) noexcept { m_callback = cb; }

void Stm32Timer::handleEvent() noexcept
{
    if (m_mode == hel::TimerMode::OneShot)
    {
        HAL_TIM_Base_Stop_IT(&m_htim);
        m_active = false;
    }
    m_callback();
}

// BSP:
extern Stm32Timer g_timer;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM6)
        g_timer.handleEvent();
}
```

---

## Usage Example

```cpp
Stm32Timer timer(htim6);

// One-shot: fire once after 500 ms
timer.setCallback(hel::TimerCallback(&handler, &MyClass::onTimeout));
timer.start(500'000U, hel::TimerMode::OneShot);

// Check elapsed without stopping
uint64_t us = timer.count();

// Periodic: fire every 10 ms
timer.start(10'000U, hel::TimerMode::Periodic);

// Stop at any time
timer.stop();
```

---

## Checklist for New Implementations

- [ ] `start` restarts the timer if it is already running (no need to `stop` first)
- [ ] `stop` and `reset` return `NotInitialized` when the timer is not active
- [ ] `handleEvent` marks the timer inactive and stops the peripheral for `OneShot` mode
- [ ] `handleEvent` does **not** stop the peripheral for `Periodic` mode
- [ ] `count()` returns 0 when `isActive()` is false
- [ ] `handleEvent` is called only from ISR dispatch, never from the public API
- [ ] All public methods are `noexcept`
