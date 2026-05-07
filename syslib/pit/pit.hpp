/**
 ******************************************************************************
 * @file    pit.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-20
 * @ingroup HELIOS_SYSLIB_PIT
 * @brief   Polled interval timer (cooperative, allocation-free).
 *
 * @details Two operating modes:
 *   - **Milliseconds (default)** — tick source is `HEL_TARGET_TICK()`.
 *   - **Microseconds** — tick source is `iTimer::elapsed_us()` from a
 *     hardware timer supplied by the caller.
 *
 * ### Millisecond mode (no hardware timer needed)
 * @code
 *   hel::Pit pit;
 *   pit.start(500);           // 500 ms
 *   if (pit.expired()) { ... }
 * @endcode
 *
 * ### Microsecond mode (requires a running hel::Timer)
 * @code
 *   // timer must be started in Periodic mode and must outlive the Pit.
 *   hel::Pit pit(hw_timer);
 *   pit.start(250, hel::TimeUnit::Microseconds);
 *   if (pit.expired()) { ... }
 * @endcode
 *
 * ### Static utilities (delay / uptime)
 * @code
 *   hel::Pit::delay(10);      // 10 ms, no timer needed
 *
 *   hel::Pit::set_global_timer(hw_timer); // one-time call
 *   hel::Pit::delay(500, hel::TimeUnit::Microseconds);
 *   hel::Pit::uptime(hel::TimeUnit::Microseconds);
 * @endcode
 *
 ******************************************************************************
 */

#ifndef HELIOS_SYSLIB_PIT_HPP_
#define HELIOS_SYSLIB_PIT_HPP_

#include <hel_itimer>
#include <cstdint>

namespace hel
{

/**
 * @enum  TimeUnit
 * @brief Time resolution accepted by the Pit public API.
 * @ingroup HELIOS_SYSLIB_PIT
 */
enum class TimeUnit : uint8_t
{
  Milliseconds = 0U, /*!< Millisecond resolution (default, `HEL_TARGET_TICK`). */
  Microseconds = 1U, /*!< Microsecond resolution (requires hardware timer).     */
};

/**
 * @class  Pit
 * @brief  Cooperative polled interval timer.
 * @ingroup HELIOS_SYSLIB_PIT
 *
 * @details Wraps a monotonic tick source and provides start / stop / pause /
 *          resume / expired semantics without blocking or dynamic allocation.
 *          All timing arithmetic uses unsigned 32-bit subtraction, so the
 *          counter wraps naturally without special handling (up to ~49 days
 *          in ms mode, ~71 minutes in µs mode).
 *
 * @note  The Pit does **not** own the hardware timer; the caller must start
 *        the timer in @ref TimerMode::Periodic mode and keep it alive for the
 *        lifetime of the Pit.
 */
class Pit
{
public:
  /** @brief Internal tick type (ms or µs depending on mode). */
  using Tick = uint64_t;

  // -------------------------------------------------------------------------
  // Construction
  // -------------------------------------------------------------------------

  /** @brief Millisecond mode — uses `HEL_TARGET_TICK()`. */
  Pit() noexcept = default;

  /**
   * @brief  Microsecond mode — uses the supplied hardware timer.
   * @param[in]  timer  Running hardware timer in @ref TimerMode::Periodic mode;
   *                    must remain valid for the lifetime of this Pit.
   */
  explicit Pit(iTimer& timer) noexcept;

  /**
   * @brief  Start (or restart) the interval timer.
   * @details Converts @p value to the internal unit (ms or µs) and captures
   *          the current tick as the start reference.
   *
   * @param[in]  value  Duration.  Must be > 0.
   * @param[in]  unit   @ref TimeUnit::Milliseconds (default) or
   *                    @ref TimeUnit::Microseconds.
   */
  void start(uint32_t value,
             TimeUnit unit = TimeUnit::Milliseconds) noexcept;

  /**
   * @brief  Stop the timer and clear all state.
   * @details After a call to stop(), @ref expired(), @ref elapsed(), and
   *          @ref remaining() all return 0 / false.
   */
  void stop() noexcept;

  /**
   * @brief  Pause time accounting.
   * @details Has no effect when already paused or not running.
   */
  void pause() noexcept;

