# iPwm — PWM Output Interface

`hel::iPwm` is the hardware-agnostic interface for PWM generation. Each instance wraps one hardware timer and exposes all its channels. Frequency is shared by all channels of the same timer.

## API Summary

| Method | Description |
|---|---|
| `start(channel)` | Start PWM on the specified channel |
| `stop(channel)` | Stop PWM on the specified channel |
| `setDutyCycle(channel, duty_permille)` | Set duty cycle in permille (0–1000) |
| `setFrequency(frequency_hz)` | Set output frequency in Hz (all channels affected) |

**Duty cycle unit**: permille (‰) — 0 = 0 %, 500 = 50 %, 1000 = 100 %. This gives 0.1 % resolution without floating-point.

---

## Implementing a Concrete Driver

### 1. Inherit from `iPwm`

```cpp
#include <hel_ipwm>

class Stm32Pwm final : public hel::iPwm
{
public:
    explicit Stm32Pwm(TIM_HandleTypeDef& htim) noexcept : m_htim(htim) {}

    hel::ReturnCode start(uint32_t channel) noexcept override;
    hel::ReturnCode stop(uint32_t channel) noexcept override;
    hel::ReturnCode setDutyCycle(uint32_t channel,
                                  uint16_t duty_permille) noexcept override;
    hel::ReturnCode setFrequency(uint32_t frequency_hz) noexcept override;

private:
    TIM_HandleTypeDef& m_htim;

    // Maps zero-based channel index to HAL channel constant
    static uint32_t toHalChannel(uint32_t channel) noexcept;
};
```

---

### 2. Channel index mapping

STM32 HAL uses `TIM_CHANNEL_1..4` constants. Map the zero-based interface index to them:

```cpp
uint32_t Stm32Pwm::toHalChannel(uint32_t channel) noexcept
{
    constexpr uint32_t k_channels[] = {
        TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
    };
    // Caller has already validated the range
    return k_channels[channel];
}
```

---

### 3. Implement `start` and `stop`

```cpp
hel::ReturnCode Stm32Pwm::start(uint32_t channel) noexcept
{
    if (channel >= 4U)
        return hel::ReturnCode::ErrorParam;

    const auto hal_ch = toHalChannel(channel);
    return (HAL_TIM_PWM_Start(&m_htim, hal_ch) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}

hel::ReturnCode Stm32Pwm::stop(uint32_t channel) noexcept
{
    if (channel >= 4U)
        return hel::ReturnCode::ErrorParam;

    const auto hal_ch = toHalChannel(channel);
    return (HAL_TIM_PWM_Stop(&m_htim, hal_ch) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}
```

---

### 4. Implement `setDutyCycle`

Convert from permille to the compare register value using the current ARR.

```cpp
hel::ReturnCode Stm32Pwm::setDutyCycle(uint32_t channel,
                                         uint16_t duty_permille) noexcept
{
    if ((channel >= 4U) || (duty_permille > 1000U))
        return hel::ReturnCode::ErrorParam;

    const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&m_htim);
    const uint32_t ccr = (arr * duty_permille) / 1000U;
    __HAL_TIM_SET_COMPARE(&m_htim, toHalChannel(channel), ccr);
    return hel::ReturnCode::AnsweredRequest;
}
```

---

### 5. Implement `setFrequency`

Recalculate PSC and ARR from the timer input clock.

```cpp
hel::ReturnCode Stm32Pwm::setFrequency(uint32_t frequency_hz) noexcept
{
    if (frequency_hz == 0U)
        return hel::ReturnCode::ErrorParam;

    // Example: timer clock = APB1 * 2 (check your clock tree)
    const uint32_t timer_clk = HAL_RCC_GetPCLK1Freq() * 2U;
    const uint32_t period    = (timer_clk / frequency_hz) - 1U;

    if (period > 0xFFFFU) // 16-bit timer limit
        return hel::ReturnCode::ErrorParam;

    __HAL_TIM_SET_AUTORELOAD(&m_htim, period);
    return hel::ReturnCode::AnsweredRequest;
}
```

> **Warning**: Changing frequency affects all channels of the same timer simultaneously. Recalculate and reapply duty cycles after changing frequency if absolute duty-cycle values matter.

---

## Usage Example

```cpp
Stm32Pwm pwm(htim3);

// 1 kHz, 25 % duty on channel 0
pwm.setFrequency(1000U);
pwm.setDutyCycle(0U, 250U); // 250‰ = 25 %
pwm.start(0U);

// Change duty without stopping
pwm.setDutyCycle(0U, 750U); // 75 %

pwm.stop(0U);
```

---

## Checklist for New Implementations

- [ ] `start` returns `OperationRunning` if the channel is already active
- [ ] `stop` returns `ErrorInvalidState` if the channel is not running
- [ ] `setDutyCycle` returns `ErrorParam` for `duty_permille > 1000`
- [ ] `setFrequency` returns `ErrorParam` for `frequency_hz == 0` or out-of-range periods
- [ ] Duty cycle changes take effect on the next timer period (no glitches)
- [ ] Frequency change is documented as affecting all channels of the timer
- [ ] All public methods are `noexcept`
