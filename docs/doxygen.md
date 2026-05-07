# Padrões de Comentários Doxygen — Helios SDK

Este guia define os padrões de documentação Doxygen para o projeto Helios. Siga-o em todos os arquivos públicos para garantir consistência na documentação gerada e conformidade com MISRA C++.

> **Resumo:** Use `@brief` curto + `@details` em bullets. Documente todos os parâmetros, retornos, efeitos colaterais, thread-safety, timeouts e semântica de erros via `ReturnCode`. Sem exceções.

Os grupos Doxygen do projeto são definidos no arquivo [`groups.dox`](groups.dox). **Não redefina grupos nos headers** — apenas use `@ingroup`.

---

## 1) Cabeçalho de Arquivo

Todo arquivo deve começar com o bloco de cabeçalho padronizado. Use o snippet do seu IDE para agilizar.

**Campos obrigatórios:** `@file`, `@author`, `@version`, `@date`, `@ingroup`, `@brief`, e o bloco de licença.
O campo `@defgroup` **não deve ser usado nos headers** — os grupos são definidos exclusivamente em [`groups.dox`](groups.dox).

```c
/**
 ******************************************************************************
 * @file    <module_name>.hpp
 * @author  Samuel Almeida Rocha 
 * @version 1.0.0
 * @date    <YYYY-MM-DD>
 * @ingroup <GROUP_ID>
 * @brief   <Short one-line description.>
 *
 * @details
 *   - <Key behavior or constraint 1>
 *   - <Key behavior or constraint 2>
 *
 * @note
 *   <Important usage note, e.g., thread-safety or lifetime requirements.>
 *

 */
```

---

## 2) Grupos e Navegação

Os grupos organizam a API em módulos navegáveis na documentação gerada.

- Todos os grupos são **definidos uma única vez** em [`groups.dox`](groups.dox).
- Nos headers, use apenas `@ingroup` para associar o elemento a um grupo existente.
- Use `@addtogroup` apenas para adicionar documentação extra a um grupo já definido.
- `@{` e `@}` delimitam o escopo do grupo.

**Nos headers apenas `@ingroup`:**

```c
/**
 * @brief  GPIO driver interface.
 * @ingroup HELIOS_DRV_GPIO
 */
class iGpio { /* ... */ };
```

**Em `groups.dox` — onde os grupos são definidos:**

```c
/** @defgroup HELIOS_DRV_GPIO GPIO Driver
 *  @ingroup  HELIOS_DRIVERS
 *  @brief    Digital I/O interface.
 */
```

---

## 3) Namespaces

Documente namespaces públicos uma única vez, com propósito e escopo.

```c++
/**
 * @namespace hel
 * @brief     Driver interfaces and thin hardware abstractions.
 * @details
 *   - Pure interfaces + minimal glue
 *   - Consistent error model via ReturnCode
 *   - No dynamic allocation
 */
namespace hel { /* ... */ }
```

---

## 4) Classes e Structs

Use `@brief` de uma linha e bullets em `@details`. Mencione invariantes, propriedade, tempo de vida e thread-safety.

```c++
/**
 * @brief   Buffered UART driver.
 * @details
 *   - Non-blocking TX via IRQ/DMA
 *   - RX ring buffer (lock-free)
 *   - No dynamic allocation after init
 * @ingroup HELIOS_DRV_UART
 * @note    Reentrant across distinct instances; individual methods are not thread-safe unless stated.
 */
class Uart final {
public:
  /**
   * @brief     Construct a UART driver bound to a hardware handle.
   * @param[in] handle   Opaque HAL handle (must remain valid for lifetime of this object).
   * @param[in] rx_buf   Non-null RX buffer memory.
   * @param[in] rx_size  RX buffer size in bytes (must be > 0).
   * @warning   Does not take ownership of buffers; caller manages memory lifetime.
   */
  Uart(UART_HandleTypeDef* handle, uint8_t* rx_buf, size_t rx_size) noexcept;
};
```

**Classes template** devem documentar os parâmetros com `@tparam`:

```c++
/**
 * @brief    Fixed-capacity ring buffer.
 * @tparam   T         Element type (TriviallyCopyable recommended).
 * @tparam   Capacity  Maximum number of elements (must be > 0).
 * @note     No exceptions; operations return ReturnCode on failure.
 * @ingroup  HELIOS_SYSLIB_QUEUE
 */
template<class T, size_t Capacity>
class RingBuffer { /* ... */ };
```

