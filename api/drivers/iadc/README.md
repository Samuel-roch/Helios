# iAdc — ADC Driver Interface

`hel::iAdc` is the hardware-agnostic interface for analog-to-digital converters. It supports blocking single-channel reads and asynchronous multi-channel DMA scans.

## API Summary

| Method | Description |
|---|---|
| `read(channel, value, timeout_ms)` | Blocking single-channel conversion |
| `startDMA(buffer)` | Start multi-channel DMA scan |
| `stopDMA()` | Abort an ongoing DMA scan |
| `setCallback(cb)` | Register the async event callback |

Return codes follow the `hel::ReturnCode` convention: values below `0x0100` are informational; `0x0100` and above indicate errors.

---

## Implementing a Concrete Driver

### 1. Inherit from `iAdc`

```cpp
#include <hel_iadc>

class Stm32Adc final : public hel::iAdc
{
public:
    explicit Stm32Adc(ADC_HandleTypeDef& hadc) noexcept : m_hadc(hadc) {}

    hel::ReturnCode read(uint32_t channel, uint16_t& value,
                         uint32_t timeout_ms) noexcept override;

    hel::ReturnCode startDMA(hel::Span<uint16_t> buffer) noexcept override;
    hel::ReturnCode stopDMA() noexcept override;
    void setCallback(hel::AdcCallback callback) noexcept override;

protected:
    void handleEvent(hel::AdcEvent event, uint16_t count) noexcept override;

private:
    ADC_HandleTypeDef& m_hadc;
    hel::AdcCallback m_callback{};
};
```

> **Singleton rule**: ADC peripheral objects are owned by the BSP. Never copy or move an `iAdc` instance — the deleted `operator=` enforces this at compile time.

---

### 2. Implement `read` (blocking)

Select the channel, start the conversion, wait for the HAL, then read the result.

```cpp
hel::ReturnCode Stm32Adc::read(uint32_t channel, uint16_t& value,
                                uint32_t timeout_ms) noexcept
{
    // Reject if a DMA scan is in progress
    if (HAL_ADC_GetState(&m_hadc) & HAL_ADC_STATE_REG_BUSY)
        return hel::ReturnCode::FunctionBusy;

    ADC_ChannelConfTypeDef cfg{};
    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_28CYCLES; // adjust per channel impedance

    if (HAL_ADC_ConfigChannel(&m_hadc, &cfg) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    if (HAL_ADC_Start(&m_hadc) != HAL_OK)
        return hel::ReturnCode::ErrorGeneral;

    const HAL_StatusTypeDef s = HAL_ADC_PollForConversion(&m_hadc, timeout_ms);
    HAL_ADC_Stop(&m_hadc);

    if (s == HAL_TIMEOUT) return hel::ReturnCode::ErrorTimeout;
    if (s != HAL_OK)      return hel::ReturnCode::ErrorGeneral;

    value = static_cast<uint16_t>(HAL_ADC_GetValue(&m_hadc));
    return hel::ReturnCode::AnsweredRequest;
}
```

**Rules:**
- Never call `read` from an ISR context.
- Always stop the peripheral before returning on error — leave the hardware in a clean state.
- Return `FunctionBusy` (not `ErrorGeneral`) when a DMA scan is active.

---

### 3. Implement `startDMA`

```cpp
hel::ReturnCode Stm32Adc::startDMA(hel::Span<uint16_t> buffer) noexcept
{
    if (HAL_ADC_GetState(&m_hadc) & HAL_ADC_STATE_REG_BUSY)
        return hel::ReturnCode::FunctionBusy;

    const HAL_StatusTypeDef s =
        HAL_ADC_Start_DMA(&m_hadc,
                          reinterpret_cast<uint32_t*>(buffer.data()),
                          static_cast<uint32_t>(buffer.size()));

    return (s == HAL_OK) ? hel::ReturnCode::AnsweredRequest
                         : hel::ReturnCode::ErrorGeneral;
}
```

**Rules:**
- `buffer` must remain valid until the callback fires or `stopDMA` returns.
- The span size must match the number of enabled scan channels × number of scans configured in HAL. Mismatches cause silent buffer overflows — document this constraint in the BSP.
- Do not start a new DMA if one is already active; return `FunctionBusy`.

---

### 4. Implement `stopDMA`

```cpp
hel::ReturnCode Stm32Adc::stopDMA() noexcept
{
    if (!(HAL_ADC_GetState(&m_hadc) & HAL_ADC_STATE_REG_BUSY))
        return hel::ReturnCode::NotInitialized;

    HAL_ADC_Stop_DMA(&m_hadc);
    return hel::ReturnCode::AnsweredRequest;
}
```

> After `stopDMA` returns, the HAL fires the conversion-complete or error callback, which eventually calls `handleEvent`. The sample count delivered at that point is implementation-defined.

---

### 5. Implement `setCallback` and `handleEvent`

```cpp
void Stm32Adc::setCallback(hel::AdcCallback callback) noexcept
{
    m_callback = callback;
}

// Called from HAL interrupt callbacks (see section 6)
void Stm32Adc::handleEvent(hel::AdcEvent event, uint16_t count) noexcept
{
    m_callback(event, count);
}
```

---

### 6. Wire HAL Interrupt Callbacks

STM32 HAL calls global weak functions on DMA events. Override them to forward to `handleEvent`.

```cpp
// Defined in your BSP (e.g. stm32xx_it.cpp or adc_bsp.cpp)
extern Stm32Adc g_adc1;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        const auto count = static_cast<uint16_t>(hadc->DMA_Handle->Instance->NDTR);
        g_adc1.handleEvent(hel::AdcEvent::ConversionComplete, count);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
        g_adc1.handleEvent(hel::AdcEvent::Error, 0U);
}
```

> `handleEvent` is `protected` — only HAL callbacks and ISR forwarders should call it directly.

---

## Usage Example

```cpp
// BSP instantiation
Stm32Adc adc1(hadc1);

// Register callback once during init
adc1.setCallback(hel::AdcCallback(&myHandler, &MyClass::onAdcEvent)); // member function
// or: hel::AdcCallback(&freeFunction)                                 // free function

// Blocking read (task context only)
uint16_t raw{};
if (adc1.read(ADC_CHANNEL_0, raw, 10U) == hel::ReturnCode::AnsweredRequest)
{
    const float voltage = (raw / 4095.0F) * 3.3F;
}

// DMA multi-channel scan
uint16_t samples[4]{};
hel::Span<uint16_t> buf(samples, 4U);
if (adc1.startDMA(buf) == hel::ReturnCode::AnsweredRequest)
{
    // results available in samples[] when callback fires with AdcEvent::ConversionComplete
}
```

---

## Checklist for New Implementations

- [ ] `read` returns `FunctionBusy` when a DMA scan is active
- [ ] `startDMA` returns `FunctionBusy` when a conversion is already running
- [ ] `stopDMA` returns `NotInitialized` when nothing is active
- [ ] `handleEvent` is invoked for both success (`ConversionComplete`) and error paths
- [ ] `handleEvent` is called only from HAL callbacks or ISR forwarders, never directly by API callers
- [ ] Buffer passed to `startDMA` outlives the DMA transfer
- [ ] No dynamic memory allocation anywhere in the implementation
- [ ] All public methods are `noexcept`
