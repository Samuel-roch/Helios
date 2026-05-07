# iUsbHost — USB Host Stack Interface

> **Status: placeholder** — `iUsbHost` is an empty stub. The API design is pending.

`hel::iUsbHost` will be the hardware-agnostic interface for USB host-mode operation. It will abstract device enumeration and class-specific communication (HID, MSC, CDC-ACM, etc.).

---

## Planned API (not yet defined)

| Operation | Description |
|---|---|
| `start()` | Start host controller and VBUS power |
| `stop()` | Stop host controller and cut VBUS |
| `getDeviceState()` | Query device connection/enumeration state |
| `send(pipe, data)` | Transmit data through a pipe |
| `receive(pipe, buffer)` | Receive data through a pipe |
| `setEventCallback(cb)` | Register callback for connect/disconnect/error events |

---

## Implementing this Interface

Until the API is defined, do not inherit from `iUsbHost` for production code. The final interface will follow Helios driver conventions:

- All methods return `hel::ReturnCode`
- Async operations deliver results via `hel::Callback<>`
- No dynamic allocation
- All methods `noexcept`

---

## Interim Alternative

Use the STM32 USB host library (USB_Host middleware) directly:

```cpp
// HID class: poll for report
HID_MOUSE_Info_TypeDef* info = USBH_HID_GetMouseInfo(&hUsbHostFS);
```

Wrap in a BSP-local class and migrate to `iUsbHost` once the interface is stable.
