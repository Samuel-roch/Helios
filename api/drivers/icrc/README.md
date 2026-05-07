# iCrc — CRC Engine Interface

`hel::iCrc` is the hardware-agnostic interface for CRC calculation engines. All operations are synchronous — the hardware unit completes before the method returns.

## API Summary

| Method | Description |
|---|---|
| `compute(data, crc)` | Full-buffer CRC: resets accumulator, processes all bytes, returns result |
| `accumulate(data, crc)` | Streaming CRC: feeds bytes into the running state without resetting |
| `reset()` | Resets the accumulator to its initial value |

Return codes follow the `hel::ReturnCode` convention: values below `0x0100` are informational; `0x0100` and above indicate errors.

### `compute` vs `accumulate`

| | `compute` | `accumulate` |
|---|---|---|
| Resets accumulator first | Yes | No |
| Use when | Processing a single self-contained buffer | Processing data in chunks |
| Typical usage | One-shot packet integrity check | Chunked transfer, scatter-gather |

---

## Implementing a Concrete Driver

### 1. Inherit from `iCrc`

```cpp
#include <hel_icrc>

class Stm32Crc final : public hel::iCrc
{
public:
    explicit Stm32Crc(CRC_HandleTypeDef& hcrc) noexcept : m_hcrc(hcrc) {}

    hel::ReturnCode compute(hel::ConstByteArray data, uint32_t& crc) noexcept override;
    hel::ReturnCode accumulate(hel::ConstByteArray data, uint32_t& crc) noexcept override;
    void reset() noexcept override;

private:
    CRC_HandleTypeDef& m_hcrc;
};
```

> **Singleton rule**: CRC peripheral objects are owned by the BSP. The deleted `operator=` enforces this at compile time — never copy or move an `iCrc` instance.

---

### 2. Implement `compute`

Reset the accumulator, feed the entire buffer, return the result.

```cpp
hel::ReturnCode Stm32Crc::compute(hel::ConstByteArray data, uint32_t& crc) noexcept
{
    __HAL_RCC_CRC_FORCE_RESET();
    __HAL_RCC_CRC_RELEASE_RESET();

    const uint32_t result = HAL_CRC_Calculate(
        &m_hcrc,
        // STM32 HAL expects uint32_t* — the CRC unit reads word-by-word internally
        reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(data.data())),
        static_cast<uint32_t>(data.size()));

    if (HAL_CRC_GetState(&m_hcrc) == HAL_CRC_STATE_ERROR)
        return hel::ReturnCode::ErrorGeneral;

    crc = result;
    return hel::ReturnCode::AnsweredRequest;
}
```

**Rules:**
- Always reset the accumulator before feeding data — `compute` must leave `crc` unmodified on failure.
- The HAL may require the buffer to be word-aligned depending on the input data format configured (byte, half-word, or word). Document this constraint in the BSP.
- On STM32, `HAL_CRC_Calculate` resets the peripheral internally; explicit reset via RCC is only needed if recovering from a fault state.

---

### 3. Implement `accumulate`

Feed data into the running state without resetting.

```cpp
hel::ReturnCode Stm32Crc::accumulate(hel::ConstByteArray data, uint32_t& crc) noexcept
{
    const uint32_t result = HAL_CRC_Accumulate(
        &m_hcrc,
        reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(data.data())),
        static_cast<uint32_t>(data.size()));

    if (HAL_CRC_GetState(&m_hcrc) == HAL_CRC_STATE_ERROR)
        return hel::ReturnCode::ErrorGeneral;

    crc = result;
    return hel::ReturnCode::AnsweredRequest;
}
```

**Rules:**
- Caller must call `reset()` before the first `accumulate` in a new sequence; this method never resets on its own.
- `crc` receives the intermediate running value after each chunk — it is valid even before the final chunk.
- `crc` must not be modified if an error occurs.

---

### 4. Implement `reset`

```cpp
void Stm32Crc::reset() noexcept
{
    __HAL_RCC_CRC_FORCE_RESET();
    __HAL_RCC_CRC_RELEASE_RESET();
}
```

`reset` has no return value because a register write cannot fail on STM32. If your target's reset can fail, wrap it in `accumulate` state tracking rather than adding a return value (that would break the interface contract).

---

## Usage Examples

### One-shot computation

```cpp
Stm32Crc crc(hcrc);

uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
hel::ConstByteArray view(payload, sizeof(payload));

uint32_t result{};
if (crc.compute(view, result) == hel::ReturnCode::AnsweredRequest)
{
    // result holds the CRC of the entire payload
}
```

### Chunked (streaming) computation

```cpp
crc.reset(); // mandatory before first accumulate

uint32_t running{};
for (const auto& chunk : chunks)
{
    if (crc.accumulate(hel::ConstByteArray(chunk.data(), chunk.size()), running)
        != hel::ReturnCode::AnsweredRequest)
    {
        // handle error
        break;
    }
}
// running now holds the CRC over all chunks concatenated
```

---

## Checklist for New Implementations

- [ ] `compute` always resets the accumulator before processing, regardless of prior state
- [ ] `compute` leaves `crc` unmodified on error
- [ ] `accumulate` never resets the accumulator
- [ ] `accumulate` leaves `crc` unmodified on error
- [ ] `reset` is unconditionally `noexcept` with no return value
- [ ] Buffer alignment requirements (if any) are documented in the BSP
- [ ] No dynamic memory allocation anywhere in the implementation
- [ ] All methods are `noexcept`