---

## 5) Enums

Use sempre `enum class` com tipo subjacente fixo. Forneça `@brief` para o enum e documente cada valor com `/*!< ... */` alinhado.

```c++
/**
 * @brief   GPIO interrupt trigger mode.
 * @ingroup HELIOS_DRV_GPIO
 */
enum class GpioIrqMode : uint8_t
{
  DISABLED = 0U, /*!< Interrupt disabled                               */
  RISING   = 1U, /*!< Trigger on rising edge                           */
  FALLING  = 2U, /*!< Trigger on falling edge                          */
  CHANGE   = 3U  /*!< Trigger on both rising and falling edges         */
};
```

Se o enum mapeia registradores de hardware, adicione uma `@details` com tabela ou notas sobre valores de bits.

---

## 6) Funções e Métodos

### 6.1 Onde colocar a documentação: interface vs. implementação

A documentação Doxygen de um método deve estar em **um único lugar**: onde o contrato é definido.

| Arquivo | Tipo | Comentário nos métodos |
|---|---|---|
| `i*.hpp` (interface / pure virtual) | Contrato | `/** @brief ... */` completo |
| `*.hpp` (implementação concreta) | Declaração | `//` simples — uma linha |
| `*.hpp` (método implementado inline) | Implementação | `/** @brief ... */` completo |
| `*.cpp` | Implementação | `/** @brief ... */` completo |

**Motivação:** a interface é o contrato que todos os consumidores lêem. A declaração na implementação é redundante — o Doxygen já herda a documentação da base virtual.

#### Interface (`igpio.hpp`) — Doxygen completo

```cpp
/**
 * @brief      Read the current logical level of the pin.
 * @param[out] state  `true` if the pin is high, `false` if low.
 * @return     ReturnCode
 *   - @ref ReturnCode::AnsweredRequest : Read successful; @p state is valid.
 *   - @ref ReturnCode::NotInitialized  : Pin object is in an invalid state.
 */
[[nodiscard]] virtual ReturnCode read(bool& state) const noexcept = 0;
```

#### Implementação concreta (`gpio.hpp`) — comentário simples

```cpp
// Read the current logical level of the pin.
[[nodiscard]] ReturnCode read(bool& level) const noexcept override;
```

#### Implementação (`gpio.cpp`) — Doxygen completo

```cpp
/**
 * @brief Read the current logical level of the pin.
 * @details Delegates to HAL_GPIO_ReadPin. Result is true for GPIO_PIN_SET.
 *
 * @param[out] level  Populated with the current pin state on success.
 * @return @ref ReturnCode::AnsweredRequest on success.
 * @return @ref ReturnCode::NotInitialized if @ref m_port is nullptr.
 */
ReturnCode Gpio::read(bool& level) const noexcept
{
  // ...
}
```

#### Exceção — método implementado inline no `.hpp`

Se a implementação está no próprio `.hpp` (template, `inline`, método curto), use Doxygen completo diretamente no `.hpp`:

```cpp
// Returns the currently configured interrupt trigger mode.
[[nodiscard]] GpioIrqMode interruptMode() const noexcept
{
  return m_interrupt_mode; // inline — Doxygen aqui seria aceitável
}
```

---

### 6.2 Regras Gerais

- Use `noexcept` em todas as funções (padrão Helios — sem exceções).
- Use `[[nodiscard]]` em funções que retornam `ReturnCode` ou valores que **devem** ser verificados.
- Use `@param[in]`, `@param[out]`, `@param[in,out]` conforme a direção do dado.
- Documente explicitamente semântica de consumo de buffers e comportamento de timeout.
- Documente thread-safety e reentrância quando relevante.

```c++
/**
 * @brief     Transmit a buffer over UART.
 * @param[in] buf   HEL_NONNULL pointer to data (caller must ensure size >= @p size).
 * @param[in] size  Number of bytes to transmit.
 * @return    ReturnCode (see @ref ReturnCode).
 * @warning   On non-OK return, no bytes were consumed.
 */
[[nodiscard]]
ReturnCode write(const uint8_t* buf, size_t size) noexcept;
```

