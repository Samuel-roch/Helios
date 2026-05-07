# iSdio — SD/MMC Card Interface

> **Status: placeholder** — `iSdio` is an empty stub. The API design is pending.

`hel::iSdio` will be the hardware-agnostic interface for SD and MMC card access over the SDIO/SDMMC peripheral.

---

## Planned API (not yet defined)

The following capabilities are expected in a future revision:

| Operation | Description |
|---|---|
| `init()` | Detect, identify, and initialize the inserted card |
| `read(block, buffer)` | Read one or more 512-byte blocks |
| `write(block, buffer)` | Write one or more 512-byte blocks |
| `erase(start, end)` | Erase a range of blocks |
| `getCardInfo(info)` | Query card capacity, type, and speed class |
| `setCallback(cb)` | Register async completion callback |

---

## Implementing this Interface

Until the API is defined, do not inherit from `iSdio` for production code. When the interface is finalized it will follow the same patterns as the other Helios drivers:

- All methods return `hel::ReturnCode`
- Async operations deliver results via `hel::Callback<>`
- No dynamic allocation
- All methods `noexcept`
- `handleEvent` protected, called only from ISR dispatch

---

## Interim Alternative

For immediate SD card access, drive the SDMMC peripheral directly via the STM32 HAL:

```cpp
// Blocking read via HAL (bypasses Helios abstraction)
HAL_SD_ReadBlocks(&hsd1, buffer, block_addr, num_blocks, timeout_ms);
```

Wrap this in a BSP-local class and migrate to `iSdio` once the interface is stable.
