# Stream

> Abstract byte stream interface — transport-agnostic sequential read and write over any underlying channel.

<!--
  Suggested image: block diagram showing application / protocol parser →
  iStream interface → concrete implementations (UartStream, UsbCdcStream, TcpStream).
  Recommended size: 900×280 px.

  ![Stream block diagram](../../../docs/img/stream_diagram.png)
-->

---

## Features

- Bidirectional sequential byte I/O — read, readByte, write, writeByte, flush
- Byte-available query for polling (`available`)
- Open/close lifecycle
- Asynchronous event delivery via `StreamCallback` (DataAvailable, WriteComplete, Error, Closed)
- Read-only or write-only implementations return `ErrorNotSupported` for the unsupported direction
- No dynamic allocation; all buffers are caller-owned

---

## Header

```cpp
#include <hel_istream>
```

---

## Types

### `StreamEvent`

| Value | Description |
|---|---|
| `DataAvailable` | New bytes arrived and are ready to read |
| `WriteComplete` | A pending write or flush completed |
| `Error` | An I/O error occurred on the stream |
| `Closed` | The stream was closed (locally or by the remote side) |

---

## API

### State

| Method | Description |
|---|---|
| `available(count)` | Number of bytes immediately available to read |
| `isOpen(open)` | Whether the stream is open and ready for I/O |
| `close()` | Close the stream and release transport resources |

### Read

| Method | Description |
|---|---|
| `read(buffer, bytes_read)` | Read up to `buffer.size()` bytes |
| `readByte(byte)` | Read exactly one byte |

`read` returns `ErrorQueueEmpty` (not an error — just nothing available yet) when the receive buffer is empty.

### Write

| Method | Description |
|---|---|
| `write(data)` | Write all bytes in `data`; may buffer internally |
| `writeByte(byte)` | Write exactly one byte |
| `flush()` | Push any buffered bytes to the underlying transport |

### Callback

| Method | Description |
|---|---|
| `setCallback(callback)` | Register handler for `StreamEvent` notifications |

---

## Usage examples

### Polling loop

```cpp
#include <hel_istream>

uint8_t raw[64];
hel::ByteArray buf(raw, sizeof(raw));

uint32_t n = 0U;
if (stream.read(buf, n) == hel::ReturnCode::AnsweredRequest && n > 0U)
{
    processBytes(raw, n);
}
```

### Interrupt-driven receive

```cpp
stream.setCallback(hel::StreamCallback(this, &Protocol::onStreamEvent));

void Protocol::onStreamEvent(hel::StreamEvent event) noexcept
{
    if (event == hel::StreamEvent::DataAvailable)
    {
        uint32_t n = 0U;
        uint8_t raw[64];
        m_stream.read(hel::ByteArray(raw, sizeof(raw)), n);
        for (uint32_t i = 0U; i < n; ++i)
        {
            m_parser.feed(raw[i]);
        }
    }
    else if (event == hel::StreamEvent::Error)
    {
        m_stream.close();
    }
}
```

### Writing a framed message

```cpp
static const uint8_t msg[] = { 0xAA, 0x01, 0xFF, 0x55 };
stream.write(hel::ConstByteArray(msg, sizeof(msg)));
stream.flush();
```

---

## Implementing for a new target

Map each virtual method to the underlying transport (UART, USB CDC, TCP, etc.):

```cpp
class UartStream : public hel::iStream
{
public:
    hel::ReturnCode read(hel::ByteArray buffer, uint32_t& bytes_read) noexcept override
    {
        bytes_read = m_rxRing.read(buffer.data(), buffer.size());
        return (bytes_read > 0U) ? hel::ReturnCode::AnsweredRequest
                                 : hel::ReturnCode::ErrorQueueEmpty;
    }

    hel::ReturnCode write(hel::ConstByteArray data) noexcept override
    {
        // copy to TX ring buffer; DMA or TX ISR drains it
        const bool ok = m_txRing.write(data.data(), data.size());
        return ok ? hel::ReturnCode::AnsweredRequest
                  : hel::ReturnCode::ErrorBufferTooLarge;
    }

    hel::ReturnCode flush() noexcept override
    {
        startDmaTx();  // kick DMA transfer from TX ring
        return hel::ReturnCode::AnsweredRequest;
    }

    // Called from UART RX ISR:
    void onRxIsr(uint8_t byte) noexcept
    {
        m_rxRing.writeByte(byte);
        handleEvent(hel::StreamEvent::DataAvailable);
    }

    // ... remaining methods

protected:
    void handleEvent(hel::StreamEvent event) noexcept override
    {
        m_callback(event);
    }

private:
    hel::StreamCallback m_callback;
    RingBuffer<uint8_t, 256> m_rxRing;
    RingBuffer<uint8_t, 256> m_txRing;
};
```

---

## Thread-safety and ISR constraints

- All public methods are **not** thread-safe on the same instance; the caller must serialize concurrent access.
- Reentrant across **distinct** instances.
- `handleEvent` is intended for ISR or driver-task context only — `noexcept`, must not block.
- Callbacks fire from ISR context; they must not disable interrupts for extended periods.
- `read` inside a `DataAvailable` callback is safe — the ISR will have already placed the bytes into the receive buffer before firing the event.
