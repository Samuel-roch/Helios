# iQspi — Quad-SPI Flash Memory Interface

`hel::iQspi` is the hardware-agnostic interface for external flash memory accessed via Quad-SPI (QSPI / OSPI). It covers blocking and DMA-based read, program, and erase operations.

## API Summary

| Method | Description |
|---|---|
| `read(addr, data, timeout_ms)` | Blocking read from flash |
| `write(addr, data, timeout_ms)` | Blocking program (write) to flash |
| `erase(addr, type, timeout_ms)` | Blocking erase (sector, block, or chip) |
| `readDMA(addr, data)` | Async DMA read |
| `writeDMA(addr, data)` | Async DMA program |
| `abort()` | Abort an ongoing async operation |
| `setCallback(cb)` | Register the async event callback |

The `QspiCallback` signature is `void handler(ReturnCode code, uint32_t count) noexcept`.

### Erase Granularity

| `EraseType` | Typical size | `addr` required |
|---|---|---|
| `Sector` | 4 KB | Yes — must be sector-aligned |
| `Block` | 32 or 64 KB | Yes — must be block-aligned |
| `Chip` | Entire device | No — ignored |

---

## Implementing a Concrete Driver

### 1. Inherit from `iQspi`

```cpp
#include <hel_iqspi>

class Stm32Qspi final : public hel::iQspi
{
public:
    explicit Stm32Qspi(QSPI_HandleTypeDef& hqspi) noexcept : m_hqspi(hqspi) {}

    hel::ReturnCode read(uint32_t addr, hel::ByteArray data,
                         uint32_t timeout_ms) noexcept override;
    hel::ReturnCode write(uint32_t addr, hel::ConstByteArray data,
                          uint32_t timeout_ms) noexcept override;
    hel::ReturnCode erase(uint32_t addr, hel::EraseType type,
                          uint32_t timeout_ms) noexcept override;
    hel::ReturnCode readDMA(uint32_t addr, hel::ByteArray data) noexcept override;
    hel::ReturnCode writeDMA(uint32_t addr, hel::ConstByteArray data) noexcept override;
    hel::ReturnCode abort() noexcept override;
    void setCallback(hel::QspiCallback callback) noexcept override;

protected:
    void handleEvent(hel::ReturnCode code, uint32_t count) noexcept override;

private:
    QSPI_HandleTypeDef& m_hqspi;
    hel::QspiCallback   m_callback{};
};
```

---

### 2. Implement `read` (blocking)

```cpp
hel::ReturnCode Stm32Qspi::read(uint32_t addr, hel::ByteArray data,
                                  uint32_t timeout_ms) noexcept
{
    if (HAL_QSPI_GetState(&m_hqspi) != HAL_QSPI_STATE_READY)
        return hel::ReturnCode::FunctionBusy;

    QSPI_CommandTypeDef cmd{};
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction       = 0xEBU; // Quad Output Fast Read (device-specific)
    cmd.AddressMode       = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.Address           = addr;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.NbData            = static_cast<uint32_t>(data.size());
    cmd.DummyCycles       = 6U;

    if (HAL_QSPI_Command(&m_hqspi, &cmd, timeout_ms) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    return (HAL_QSPI_Receive(&m_hqspi, data.data(), timeout_ms) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}
```

---

### 3. Implement `write` (blocking program)

```cpp
hel::ReturnCode Stm32Qspi::write(uint32_t addr, hel::ConstByteArray data,
                                   uint32_t timeout_ms) noexcept
{
    if (HAL_QSPI_GetState(&m_hqspi) != HAL_QSPI_STATE_READY)
        return hel::ReturnCode::FunctionBusy;

    // Issue Write Enable latch before programming (device-specific)
    QSPI_CommandTypeDef wen{};
    wen.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    wen.Instruction     = 0x06U; // WREN
    if (HAL_QSPI_Command(&m_hqspi, &wen, timeout_ms) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    QSPI_CommandTypeDef cmd{};
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction     = 0x32U; // Quad Page Program
    cmd.AddressMode     = QSPI_ADDRESS_1_LINE;
    cmd.AddressSize     = QSPI_ADDRESS_24_BITS;
    cmd.Address         = addr;
    cmd.DataMode        = QSPI_DATA_4_LINES;
    cmd.NbData          = static_cast<uint32_t>(data.size());

    if (HAL_QSPI_Command(&m_hqspi, &cmd, timeout_ms) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    return (HAL_QSPI_Transmit(&m_hqspi,
                               const_cast<uint8_t*>(data.data()),
                               timeout_ms) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorGeneral;
}
```