### 6.2 Funções com Timeout

Sempre documente o que acontece quando o timeout expira: qual o estado do buffer, do hardware e do objeto.

```c++
/**
 * @brief      Receive bytes into a caller-provided buffer.
 * @param[out] buf         Non-null destination buffer (must hold at least @p size bytes).
 * @param[in]  size        Number of bytes requested.
 * @param[in]  timeout_ms  Maximum wait time in milliseconds.
 * @return     ReturnCode
 *   - ReturnCode::ANSWERED_REQUEST : All @p size bytes written to @p buf.
 *   - ReturnCode::ERR_TIMEOUT      : Deadline expired; no bytes were written.
 *   - ReturnCode::FUNCTION_BUSY    : Resource busy; no bytes were written.
 * @note       Reentrant across distinct Uart instances.
 * @warning    Buffer content is unspecified on error or timeout.
 */
[[nodiscard]]
ReturnCode read(uint8_t* buf, size_t size, uint32_t timeout_ms) noexcept;
```

### 6.3 Overloads e Templates

Documente o comportamento comum uma vez e indique as diferenças específicas de cada overload.

```c++
/**
 * @brief    Serialize a POD object and transmit it as raw bytes.
 * @tparam   T    TriviallyCopyable type.
 * @param[in] obj  Object to serialize via memcpy.
 * @return   ReturnCode (see @ref ReturnCode).
 * @note     Equivalent to write(reinterpret_cast<const uint8_t*>(&obj), sizeof(T)).
 */
template<class T>
[[nodiscard]]
ReturnCode write_pod(const T& obj) noexcept;
```

---

## 7) Modelo de Erros (ReturnCode)

Referencie o enum de erro global do projeto. Enumere sempre os casos de sucesso e os erros esperados em lista de bullets.

```c++
/**
 * @enum    ReturnCode
 * @brief   Standard operation status for Helios APIs.
 * @details
 *   - ANSWERED_REQUEST : Success; operation completed as requested.
 *   - FUNCTION_BUSY    : Resource is busy; no progress was made.
 *   - ERR_TIMEOUT      : Deadline expired; object state remains consistent.
 *   - ERR_INVALID_ARG  : One or more arguments violate preconditions.
 */
```

---

## 8) Constantes, Macros e Atributos

Documente macros com intenção e restrições. Evite descrever o que já é óbvio pelo nome.

```c
/**
 * @brief   Mark return values as must-check.
 * @details Expands to [[nodiscard]] on supported compilers.
 *          Apply to any API where ignoring the return value is a bug.
 */
#define NODISCARD /* ... */

/**
 * @brief  Non-null pointer contract (static analysis aid).
 * @note   Apply to pointer parameters only. Does not generate runtime checks.
 */
#define NONNULL /* ... */
```

---

## 9) Concorrência e Reentrância

Documente explicitamente para toda API pública:

- **Thread-safety** dos métodos (nenhuma, lock interno, sincronização externa exigida)
- **Reentrância** entre instâncias distintas vs. mesma instância
- **IRQ-safety** (pode ser chamado de ISR ou requer contexto adiado)
- **Comportamento bloqueante** (busy-wait, IRQ/DMA, dependência de RTOS)

Use `@note`, `@warning` ou uma subseção **"Thread-safety"** em `@details`.

---

## 10) Exemplos de Uso

Use `@code` / `@endcode` para exemplos compiláveis. Prefira trechos pequenos e focados.

```c++
/**
 * @example uart_tx_example.cpp
 * @brief   Demonstrates synchronous write with error handling.
 * @code
 * Uart uart{&huart2, rx_buf, sizeof(rx_buf)};
 * const uint8_t msg[] = "hello";
 * const ReturnCode rc = uart.write(msg, sizeof(msg));
 * if (rc != ReturnCode::ANSWERED_REQUEST) { handle_error(rc); }
 * @endcode
 */
```

---

## 11) Regras de Formatação

- Limite de **100 colunas** por linha sempre que possível.
- Alinhe os comentários `/*!< ... */` de valores de enum para facilitar a leitura.
- Use **frases completas** em `@brief` e **bullets** em `@details`.
- Ortografia em **inglês americano** em todos os comentários de código.
- Uma linha em branco entre o bloco Doxygen e o elemento de código.
- Use `/** ... */` para C++ e `/* ... */` para C puro.
- Prefira voz ativa: diga o que a função **faz**, não "é usada para fazer".

