# iPower — Power Management Interface

`hel::iPower` is the hardware-agnostic interface for MCU power management. It covers low-power sleep entry, software reset, and reset-source query.

## API Summary

| Method | Description |
|---|---|
| `enterSleep(mode)` | Enter a low-power mode; blocks until wakeup (Sleep/Stop) or never returns (Standby/Shutdown) |
| `reset()` | Perform a software system reset — never returns |
| `getResetSource(source)` | Query the cause of the last reset |

### Sleep Mode Behaviour

| `SleepMode` | Returns? | SRAM retained? | Wakeup |
|---|---|---|---|
| `Sleep` | Yes | Yes | Any interrupt |
| `Stop` | Yes | Yes | EXTI, RTC alarm, etc. |
| `Standby` | No (resets) | No | RTC alarm, wakeup pin |
| `Shutdown` | No (resets) | No | External wakeup pin only |

---

## Implementing a Concrete Driver

### 1. Inherit from `iPower`

```cpp
#include <hel_ipower>

class Stm32Power final : public hel::iPower
{
public:
    hel::ReturnCode enterSleep(hel::SleepMode mode) noexcept override;
    [[noreturn]] void reset() noexcept override;
    hel::ReturnCode getResetSource(hel::ResetSource& source) noexcept override;
};
```

---

### 2. Implement `enterSleep`

```cpp
hel::ReturnCode Stm32Power::enterSleep(hel::SleepMode mode) noexcept
{
    switch (mode)
    {
    case hel::SleepMode::Sleep:
        HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
        return hel::ReturnCode::AnsweredRequest;

    case hel::SleepMode::Stop:
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        // Restore system clock after wakeup from Stop (HSI is selected by HW)
        SystemClock_Config();
        return hel::ReturnCode::AnsweredRequest;

    case hel::SleepMode::Standby:
        HAL_PWR_EnterSTANDBYMode(); // does not return
        break;

    case hel::SleepMode::Shutdown:
        HAL_PWREx_EnterSHUTDOWNMode(); // does not return
        break;
    }

    return hel::ReturnCode::ErrorGeneral; // unreachable, satisfies compiler
}
```

**Rules:**
- For `Stop` mode on STM32, the system clock reverts to HSI after wakeup. You must reconfigure the PLL before resuming normal operation.
- `Standby` and `Shutdown` are points of no return — persist any critical state to RTC backup registers or flash before calling.
- Ensure all peripherals are in a safe state (DMA stopped, SPI CS deasserted) before entering low-power modes.

---

### 3. Implement `reset`

```cpp
[[noreturn]] void Stm32Power::reset() noexcept
{
    NVIC_SystemReset(); // never returns
    while (true) {}    // suppress compiler warning
}
```

---

### 4. Implement `getResetSource`

Read and clear the RCC reset flags immediately after boot (before any other code clears them).

```cpp
hel::ReturnCode Stm32Power::getResetSource(hel::ResetSource& source) noexcept
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
        source = hel::ResetSource::PowerOn;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
        source = hel::ResetSource::External;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
        source = hel::ResetSource::Software;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ||
             __HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
        source = hel::ResetSource::Watchdog;
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))
        source = hel::ResetSource::BrownOut;
    else
        source = hel::ResetSource::Unknown;

    __HAL_RCC_CLEAR_RESET_FLAGS();
    return hel::ReturnCode::AnsweredRequest;
}
```

> Call `getResetSource` once during BSP initialization and cache the result. The flags are cleared after the first read.

---

## Usage Example

```cpp
Stm32Power power;

// Log reset cause on boot
hel::ResetSource cause{};
power.getResetSource(cause);

// Enter Stop mode until RTC alarm or EXTI
if (power.enterSleep(hel::SleepMode::Stop) == hel::ReturnCode::AnsweredRequest)
{
    // resumed from Stop — clock has been reconfigured
}

// Software reset
power.reset(); // never returns
```

---

## Checklist for New Implementations

- [ ] `enterSleep(Stop)` reconfigures the system clock after wakeup
- [ ] `enterSleep(Standby)` and `enterSleep(Shutdown)` never return
- [ ] `reset()` is marked `[[noreturn]]` and never returns
- [ ] `getResetSource` clears hardware flags after reading to avoid stale values on next boot
- [ ] All peripherals are documented as needing safe-state before `enterSleep`
- [ ] All public methods are `noexcept`
