# Power Supply Devices

> Abstract interfaces for battery management ICs — charger control and fuel gauge monitoring.

<!--
  Suggested image: system block diagram showing the power path:
  Input (USB/DC) → iCharger → Battery ← iFuelGauge → System Load
  Recommended size: 900×350 px.

  ![Power supply block diagram](../../../docs/img/power_suply_diagram.png)
-->

This directory provides two complementary interfaces that together cover the full battery management cycle:

| Interface | Header | IC examples |
|---|---|---|
| [`iCharger`](#icharger) | `hel_icharger` | BQ25798, MP2762, LT3651 |
| [`iFuelGauge`](#ifuegauge) | `hel_ifuel_gauge` | BQ27427, MAX17055, LC709203 |

---

## `iCharger`

Controls the charging IC — input power negotiation, CC/CV parameters, OTG, and fault detection.

### Enumerations

**`ChargerStatus`** — high-level charging state

| Value | Description |
|---|---|
| `Unknown` | State cannot be determined |
| `Charging` | Actively transferring energy to the battery |
| `Discharging` | Battery is supplying the system load |
| `NotCharging` | Charger connected but charging inhibited |
| `Full` | Battery fully charged; charging terminated |

**`ChargePhase`** — active stage of the CC/CV algorithm

| Value | Description |
|---|---|
| `None` | Charger idle or disabled |
| `Trickle` | Very low current to a deeply discharged cell |
| `Precharge` | Limited current until cell voltage reaches precharge limit |
| `ConstantCurrent` | Main CC phase at configured fast-charge current |
| `ConstantVoltage` | CV phase; voltage held, current tapers to cut-off |
| `Maintenance` | Float charge after CV completes |

**`ChargerHealth`** — battery and charger condition

`Unknown` · `Good` · `Overheat` · `Cold` · `Overvoltage` · `Undervoltage` · `Overcurrent` · `Dead` · `SafetyTimerExpired`

**`ChargerFault`** — hardware fault register

`Normal` · `NtcCold` · `NtcHot` · `BatOvervoltage` · `BatUndervoltage` · `InputOvervoltage` · `InputUndervoltage` · `ChargeOvercurrent` · `WatchdogReset`

**`ChargerEvent`** — asynchronous events via `ChargerCallback`

`StatusChanged` · `PhaseChanged` · `HealthAlert` · `FaultDetected` · `InputConnected` · `InputDisconnected` · `WatchdogExpired`

### API summary

**Control**

| Method | Description |
|---|---|
| `begin()` | Initialize IC and verify communication |
| `setOperationMode(mode)` | Switch between `Normal` and `Shipping` mode |
| `resetDefaults()` | Restore IC registers to power-on defaults |

**Status and measurements** — all units: voltages in mV, currents in mA

| Method | Out parameter | Description |
|---|---|---|
| `getStatus(status)` | `ChargerStatus` | Active charging state |
| `getHealth(health)` | `ChargerHealth` | Battery and charger health |
| `getPhase(phase)` | `ChargePhase` | Active CC/CV phase |
| `getFault(fault)` | `ChargerFault` | Hardware fault register |
| `getInputVoltage(mv)` | `int32_t` | Input source voltage |
| `getBatteryVoltage(mv)` | `int32_t` | Battery terminal voltage |
| `getChargeCurrent(ma)` | `int32_t` | Instantaneous charge current |
| `isInputPresent(present)` | `bool` | Whether a valid input source is detected |

**Configuration** — all units: voltages in mV, currents in mA

| Method | Description |
|---|---|
| `setChargingEnabled(bool)` | Enable or inhibit charging |
| `setInputCurrentLimit(ma)` | Maximum input current (ILIM) |
| `setInputVoltageLimit(mv)` | Minimum input voltage threshold (VINDPM) |
| `setPreChargeCurrent(ma)` | Current during Trickle/Precharge phases |
| `setTerminationCurrent(ma)` | End-of-charge cut-off current |
| `setConstantChargeCurrent(ma)` | CC phase fast-charge current |
| `setConstantChargeVoltage(mv)` | CV phase regulation voltage target |
| `setChargingVoltageLimit(mv)` | Absolute battery overvoltage cut-off |
| `setBoostVoltage(mv)` | OTG/boost output voltage |

Each setter has a corresponding `get*` method.

### Usage example

```cpp
#include <hel_icharger>

// Initialization
charger.begin();
charger.setConstantChargeCurrent(2000U);   // 2 A CC
charger.setConstantChargeVoltage(4200U);   // 4.2 V CV
charger.setTerminationCurrent(100U);       // 100 mA cut-off
charger.setChargingEnabled(true);

// Register event callback
charger.setCallback(hel::ChargerCallback(this, &PowerManager::onChargerEvent));

// Monitor
hel::ChargerStatus status;
charger.getStatus(status);
if (status == hel::ChargerStatus::Full) { /* notify */ }

void PowerManager::onChargerEvent(hel::ChargerEvent event) noexcept
{
    if (event == hel::ChargerEvent::FaultDetected)
    {
        hel::ChargerFault fault;
        m_charger.getFault(fault);
        // log fault
    }
}
```

---

## `iFuelGauge`

Monitors battery state — SoC, capacity, power, temperature, and lifetime metrics.

### Enumerations

**`FuelGaugeAlert`** — conditions reported via `FuelGaugeCallback`

| Value | Description |
|---|---|
| `LowCapacity` | SoC fell below the low threshold |
| `CriticalCapacity` | SoC fell below the critical threshold |
| `Overvoltage` | Voltage exceeded the upper alert limit |
| `Undervoltage` | Voltage fell below the lower alert limit |
| `Overtemperature` | Temperature exceeded the upper limit |
| `Undertemperature` | Temperature fell below the lower limit |
| `FullCharge` | Battery reached full charge |

**`OperationMode`** — IC power mode

`Normal` · `Sleep` · `DeepSleep`

### API summary

**Control**

| Method | Description |
|---|---|
| `begin()` | Initialize IC and verify communication |
| `reset()` | Full IC reset; clears all learned capacity data |
| `setOperationMode(mode)` | Switch between Normal, Sleep, and DeepSleep |
| `setCapacity(mah)` | Set battery design capacity |
| `setDesignEnergy(mwh)` | Set battery design energy (optional) |
| `setTerminateVoltage(mv)` | Set minimum battery voltage for empty detection |

**Measurements** — voltages in mV, currents in mA, temperatures in dC (tenths of °C)

| Method | Out parameter | Description |
|---|---|---|
| `getVoltage(mv)` | `int32_t` | Battery terminal voltage |
| `getCurrent(ma)` | `int32_t` | Instantaneous current (+ charge / − discharge) |
| `getTemperature(dc)` | `int32_t` | Battery temperature (e.g. 253 = 25.3 °C) |
| `getAveragePower(mw)` | `int32_t` | Average power (+ charge / − discharge) |

**State of charge**

| Method | Out parameter | Description |
|---|---|---|
| `getStateOfCharge(%)` | `uint8_t` | SoC in percent [0, 100] |
| `getRemainingCharge(mah)` | `int32_t` | Remaining charge |
| `getFullChargeCapacity(mah)` | `int32_t` | Learned full-charge capacity |
| `getTimeToEmpty(s)` | `uint32_t` | Estimated seconds to empty |
| `getTimeToFull(s)` | `uint32_t` | Estimated seconds to full |
| `getCycleCount(n)` | `uint16_t` | Accumulated charge cycles |

Methods that are not supported by the IC return `ReturnCode::ErrorNotSupported`.

### Usage example

```cpp
#include <hel_ifuel_gauge>

// Initialization
gauge.begin();
gauge.setCapacity(3000U);     // 3000 mAh cell
gauge.setTerminateVoltage(3000U);  // 3.0 V empty threshold

// Register alert callback
gauge.setCallback(hel::FuelGaugeCallback(this, &PowerManager::onGaugeAlert));

// Read SoC
uint8_t soc;
gauge.getStateOfCharge(soc);

// Read time to empty
uint32_t tte_s;
if (gauge.getTimeToEmpty(tte_s) == hel::ReturnCode::AnsweredRequest)
{
    // tte_s / 60U gives minutes remaining
}

void PowerManager::onGaugeAlert(hel::FuelGaugeAlert alert) noexcept
{
    if (alert == hel::FuelGaugeAlert::CriticalCapacity) { shutdownSequence(); }
}
```

---

## Implementing for a new target

```cpp
class Bq25798 : public hel::iCharger
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify device ID over I2C
    }
    hel::ReturnCode setConstantChargeCurrent(uint32_t current_ma) noexcept override
    {
        // write ICHG register
    }
    // ... remaining methods

protected:
    void handleEvent(hel::ChargerEvent event) noexcept override
    {
        m_callback(event);
    }
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` / `handleAlert` are intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not call functions that disable interrupts for extended periods.
