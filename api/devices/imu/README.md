# IMU

> Interface for 6-axis inertial measurement units — accelerometer and gyroscope, with optional integrated magnetometer.

<!--
  Suggested image: block diagram showing the three sensor axes and the data path
  from IC → iImu → ImuData struct.
  Recommended size: 800×300 px.

  ![IMU block diagram](../../../docs/img/imu_diagram.png)
-->

---

## Features

- Single read call returns 6-axis accelerometer + gyroscope as `ImuData`
- Optional 3-axis magnetometer via `readMag` (returns `ErrorNotSupported` if absent)
- Optional on-die temperature reading
- Configurable full-scale range and output data rate
- Asynchronous data-ready and motion-event notification via `ImuCallback`
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_iimu>
```

---

## Types

### `ImuAccelRange`

| Value | Range |
|---|---|
| `G2` | ±2 g — highest sensitivity |
| `G4` | ±4 g |
| `G8` | ±8 g |
| `G16` | ±16 g — widest range |

### `ImuGyroRange`

| Value | Range |
|---|---|
| `Dps250` | ±250 °/s — highest sensitivity |
| `Dps500` | ±500 °/s |
| `Dps1000` | ±1000 °/s |
| `Dps2000` | ±2000 °/s — widest range |

### `ImuEvent`

| Value | Description |
|---|---|
| `DataReady` | A new sample is available |
| `MotionDetected` | Wake-on-motion threshold exceeded |
| `FreeFallDetected` | Near-zero acceleration detected |
| `TapDetected` | Single or double tap detected |

### `ImuData`

| Field | Unit | Description |
|---|---|---|
| `accel_x_mg` | mg | X-axis acceleration in milli-g |
| `accel_y_mg` | mg | Y-axis acceleration in milli-g |
| `accel_z_mg` | mg | Z-axis acceleration in milli-g |
| `gyro_x_mdps` | mdps | X-axis angular rate in milli-°/s |
| `gyro_y_mdps` | mdps | Y-axis angular rate in milli-°/s |
| `gyro_z_mdps` | mdps | Z-axis angular rate in milli-°/s |

1 mg ≈ 9.807 mm/s². 1 mdps = 0.001 °/s.

### `ImuMagData`

| Field | Unit | Description |
|---|---|---|
| `x_mgauss` | mGauss | X-axis magnetic field |
| `y_mgauss` | mGauss | Y-axis magnetic field |
| `z_mgauss` | mGauss | Z-axis magnetic field |

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize IC and verify communication |

### Measurements

| Method | Description |
|---|---|
| `read(data)` | Read 6-axis accelerometer + gyroscope |
| `readMag(data)` | Read 3-axis magnetometer (optional) |
| `getTemperature(dc)` | Read on-die temperature in tenths of °C (optional) |
| `isDataReady(ready)` | Poll data-ready status |

### Configuration

| Method | Description |
|---|---|
| `setAccelRange(range)` / `getAccelRange(range)` | Accelerometer full-scale range |
| `setGyroRange(range)` / `getGyroRange(range)` | Gyroscope full-scale range |
| `setOutputDataRate(hz)` / `getOutputDataRate(hz)` | Sample rate for all axes |

---

## Usage examples

### Polling IMU data

```cpp
#include <hel_iimu>

imu.begin();
imu.setAccelRange(hel::ImuAccelRange::G4);
imu.setGyroRange(hel::ImuGyroRange::Dps500);
imu.setOutputDataRate(100U);  // 100 Hz

hel::ImuData data;
if (imu.read(data) == hel::ReturnCode::AnsweredRequest)
{
    // data.accel_z_mg, data.gyro_x_mdps, ...
}
```

### Data-ready interrupt

```cpp
imu.setCallback(hel::ImuCallback(this, &Controller::onImuEvent));

void Controller::onImuEvent(hel::ImuEvent event) noexcept
{
    if (event == hel::ImuEvent::DataReady)
    {
        hel::ImuData data;
        m_imu.read(data);
        m_filter.update(data);
    }
    else if (event == hel::ImuEvent::FreeFallDetected)
    {
        m_safetyHandler.trigger();
    }
}
```

### Optional magnetometer

```cpp
hel::ImuMagData mag;
const hel::ReturnCode rc = imu.readMag(mag);
if (rc == hel::ReturnCode::ErrorNotSupported)
{
    // IMU has no integrated compass — use a separate magnetometer
}
```

---

## Implementing for a new target

```cpp
class Icm42688 : public hel::iImu
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify WHO_AM_I register over SPI/I2C
    }

    hel::ReturnCode read(hel::ImuData& data) noexcept override
    {
        // burst-read ACCEL and GYRO output registers
        // convert raw counts to mg / mdps using current range
    }

    hel::ReturnCode readMag(hel::ImuMagData&) noexcept override
    {
        return hel::ReturnCode::ErrorNotSupported;  // no integrated compass
    }

    // ... remaining methods

protected:
    void handleEvent(hel::ImuEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::ImuCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- Blocking methods must **not** be called from ISR context.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not disable interrupts for extended periods.
