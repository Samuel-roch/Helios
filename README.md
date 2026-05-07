# Helios

> Cross-platform C++17 embedded SDK — hardware-agnostic drivers, RTOS-independent kernel interfaces, and allocation-free data structures.

---

<!--
  Suggested image: architecture overview diagram showing the four layers
  (Application → include/ → api/ → target/) with syslib/utils as satellite.
  Recommended size: 900×400 px, transparent or white background.

  ![Architecture](docs/img/architecture.png)
-->

## Overview

Helios provides a uniform API across STM32, ESP32, NXP, and other microcontroller families. The same application code compiles and runs on any supported target — or on a Qt desktop host for simulation and testing.

**Design constraints:**

- No dynamic memory — all containers use compile-time fixed capacity
- No exceptions — errors returned as `ReturnCode`
- No RTOS dependency — kernel interfaces abstract over FreeRTOS, ThreadX, Zephyr, and others
- MISRA C++ 2023 oriented — strong typing, `noexcept`, `[[nodiscard]]`, no implicit casts

---

## Architecture

```
┌─────────────────────────────────────────┐
│              Application                │
└────────────────────┬────────────────────┘
                     │
┌────────────────────▼────────────────────┐
│          include/  (hel_* headers)      │  ← public API entry point
└────────────────────┬────────────────────┘
                     │
          ┌──────────▼──────────┐
          │       api/          │  ← abstract interfaces (pure virtual)
          │  drivers/           │
          │  libraries/         │
          └──────────┬──────────┘
                     │
          ┌──────────▼──────────┐
          │      target/        │  ← platform bindings (STM32, ESP32, Qt…)
          └─────────────────────┘

   syslib/   ──  allocation-free data structures (no api/ dependency)
   utils/    ──  FSM, CLI (no api/ dependency)
```

---

## Modules

### Driver interfaces — `api/drivers/`

| Interface | Header | Description |
|---|---|---|
| `iUart` | `hel_iuart` | UART — blocking, interrupt, DMA, circular |
| `iSpi` | `hel_ispi` | SPI master/slave |
| `iI2c` | `hel_ii2c` | I2C master |
| `iAdc` | `hel_iadc` | Analog-to-digital converter |
| `iGpio` | `hel_igpio` | Digital I/O, edge interrupts |
| `iPwm` | `hel_ipwm` | PWM output |
| `iTimer` | `hel_itimer` | Hardware timer — one-shot and periodic |
| `iCrc` | `hel_icrc` | Hardware CRC engine |
| `iRtc` | `hel_irtc` | Real-time clock |
| `iPower` | `hel_ipower` | Sleep modes, software reset |
| `iWdg` | `hel_iwdg` | Independent / window watchdog |
| `iQspi` | `hel_iqspi` | Quad-SPI flash |
| `iSdio` | `hel_isdio` | SD/MMC card |
| `iPcnt` | `hel_ipcnt` | Hardware pulse counter |
| `iUsbDevice` | `hel_iusb_device` | USB device-mode stack |
| `iUsbHost` | `hel_iusb_host` | USB host-mode stack |

### Kernel interfaces — `api/libraries/kernel/`

| Interface | Header | Description |
|---|---|---|
| `iKernel` | `hel_ikernel` | Scheduler lifecycle, tick, critical section |
| `iTask` | `hel_itask` | Task creation and lifecycle |
| `iMutex` | `hel_imutex` | Mutual exclusion lock |
| `iSemaphore` | `hel_isemaphore` | Counting and binary semaphore |
| `iQueue` | `hel_iqueue` | Inter-task message queue |

### System library — `syslib/`

| Type | Header | Description |
|---|---|---|
| `ByteArray` / `ConstByteArray` | `hel_bytearray` | Non-owning byte buffer views, endian-aware read/write |
| `String` | `hel_string` | Fixed-capacity null-terminated string, printf/scanf, numeric formatting |
| `StringList<N, L>` | `hel_stringlist` | Fixed-capacity list of fixed-capacity strings |
| `Span<T, Extent>` | `hel_span` | Non-owning view over a contiguous sequence |
| `RingBuffer<T, N>` | `hel_ringbuffer` | Circular buffer |
| `LinkedList<T>` | `hel_linkedlist` | Intrusive singly-linked list |
| `Callback<Ret(Args...)>` | `hel_callback` | Allocation-free type-erased callable |
| `Calendar` | `hel_calendar` | Date and time value type |
| `Atomic<T>` | `hel_atomic` | Atomic operations wrapper |

### Utilities — `utils/`

| Component | Header | Description |
|---|---|---|
| `Fsm<T, N>` | `hel_fsm` | Generic finite-state machine |
| `Cli` | `hel_cli` | Command-line interface parser |

---

## Platform support

| Target | Activation | Families |
|---|---|---|
| STM32 | STM32 HAL headers present | F4, H7, L4, WB, and others |
| ESP32 | ESP-IDF headers present | ESP32, ESP32-S3, ESP32-C3 |
| NXP | NXP SDK headers present | Kinetis |
| Linux | POSIX headers present | — |
| Qt | `HeliosQt` preprocessor define | Desktop simulation and unit testing |

Target selection is automatic via `__has_include()` — no manual configuration needed.

---

## Quick start

### Using a driver

```cpp
#include <hel_iuart>
#include <hel_string>

// Implement the interface for your target
class MyUart : public hel::iUart { /* ... */ };

MyUart uart;
hel::StringData<64> msg;
msg.printf("Hello at %lu Hz\n", baudrate);

const hel::ReturnCode rc = uart.write(msg, 100U);
if (rc != hel::ReturnCode::AnsweredRequest) { /* handle error */ }
```

### Implementing a driver

```cpp
#include <hel_iuart>

class Stm32Uart : public hel::iUart
{
public:
    hel::ReturnCode write(hel::ConstByteArray data, uint32_t timeout_ms) noexcept override
    {
        const HAL_StatusTypeDef s = HAL_UART_Transmit(
            &m_handle, data.data(), static_cast<uint16_t>(data.size()),
            timeout_ms);
        return (s == HAL_OK) ? hel::ReturnCode::AnsweredRequest
                             : hel::ReturnCode::ErrorGeneral;
    }
    // ... other methods
};
```

### Using syslib independently

```cpp
#include <hel_bytearray>
#include <hel_ringbuffer>

uint8_t raw[256];
hel::ByteArray buf(raw, sizeof(raw));
buf.write_le<uint32_t>(0xDEADBEEF);

hel::RingBuffer<uint8_t, 32> rb;
rb.push(0xAAU);
```

---

## Requirements

| Item | Minimum |
|---|---|
| C++ standard | C++17 |
| Compiler | GCC 8+, Clang 9+, or MSVC 19.14+ |
| Dependencies | None — target bindings require the respective platform SDK |

---

## Documentation

```bash
cd doxygen
doxygen Doxyfile
```

API documentation is generated with Doxygen. All public headers follow the comment standard defined in [`doxygen/doxygen.md`](doxygen/doxygen.md).

---

## Repository layout

```
helios/
├── api/
│   ├── drivers/        # Abstract peripheral driver interfaces
│   ├── devices/        # High-level device abstractions (charger, fuel gauge)
│   └── libraries/      # Kernel, filesystem, and network abstractions
├── syslib/             # Allocation-free data structures
├── utils/              # FSM and CLI utilities
├── target/             # Platform-specific HAL bindings
├── include/            # Public convenience headers (hel_*)
└── doxygen/            # Doxygen configuration and group definitions
```
