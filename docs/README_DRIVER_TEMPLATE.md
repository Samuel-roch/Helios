# `iDriverName` — Short description

> One-sentence summary of what the peripheral does and its main usage pattern.

<!--
  Suggested image: block diagram showing the interface between application,
  iDriverName, and the target HAL (e.g. STM32 HAL, ESP-IDF).
  Recommended size: 800×300 px.

  ![Block diagram](../../docs/img/driver_name_diagram.png)
-->

---

## Features

- Feature 1 (e.g. blocking TX/RX with configurable timeout)
- Feature 2 (e.g. interrupt and DMA async transfers)
- Feature 3 (e.g. event callback via `DriverCallback`)
- No dynamic allocation; all state held in the implementation object

---

## Header

```cpp
#include <hel_idrivername>
```

---

## Types

### Events — `DriverEvent`

| Value | Description |
|---|---|
| `TxComplete` | Transmission completed successfully |
| `RxComplete` | Reception completed successfully |
| `ErrorGeneral` | Unspecified hardware error |

### Callback

```cpp
using DriverCallback = hel::Callback<void(DriverEvent, uint16_t)>;
```

Fired from ISR or driver-task context (implementation-defined). Must not block or allocate.

---

## API

### Blocking transfers

```cpp
[[nodiscard]] virtual ReturnCode write(ConstByteArray data, uint32_t timeout_ms) noexcept = 0;
[[nodiscard]] virtual ReturnCode read(ByteArray data, uint32_t timeout_ms) noexcept = 0;
```

| Method | Success | Timeout | Busy | Fault |
|---|---|---|---|---|
| `write` | `AnsweredRequest` | `ErrorTimeout` | `FunctionBusy` | `ErrorGeneral` |
| `read` | `AnsweredRequest` | `ErrorTimeout` | `FunctionBusy` | `ErrorGeneral` |

### Async transfers

```cpp
[[nodiscard]] virtual ReturnCode writeInterrupt(ConstByteArray data, DriverMode mode) noexcept = 0;
[[nodiscard]] virtual ReturnCode writeDMA(ConstByteArray data, DriverMode mode) noexcept = 0;
```

Returns immediately. Completion is reported via the registered `DriverCallback`.

> **DMA note:** the source buffer must remain valid until `DriverEvent::TxComplete` fires — the DMA controller reads directly from it after the call returns.

### Callback

```cpp
virtual void setCallback(DriverCallback callback) noexcept = 0;
```

Registers or replaces the event handler. Safe to call at any time; takes effect for the next async operation.

---

## Usage examples

### Blocking write

```cpp
hel::StringData<64> msg;
msg.printf("sensor=%d\n", value);

const hel::ReturnCode rc = driver.write(msg, 100U);
if (rc != hel::ReturnCode::AnsweredRequest) { /* handle error */ }
```

### Async DMA receive with callback

```cpp
static uint8_t rx_buf[128];

driver.setCallback(hel::DriverCallback(this, &MyClass::onDriverEvent));
driver.readDMA(hel::ByteArray(rx_buf, sizeof(rx_buf)), hel::DriverMode::Circular);

void MyClass::onDriverEvent(hel::DriverEvent event, uint16_t count) noexcept
{
    if (event == hel::DriverEvent::RxComplete) { processData(rx_buf, count); }
}
```

---

## Implementing for a new target

Derive from `hel::iDriverName` and map each virtual method to the target HAL:

```cpp
class DriverName : public hel::iDriverName
{
public:
    hel::ReturnCode write(hel::ConstByteArray data, uint32_t timeout_ms) noexcept override
    {
        // HAL_DriverName_Transmit(...)
    }
    // ... remaining methods
};
```

---

## Thread-safety and ISR constraints

- Individual methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- Blocking methods (`write`, `read`) must **not** be called from ISR context.
- `handleEvent` is intended for ISR or driver-task context only; it is `noexcept` and must not block.
