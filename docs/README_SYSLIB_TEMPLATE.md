# `ComponentName` — Short description

> One-sentence summary of the component's purpose and main use case.

<!--
  Suggested image: memory layout or usage diagram.
  Recommended size: 700×250 px.

  ![Memory layout](../../docs/img/component_name_layout.png)
-->

---

## Features

- Feature 1 (e.g. fixed-capacity, no heap allocation)
- Feature 2 (e.g. thread-safe concurrent reads)
- Feature 3 (e.g. `constexpr`-compatible construction)
- Self-contained — no dependency on `api/` or `target/`

---

## Header

```cpp
#include <hel_componentname>
```

---

## Template parameters

| Parameter | Description | Constraint |
|---|---|---|
| `T` | Element type | Must be trivially copyable |
| `N` | Maximum capacity | Must be > 0 |

---

## Types

| Type | Description |
|---|---|
| `ComponentStatus` | Return type for operations that can fail |

### `ComponentStatus`

| Value | Description |
|---|---|
| `Ok` | Operation completed successfully |
| `Full` | Container is at capacity |
| `Empty` | Container has no elements |

---

## API

### Construction

```cpp
// Stack-allocated, fixed capacity
hel::ComponentName<int, 16> c;
```

### Modifiers

| Method | Description | Returns |
|---|---|---|
| `push(value)` | Insert element at the back | `ComponentStatus` |
| `pop()` | Remove and return element at the front | `T` |
| `clear()` | Remove all elements | `void` |

### Observers

| Method | Description | Returns |
|---|---|---|
| `size()` | Number of elements currently held | `std::size_t` |
| `capacity()` | Maximum number of elements | `std::size_t` |
| `empty()` | `true` when size is 0 | `bool` |
| `full()` | `true` when size equals capacity | `bool` |

---

## Usage examples

### Basic usage

```cpp
hel::ComponentName<int, 8> c;

c.push(10);
c.push(20);

const int v = c.pop();   // v == 10
```

### Checking capacity before insert

```cpp
if (!c.full())
{
    c.push(value);
}
```

### Iterating

```cpp
for (const auto& item : c)
{
    process(item);
}
```

---

## Notes

- All operations are O(1) unless stated otherwise.
- No exceptions are thrown; failures are reported via `ComponentStatus`.
- Concurrent reads on the same instance are safe. Concurrent writes must be serialized by the caller.
