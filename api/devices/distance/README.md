# Distance Sensor

> Interface for time-of-flight and ultrasonic distance sensors — single-shot and continuous ranging.

<!--
  Suggested image: block diagram showing the sensor emitter/receiver,
  the ranging modes, and the data path to iDistanceSensor.
  Recommended size: 800×300 px.

  ![Distance sensor block diagram](../../../docs/img/distance_diagram.png)
-->

---

## Features

- Single-shot blocking measurement with configurable timeout
- Continuous ranging mode with data-ready callback
- Three ranging profiles: Short, Medium, Long
- Configurable measurement period for continuous mode
- Out-of-range event when the target exceeds the measurable distance
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_idistance_sensor>
```

---

## Types

### `DistanceRangingMode`

| Value | Typical max range | Notes |
|---|---|---|
| `Short` | ~1.3 m | Best ambient light immunity |
| `Medium` | ~3 m | Balanced range and immunity |
| `Long` | ~4+ m | Maximum range, reduced immunity |

Implementations that do not support a requested mode return `ErrorNotSupported`.

### `DistanceSensorEvent`

| Value | Description |
|---|---|
| `DataReady` | A new measurement is available |
| `OutOfRange` | Target is beyond the sensor's measurable distance |

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC, load calibration, verify communication |
| `startContinuous()` | Start continuous ranging |
| `stopContinuous()` | Stop continuous ranging |

### Measurement

| Method | Description |
|---|---|
| `getDistance(mm, timeout_ms)` | Blocking single-shot measurement in millimeters |
| `isDataReady(ready)` | Poll data-ready status |

### Configuration

| Method | Description |
|---|---|
| `setRangingMode(mode)` / `getRangingMode(mode)` | Ranging profile |
| `setMeasurementPeriod(ms)` / `getMeasurementPeriod(ms)` | Period between measurements in continuous mode |

---

## Usage examples

### Single-shot measurement

```cpp
#include <hel_idistance_sensor>

sensor.begin();
sensor.setRangingMode(hel::DistanceRangingMode::Long);

uint32_t dist_mm;
const hel::ReturnCode rc = sensor.getDistance(dist_mm, 100U);
if (rc == hel::ReturnCode::AnsweredRequest)
{
    // dist_mm contains the measured distance
}
```

### Continuous ranging with callback

```cpp
sensor.setRangingMode(hel::DistanceRangingMode::Medium);
sensor.setMeasurementPeriod(50U);  // 50 ms → 20 Hz
sensor.setCallback(hel::DistanceSensorCallback(this, &Lidar::onSensorEvent));
sensor.startContinuous();

void Lidar::onSensorEvent(hel::DistanceSensorEvent event) noexcept
{
    if (event == hel::DistanceSensorEvent::DataReady)
    {
        uint32_t dist_mm;
        m_sensor.getDistance(dist_mm, 0U);  // data already ready, no wait
        m_map.update(dist_mm);
    }
    else if (event == hel::DistanceSensorEvent::OutOfRange)
    {
        m_map.markOutOfRange();
    }
}
```

---

## Implementing for a new target

```cpp
class Vl53l1x : public hel::iDistanceSensor
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify model ID register over I2C, load trimming parameters
    }

    hel::ReturnCode getDistance(uint32_t& distance_mm, uint32_t timeout_ms) noexcept override
    {
        // start ranging, poll interrupt status until ready or timeout
        // read RESULT__FINAL_CROSSTALK_CORRECTED_RANGE_MM register
    }

    hel::ReturnCode setRangingMode(hel::DistanceRangingMode mode) noexcept override
    {
        // map Short/Medium/Long to VL53L1X timing budget and distance mode
    }

    // ... remaining methods

protected:
    void handleEvent(hel::DistanceSensorEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::DistanceSensorCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `getDistance` (blocking) must **not** be called from ISR context.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not disable interrupts for extended periods.