**Correto:**
```c++
/** @brief Start asynchronous TX transfer. */
```

**Evite:**
```c++
/** @brief Function to be used for starting the TX process. */
```

---

## 12) Guards de Header e Organização de Arquivos

- Um conceito público principal por header.
- Forward declarations apenas em headers de detalhe interno, se necessário.
- Use guards no estilo do projeto (`#ifndef HELIOS_..._HPP_`) ou `#pragma once` conforme o padrão definido.
- Não exponha headers internos em includes públicos.

---

## 13) Checklist — API Pública

Antes de submeter qualquer header público, verifique:

- [ ] `@brief` de uma linha
- [ ] `@details` com bullets descrevendo comportamento e restrições
- [ ] Todos os `@param[in/out]` documentados
- [ ] `@return` documentado; `[[nodiscard]]` aplicado se necessário
- [ ] Semântica de erro (valores de `ReturnCode` listados)
- [ ] Thread-safety / Reentrância / IRQ-safety
- [ ] Timeouts e comportamento bloqueante
- [ ] Regras de propriedade e tempo de vida
- [ ] Exemplos quando o uso não for trivial
- [ ] Pertencimento a grupo (`@ingroup`)

---

## 14) Exemplos Prontos para Colar

### 14.1 Função

```c++
/**
 * @brief     Transmit a buffer over UART.
 * @param[in] buf   HEL_NONNULL pointer to data (min size = @p size bytes).
 * @param[in] size  Number of bytes to send.
 * @return    ReturnCode (see @ref ReturnCode).
 * @warning   On non-OK return, no bytes were consumed. Always check the result.
 */
[[nodiscard]]
ReturnCode write(const uint8_t* buf, size_t size) noexcept;
```

### 14.2 Enum

```c++
/**
 * @brief   GPIO interrupt trigger mode.
 * @ingroup HELIOS_DRV_GPIO
 */
enum class GpioIrqMode : uint8_t
{
  DISABLED = 0U, /*!< Interrupt disabled                               */
  RISING   = 1U, /*!< Trigger on rising edge                           */
  FALLING  = 2U, /*!< Trigger on falling edge                          */
  CHANGE   = 3U  /*!< Trigger on both rising and falling edges         */
};
```

### 14.3 Classe

```c++
/**
 * @brief   UART driver interface.
 * @details
 *   - Synchronous TX/RX with optional IRQ/DMA
 *   - No dynamic allocation after init
 *   - Errors reported via ReturnCode
 * @ingroup HELIOS_DRV_UART
 */
class iUart {
public:
  /** @brief Initialize hardware and internal buffers. */
  [[nodiscard]] ReturnCode init() noexcept;

  /** @brief Deinitialize hardware; safe to call multiple times. */
  void deinit() noexcept;
};
```

---

## 15) Referência Rápida de Tags

| Tag | Uso |
|-----|-----|
| `@brief` | Descrição de uma linha |
| `@details` | Descrição estendida em bullets |
| `@note` | Observação importante, não crítica |
| `@warning` | Aviso crítico (comportamento incorreto se ignorado) |
| `@attention` | Bloco de licença / atenção especial |
| `@param[in]` | Parâmetro de entrada |
| `@param[out]` | Parâmetro de saída |
| `@param[in,out]` | Parâmetro de entrada e saída |
| `@tparam` | Parâmetro de template |
| `@return` | Descrição do retorno |
| `@retval` | Valor de retorno específico |
| `@ingroup` | Associa elemento a um grupo (definido em `groups.dox`) |
| `@addtogroup` | Reabre grupo para adicionar documentação |
| `@{` / `@}` | Delimita escopo de grupo |
| `@code` / `@endcode` | Bloco de código de exemplo |
| `@example` | Referência a arquivo de exemplo |
| `@see` | Referência cruzada |
| `@ref` | Link para outro elemento documentado |
| `@since` | Versão em que o elemento foi introduzido |
| `@version` | Versão atual |
| `@date` | Data de criação ou modificação |
| `@deprecated` | Marca elemento como obsoleto |

---

Para propor melhorias a este guia, abra um PR com exemplos de antes/depois.
