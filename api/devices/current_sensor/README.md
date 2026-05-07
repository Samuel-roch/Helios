# Current Sensor

> Interface for external current and power monitoring ICs — voltage, current, power, and optional energy accumulation.

<!--
  Suggested image: block diagram showing the power path with shunt resistor,
  INA-family IC, and the data path to iCurrentSensor.
  Recommended size: 900×300 px.

  ![Current sensor block diagram](../../../docs/img/current_sensor_diagram.png)
-->

---

## Features

- Bus voltage, shunt current, and calculated power readings
- Optional hardware energy accumulator (mWh) — returns `ErrorNotSupported` on ICs without it (e.g. INA219)
- Configurable over-current and voltage alert thresholds
- Asynchronous alert notification via `CurrentSensorCallback`
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_icurrent_sensor>
```

---

## Units

| Quantity | Unit | Type |
|---|---|---|
| Voltage | millivolts (mV) | `int32_t` |
| Current | milliamperes (mA) | `int32_t` |
| Power | milliwatts (mW) | `int32_t` |
| Energy | milliwatt-hours (mWh) | `int32_t` |

## Types

### `CurrentSensorEvent`

| Value | Description |
|---|---|
| `DataReady` | A new conversion is complete |
| `OverCurrentAlert` | Current exceeded the configured limit |
| `UnderVoltageAlert` | Bus voltage fell below the lower limit |
| `OverVoltageAlert` | Bus voltage exceeded the upper limit |

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC, calibrate for the shunt resistor, verify communication |

### Measurements

| Method | Description |
|---|---|
| `getVoltage(mv)` | Bus voltage in mV |
| `getCurrent(ma)` | Shunt current in mA |
| `getPower(mw)` | Instantaneous power in mW |
| `getEnergy(mwh)` | Accumulated energy in mWh (optional) |
| `resetEnergy()` | Clear the energy accumulator (optional) |

### Alert thresholds

| Method | Description |
|---|---|
| `setCurrentThreshold(ma)` / `getCurrentThreshold(ma)` | Over-current alert limit |
| `setUnderVoltageThreshold(mv)` / `getUnderVoltageThreshold(mv)` | Bus under-voltage limit |
| `setOverVoltageThreshold(mv)` / `getOverVoltageThreshold(mv)` | Bus over-voltage limit |

All threshold methods return `ErrorNotSupported` if the IC has no alert registers.

---

## Usage examples

### Reading power consumption

```cpp
#include <hel_icurrent_sensor>

sensor.begin();

int32_t v_mv, i_ma, p_mw;
sensor.getVoltage(v_mv);
sensor.getCurrent(i_ma);
sensor.getPower(p_mw);
// p_mw / 1000 = watts
```

### Energy metering with callback

```cpp
sensor.setCurrentThreshold(5000U);  // alert at 5 A
sensor.setCallback(hel::CurrentSensorCallback(this, &PowerMonitor::onSensorEvent));
sensor.resetEnergy();

// ... later, read accumulated energy
int32_t energy_mwh;
sensor.getEnergy(energy_mwh);

void PowerMonitor::onSensorEvent(hel::CurrentSensorEvent event) noexcept
{
    if (event == hel::CurrentSensorEvent::OverCurrentAlert)
    {
        m_protection.trip();
    }
}
```

---

## Implementing for a new target

```cpp
class Ina226 : public hel::iCurrentSensor
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // write calibration register: CAL = 0.00512 / (current_lsb * r_shunt)
    }

    hel::ReturnCode getCurrent(int32_t& current_ma) noexcept override
    {
        // read CURRENT register, multiply by current_lsb
    }

    hel::ReturnCode getEnergy(int32_t&) noexcept override
    {
        return hel::ReturnCode::ErrorNotSupported;  // INA226 has no energy register
    }

    // ... remaining methods

protected:
    void handleEvent(hel::CurrentSensorEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::CurrentSensorCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not disable interrupts for extended periods.
