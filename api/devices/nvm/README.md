# Non-Volatile Memory

> Unified interface for NOR Flash, NAND Flash, and EEPROM — blocking and asynchronous read, write, and erase.

<!--
  Suggested image: block diagram showing the application, iNvm interface,
  and the three concrete memory types (NOR, NAND, EEPROM) below it.
  Recommended size: 900×350 px.

  ![NVM block diagram](../../../docs/img/nvm_diagram.png)
-->

---

## Features

- Single interface for NOR Flash, NAND Flash, and EEPROM
- Blocking read, write, and erase with configurable timeout
- Asynchronous write and block-erase with event callback
- Device geometry discovery via `NvmInfo` (capacity, page size, erase block size)
- No dynamic allocation; implementations map each method to the underlying IC

---

## Header

```cpp
#include <hel_invm>
```

---

## Types

### `NvmType`

| Value | Description |
|---|---|
| `Unknown` | Memory type could not be determined |
| `Nor` | NOR Flash — byte/word random read, sector or block erase |
| `Nand` | NAND Flash — page-based read/write, block erase |
| `Eeprom` | EEPROM — byte-addressable read/write, no explicit erase step |

### `NvmEvent`

| Value | Description |
|---|---|
| `WriteComplete` | Asynchronous write completed successfully |
| `EraseComplete` | Asynchronous erase completed successfully |
| `ErrorWrite` | Asynchronous write failed |
| `ErrorErase` | Asynchronous erase failed |

### `NvmInfo`

| Field | Type | Description |
|---|---|---|
| `type` | `NvmType` | Physical memory technology |
| `capacity` | `uint32_t` | Total device capacity in bytes |
| `page_size` | `uint32_t` | Read/write page size in bytes (1 for byte-addressable devices) |
| `erase_block_size` | `uint32_t` | Smallest erasable unit in bytes (0 if erase is not needed) |
| `write_granularity` | `uint32_t` | Minimum number of bytes per write operation |

### `NvmCallback`

```cpp
using NvmCallback = hel::Callback<void(NvmEvent)>;
```

Fired from ISR or driver-task context (implementation-defined). Must not block or allocate.

---

## API

### Control

| Method | Description |
|---|---|
| `begin()` | Initialize the device and verify communication |
| `getInfo(info)` | Retrieve device geometry into `NvmInfo` |
| `isBusy(busy)` | Poll whether a program/erase cycle is in progress |

### Blocking transfers

| Method | Description |
|---|---|
| `read(address, buffer, timeout_ms)` | Read bytes from the device |
| `write(address, data, timeout_ms)` | Write bytes to the device |
| `eraseBlock(address, timeout_ms)` | Erase the block containing `address` |
| `eraseChip(timeout_ms)` | Erase the entire device |

| Return code | Meaning |
|---|---|
| `AnsweredRequest` | Operation completed successfully |
| `ErrorParam` | Address or size out of device range |
| `ErrorReadFailed` / `ErrorWriteFailed` | Communication or device error |
| `ErrorTimeout` | Device did not respond within `timeout_ms` |
| `ErrorNotSupported` | Operation not applicable (e.g. erase on EEPROM) |
| `FunctionBusy` | A previous async operation is still running |

### Asynchronous operations

| Method | Description |
|---|---|
| `writeAsync(address, data)` | Begin async write; fires `WriteComplete` or `ErrorWrite` |
| `eraseBlockAsync(address)` | Begin async block erase; fires `EraseComplete` or `ErrorErase` |

> **Buffer lifetime:** the buffer passed to `writeAsync` must remain valid and unmodified until the `WriteComplete` (or `ErrorWrite`) callback fires — the DMA or peripheral controller reads directly from it after the call returns.

### Callback

```cpp
virtual void setCallback(NvmCallback callback) noexcept = 0;
```

Registers or replaces the event handler. Pass a default-constructed `NvmCallback` to unregister.

---

## Usage examples

### Reading from NOR Flash

```cpp
#include <hel_invm>

hel::NvmInfo info;
nvm.begin();
nvm.getInfo(info);

uint8_t buf[256];
const hel::ReturnCode rc = nvm.read(0x00000000U, hel::ByteArray(buf, sizeof(buf)), 100U);
if (rc != hel::ReturnCode::AnsweredRequest) { /* handle error */ }
```

### Erase then write (blocking)

```cpp
// Erase the first block
nvm.eraseBlock(0x00000000U, 5000U);

// Write a page
static const uint8_t firmware_page[256] = { /* ... */ };
nvm.write(0x00000000U, hel::ConstByteArray(firmware_page, sizeof(firmware_page)), 500U);
```

### Async erase with callback

```cpp
nvm.setCallback(hel::NvmCallback(this, &MyClass::onNvmEvent));
nvm.eraseBlockAsync(0x00010000U);

void MyClass::onNvmEvent(hel::NvmEvent event) noexcept
{
    if (event == hel::NvmEvent::EraseComplete)
    {
        // safe to write now
        nvm.writeAsync(0x00010000U, hel::ConstByteArray(m_buffer, m_size));
    }
    else if (event == hel::NvmEvent::ErrorErase)
    {
        // handle error
    }
}
```

### Checking device geometry before write

```cpp
hel::NvmInfo info;
nvm.getInfo(info);

// Align address to page boundary
const uint32_t page_addr = (target_addr / info.page_size) * info.page_size;
// Ensure the write fits within one page
if (data_size <= info.page_size)
{
    nvm.write(page_addr, hel::ConstByteArray(data, data_size), 200U);
}
```

---

## Implementing for a new target

Derive from `hel::iNvm` and map each virtual method to the target HAL or IC driver:

```cpp
class W25Q128 : public hel::iNvm
{
public:
    hel::ReturnCode begin() noexcept override
    {
        // verify JEDEC ID over SPI
    }

    hel::ReturnCode getInfo(hel::NvmInfo& info) const noexcept override
    {
        info = { hel::NvmType::Nor, 16U * 1024U * 1024U, 256U, 4096U, 1U };
        return hel::ReturnCode::AnsweredRequest;
    }

    hel::ReturnCode eraseBlock(uint32_t address, uint32_t timeout_ms) noexcept override
    {
        // send Sector Erase (0x20) command over SPI, then poll WIP bit
    }

    // ... remaining methods

protected:
    void handleEvent(hel::NvmEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::NvmCallback m_callback;
};
```

---

## Thread-safety and ISR constraints

- All methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- Blocking methods (`read`, `write`, `eraseBlock`, `eraseChip`) must **not** be called from ISR context.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context (implementation-defined); they must not call functions that disable interrupts for extended periods.
- The source buffer passed to `writeAsync` must remain valid until the completion callback fires.
