# Temperature Sensor

> Interface for external temperature sensors with optional humidity reading and alert thresholds.

<!--
  Suggested image: block diagram showing sensor → I2C/1-Wire → iTemperatureSensor → application.
  Recommended size: 800×280 px.

  ![Temperature sensor block diagram](../../../docs/img/temperature_diagram.png)
-->

---

## Features

- Temperature reading in tenths of °C — no floating point required
- Optional humidity reading in percent (returns `ErrorNotSupported` if absent)
- Configurable high and low alert thresholds with callback notification
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_itemperature_sensor>
```

---

## Types

### `TemperatureEvent`

| Value | Description |
|---|---|
| `DataReady` | A new measurement is available |
| `HighAlertTriggered` | Temperature rose above the high threshold |
| `LowAlertTriggered` | Temperature fell below the low threshold |

### Temperature unit

All temperature values are `int32_t` in **tenths of °C** (deci-Celsius):

| Value | Meaning |
|---|---|
| `253` | 25.3 °C |
| `0` | 0.0 °C |
| `-100` | -10.0 °C |

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC and verify communication |

### Measurements

| Method | Out | Description |
|---|---|---|
| `getTemperature(dc)` | `int32_t` | Temperature in tenths of °C |
| `getHumidity(pct)` | `uint8_t` | Relative humidity in percent [0, 100] (optional) |

### Alert thresholds

| Method | Description |
|---|---|
| `setHighAlert(dc)` / `getHighAlert(dc)` | Upper temperature alert threshold |
| `setLowAlert(dc)` / `getLowAlert(dc)` | Lower temperature alert threshold |

Returns `ErrorNotSupported` if the IC has no programmable alert registers.

---

## Usage examples

### Polling temperature and humidity

```cpp
#include <hel_itemperature_sensor>

sensor.begin();

int32_t temp_dc;
sensor.getTemperature(temp_dc);
// temp_dc / 10 = integer °C,  temp_dc % 10 = tenths

uint8_t hum;
if (sensor.getHumidity(hum) == hel::ReturnCode::AnsweredRequest)
{
    // hum in percent
}
```

### Alert callback

```cpp
sensor.setHighAlert(800);   // 80.0 °C
sensor.setLowAlert(-200);   // -20.0 °C
sensor.setCallback(hel::TemperatureSensorCallback(this, &ThermalMonitor::onTempEvent));

void ThermalMonitor::onTempEvent(hel::TemperatureEvent event) noexcept
{
    if (event == hel::TemperatureEvent::HighAlertTriggered)
    {
        m_cooler.enable();
    }
}
```

---

## Implementing for a new target

```cpp
class Tmp117 : public hel::iTemperatureSensor
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify device ID register over I2C
    }

    hel::ReturnCode getTemperature(int32_t& temperature_dc) noexcept override
    {
        // read TEMP_RESULT register, multiply raw count by 0.0078125 °C/LSB
        // return as tenths of °C (multiply by 10, keep as integer)
    }

    hel::ReturnCode getHumidity(uint8_t&) noexcept override
    {
        return hel::ReturnCode::ErrorNotSupported;  // TMP117 is temperature-only
    }

    // ... remaining methods

protected:
    void handleEvent(hel::TemperatureEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::TemperatureSensorCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not disable interrupts for extended periods.