  /**
   * @brief  Resume time accounting after a @ref pause.
   * @details The paused duration is subtracted from elapsed time.
   *          Has no effect when not paused or not running.
   */
  void resume() noexcept;

  /**
   * @brief  Enable or disable automatic reload on expiry.
   * @details When enabled, @ref expired() restarts the period from the
   *          expiry point instead of stopping the timer.
   *
   * @param[in]  ar  `true` to enable auto-reload.
   */
  void setAutoReload(bool ar) noexcept;

  // -------------------------------------------------------------------------
  // Query
  // -------------------------------------------------------------------------

  /**
   * @brief  Returns `true` once the period has elapsed.
   * @details If auto-reload is enabled the timer resets automatically.
   *          Returns `false` while paused.
   *
   * @return `true` if expired, `false` otherwise.
   */
  bool expired() noexcept;

  /**
   * @brief  Time elapsed since @ref start (or last auto-reload), in ms or µs.
   * @details Paused durations are not counted.  Returns 0 when stopped.
   *
   * @return Elapsed ticks in the current mode unit.
   */
  [[nodiscard]]
  Tick elapsed() const noexcept;

  /**
   * @brief  Time remaining until expiry, in ms or µs.
   * @details Returns 0 when stopped or already expired.
   *
   * @return Remaining ticks in the current mode unit.
   */
  [[nodiscard]]
  Tick remaining() const noexcept;

  // -------------------------------------------------------------------------
  // Static utilities
  // -------------------------------------------------------------------------

  /**
   * @brief  Set the global microsecond source used by @ref delay and @ref uptime.
   * @details The timer must be started in @ref TimerMode::Periodic mode before
   *          calling @ref delay or @ref uptime with @ref TimeUnit::Microseconds.
   *          Call once at application startup.
   *
   * @param[in]  timer  Running hardware timer; must remain alive indefinitely.
   */
  static void setGlobalTimer(iTimer& timer) noexcept;

  /**
   * @brief  Cooperative busy-wait delay.
   * @details For @ref TimeUnit::Microseconds a global timer must be set via
   *          @ref set_global_timer; otherwise the value is rounded up to
   *          milliseconds.
   *
   * @param[in]  value  Duration.
   * @param[in]  unit   @ref TimeUnit::Milliseconds (default) or Microseconds.
   */
  static void delay(uint32_t value,
                    TimeUnit unit = TimeUnit::Milliseconds) noexcept;

  /**
   * @brief  Return the time elapsed since system start.
   * @details For @ref TimeUnit::Microseconds a global timer must be set via
   *          @ref setGlobalTimer; otherwise returns milliseconds regardless.
   *
   * @param[in]  unit  @ref TimeUnit::Milliseconds (default) or Microseconds.
   * @return  Monotonic uptime in the requested unit.
   */
  static Tick uptime(TimeUnit unit = TimeUnit::Milliseconds) noexcept;

private:
  // ---- Instance state ----
  iTimer* m_timer             = nullptr;
  Tick    m_period            = 0U;
  Tick    m_start             = 0U;
  Tick    m_pause_start       = 0U;
  Tick    m_pause_accumulated = 0U;
  bool    m_running           = false;
  bool    m_paused            = false;
  bool    m_auto_reload       = false;
  TimeUnit m_mode             = TimeUnit::Milliseconds;

  // ---- Global µs source ----
  static iTimer* s_global_timer;

  // ---- Private helpers ----

  /** @brief Read the current tick for this instance (ms or µs per mode). */
  Tick now() const noexcept;

  /** @brief Convert (value, unit) to the internal tick unit for this instance. */
  Tick toTicks(uint32_t value, TimeUnit unit) const noexcept;

  /** @brief Compute elapsed ticks at a given snapshot to avoid calling now() twice. */
  Tick elapsedAt(Tick t) const noexcept;

  /** @brief Read global ms tick (HEL_TARGET_TICK, wrap-extended to 32-bit). */
  static Tick globalNowMs() noexcept;

  /** @brief Read global µs tick from s_global_timer (0 if not set). */
  static Tick globalNowUs() noexcept;
};

} // namespace hel

#endif // HELIOS_SYSLIB_PIT_HPP_
