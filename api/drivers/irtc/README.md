# iRtc — Real-Time Clock Interface

`hel::iRtc` is the hardware-agnostic interface for a real-time clock peripheral. It covers time read/write and a single alarm with an ISR-fired callback.

## API Summary

| Method | Description |
|---|---|
| `getTime(datetime)` | Read the current date and time into a `struct tm` |
| `setTime(datetime)` | Set the current date and time |
| `setAlarm(alarm)` | Arm a one-shot alarm at the given date/time |
| `clearAlarm()` | Disarm the alarm without firing the callback |
| `setCallback(cb)` | Register the alarm event callback |

The `RtcCallback` signature is `void handler(ReturnCode code) noexcept` and fires from ISR context when the alarm matches.

---

## `struct tm` Field Reference

| Field | Meaning | Range |
|---|---|---|
| `tm_year` | Years since 1900 | e.g. 125 = 2025 |
| `tm_mon` | Month (0-based) | 0–11 |
| `tm_mday` | Day of month | 1–31 |
| `tm_hour` | Hour | 0–23 |
| `tm_min` | Minute | 0–59 |
| `tm_sec` | Second | 0–59 |

---

## Implementing a Concrete Driver

### 1. Inherit from `iRtc`

```cpp
#include <hel_irtc>
#include <ctime>

class Stm32Rtc final : public hel::iRtc
{
public:
    explicit Stm32Rtc(RTC_HandleTypeDef& hrtc) noexcept : m_hrtc(hrtc) {}

    hel::ReturnCode getTime(struct tm& datetime) noexcept override;
    hel::ReturnCode setTime(const struct tm& datetime) noexcept override;
    hel::ReturnCode setAlarm(const struct tm& alarm) noexcept override;
    hel::ReturnCode clearAlarm() noexcept override;
    void setCallback(hel::RtcCallback callback) noexcept override;

protected:
    void handleAlarm(hel::ReturnCode code) noexcept override;

private:
    RTC_HandleTypeDef& m_hrtc;
    hel::RtcCallback   m_callback{};
};
```

---

### 2. Implement `getTime` and `setTime`

```cpp
hel::ReturnCode Stm32Rtc::getTime(struct tm& dt) noexcept
{
    RTC_TimeTypeDef t{};
    RTC_DateTypeDef d{};

    if ((HAL_RTC_GetTime(&m_hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_GetDate(&m_hrtc, &d, RTC_FORMAT_BIN) != HAL_OK))
        return hel::ReturnCode::ErrorGeneral;

    // HAL requires GetDate to be called after GetTime to unlock the shadow registers
    dt.tm_hour  = static_cast<int>(t.Hours);
    dt.tm_min   = static_cast<int>(t.Minutes);
    dt.tm_sec   = static_cast<int>(t.Seconds);
    dt.tm_mday  = static_cast<int>(d.Date);
    dt.tm_mon   = static_cast<int>(d.Month) - 1; // tm_mon is 0-based
    dt.tm_year  = static_cast<int>(d.Year) + 100; // RTC year is 0-99; tm_year from 1900
    return hel::ReturnCode::AnsweredRequest;
}

hel::ReturnCode Stm32Rtc::setTime(const struct tm& dt) noexcept
{
    RTC_TimeTypeDef t{};
    t.Hours   = static_cast<uint8_t>(dt.tm_hour);
    t.Minutes = static_cast<uint8_t>(dt.tm_min);
    t.Seconds = static_cast<uint8_t>(dt.tm_sec);

    RTC_DateTypeDef d{};
    d.Date  = static_cast<uint8_t>(dt.tm_mday);
    d.Month = static_cast<uint8_t>(dt.tm_mon + 1);
    d.Year  = static_cast<uint8_t>(dt.tm_year - 100);

    if ((HAL_RTC_SetTime(&m_hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) ||
        (HAL_RTC_SetDate(&m_hrtc, &d, RTC_FORMAT_BIN) != HAL_OK))
        return hel::ReturnCode::ErrorGeneral;

    return hel::ReturnCode::AnsweredRequest;
}
```

> **Important**: On STM32, `HAL_RTC_GetDate` **must** be called after `HAL_RTC_GetTime` even if the date is not needed. Omitting the date read leaves the shadow registers locked and subsequent `GetTime` calls return stale data.

---

### 3. Implement `setAlarm` and `clearAlarm`

```cpp
hel::ReturnCode Stm32Rtc::setAlarm(const struct tm& alarm) noexcept
{
    RTC_AlarmTypeDef a{};
    a.AlarmTime.Hours   = static_cast<uint8_t>(alarm.tm_hour);
    a.AlarmTime.Minutes = static_cast<uint8_t>(alarm.tm_min);
    a.AlarmTime.Seconds = static_cast<uint8_t>(alarm.tm_sec);
    a.AlarmMask         = RTC_ALARMMASK_DATEWEEKDAY; // match HH:MM:SS only
    a.Alarm             = RTC_ALARM_A;

    HAL_RTC_DeactivateAlarm(&m_hrtc, RTC_ALARM_A); // replace any existing alarm

    return (HAL_RTC_SetAlarm_IT(&m_hrtc, &a, RTC_FORMAT_BIN) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}

hel::ReturnCode Stm32Rtc::clearAlarm() noexcept
{
    HAL_RTC_DeactivateAlarm(&m_hrtc, RTC_ALARM_A);
    return hel::ReturnCode::AnsweredRequest;
}
```

---

### 4. Wire the ISR callback

```cpp
void Stm32Rtc::setCallback(hel::RtcCallback callback) noexcept { m_callback = callback; }
void Stm32Rtc::handleAlarm(hel::ReturnCode code) noexcept      { m_callback(code); }

// BSP:
extern Stm32Rtc g_rtc;

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef*)
{
    g_rtc.handleAlarm(hel::ReturnCode::AnsweredRequest);
}
```

---

## Usage Example

```cpp
Stm32Rtc rtc(hrtc);

// Set time to 2025-06-15 08:30:00
struct tm now{};
now.tm_year = 125; now.tm_mon = 5; now.tm_mday = 15;
now.tm_hour = 8;   now.tm_min = 30; now.tm_sec = 0;
rtc.setTime(now);

// Read back
struct tm current{};
rtc.getTime(current);

// Alarm at 08:31:00
rtc.setCallback(hel::RtcCallback(&handler, &MyClass::onAlarm));
struct tm alarm = current;
alarm.tm_min++;
rtc.setAlarm(alarm);
```

---

## Checklist for New Implementations

- [ ] `getTime` always calls the HAL date getter after the HAL time getter (shadow register unlock)
- [ ] `tm_mon` is zero-based in `struct tm` but one-based in most RTCs — convert correctly
- [ ] `tm_year` is years since 1900; STM32 RTC year is 0–99 — convert correctly
- [ ] `setAlarm` replaces any previously armed alarm (no queue)
- [ ] `clearAlarm` is a no-op when no alarm is armed
- [ ] `handleAlarm` is called only from ISR dispatch
- [ ] All public methods are `noexcept`