**Rules:**
- Flash pages must be erased to `0xFF` before programming. Writing to a non-erased page produces undefined data.
- `addr` must be page-aligned (typically 256-byte pages). Document the alignment requirement in the BSP.

---

### 4. Implement `erase`

```cpp
hel::ReturnCode Stm32Qspi::erase(uint32_t addr, hel::EraseType type,
                                   uint32_t timeout_ms) noexcept
{
    if (HAL_QSPI_GetState(&m_hqspi) != HAL_QSPI_STATE_READY)
        return hel::ReturnCode::FunctionBusy;

    uint8_t instruction{};
    switch (type)
    {
    case hel::EraseType::Sector: instruction = 0x20U; break; // Sector Erase 4KB
    case hel::EraseType::Block:  instruction = 0xD8U; break; // Block Erase 64KB
    case hel::EraseType::Chip:   instruction = 0x60U; break; // Chip Erase
    }

    // Write Enable before erase
    QSPI_CommandTypeDef wen{};
    wen.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    wen.Instruction     = 0x06U;
    if (HAL_QSPI_Command(&m_hqspi, &wen, timeout_ms) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    QSPI_CommandTypeDef cmd{};
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction     = instruction;
    if (type != hel::EraseType::Chip)
    {
        cmd.AddressMode = QSPI_ADDRESS_1_LINE;
        cmd.AddressSize = QSPI_ADDRESS_24_BITS;
        cmd.Address     = addr;
    }

    if (HAL_QSPI_Command(&m_hqspi, &cmd, timeout_ms) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    // Poll WIP (Write In Progress) bit until erase finishes
    QSPI_AutoPollingTypeDef poll{};
    poll.Match           = 0x00U;
    poll.Mask            = 0x01U; // WIP bit
    poll.MatchMode       = QSPI_MATCH_MODE_AND;
    poll.Interval        = 0x10U;
    poll.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    QSPI_CommandTypeDef rdsr{};
    rdsr.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    rdsr.Instruction     = 0x05U; // RDSR
    rdsr.DataMode        = QSPI_DATA_1_LINE;

    return (HAL_QSPI_AutoPolling(&m_hqspi, &rdsr, &poll, timeout_ms) == HAL_OK)
        ? hel::ReturnCode::AnsweredRequest
        : hel::ReturnCode::ErrorTimeout;
}
```

> Erase is the slowest flash operation. Sector erase: ~30–100 ms; block erase: ~150–500 ms; chip erase: seconds. Size `timeout_ms` accordingly.

---

### 5. Implement async DMA and wire callbacks

```cpp
void Stm32Qspi::setCallback(hel::QspiCallback callback) noexcept
{
    m_callback = callback;
}

void Stm32Qspi::handleEvent(hel::ReturnCode code, uint32_t count) noexcept
{
    m_callback(code, count);
}

// BSP:
extern Stm32Qspi g_flash;

void HAL_QSPI_RxCpltCallback(QSPI_HandleTypeDef*)
{
    g_flash.handleEvent(hel::ReturnCode::AnsweredRequest, /* bytes */ 0U);
}

void HAL_QSPI_TxCpltCallback(QSPI_HandleTypeDef*)
{
    g_flash.handleEvent(hel::ReturnCode::AnsweredRequest, 0U);
}

void HAL_QSPI_ErrorCallback(QSPI_HandleTypeDef*)
{
    g_flash.handleEvent(hel::ReturnCode::ErrorGeneral, 0U);
}
```

---

## Checklist for New Implementations

- [ ] `write` sends Write Enable (WREN) before every page program
- [ ] `erase` sends Write Enable before every erase command
- [ ] `erase(Chip, ...)` ignores the `addr` parameter
- [ ] Blocking operations return `FunctionBusy` if an async operation is active
- [ ] Buffer passed to async methods outlives the DMA transfer
- [ ] `abort` returns `NotInitialized` when nothing is active
- [ ] `handleEvent` is called for both completion and error paths
- [ ] All public methods are `noexcept`
