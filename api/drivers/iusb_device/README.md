# iUsbDevice — USB Device Stack Interface

> **Status: placeholder** — `iUsbDevice` is an empty stub. The API design is pending.

`hel::iUsbDevice` will be the hardware-agnostic interface for USB device-mode operation. It will abstract USB class drivers (CDC, HID, MSC, etc.) behind a common interface.

---

## Planned API (not yet defined)

The expected capabilities depend on the USB class being abstracted. Common operations:

| Operation | Description |
|---|---|
| `start()` | Connect to the USB host (enable pull-up / D+ signaling) |
| `stop()` | Disconnect from the USB host |
| `send(ep, data)` | Transmit data on an endpoint |
| `setReceiveCallback(ep, cb)` | Register a callback for incoming data on an endpoint |
| `getState()` | Query connection state (disconnected, enumerated, suspended) |

---

## Implementing this Interface

Until the API is defined, do not inherit from `iUsbDevice` for production code. The final interface will follow Helios driver conventions:

- All methods return `hel::ReturnCode`
- Async operations deliver results via `hel::Callback<>`
- No dynamic allocation
- All methods `noexcept`

---

## Interim Alternative

Use the STM32 USB device library (USB_Device middleware) directly:

```cpp
// CDC class: transmit data to host
CDC_Transmit_FS(buffer, length);
```

Wrap in a BSP-local class and migrate to `iUsbDevice` once the interface is stable.
