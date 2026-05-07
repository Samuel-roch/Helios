/**
 ******************************************************************************
 * @file    pit.cpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-20
 * @ingroup HELIOS_SYSLIB_PIT
 * @brief   Polled interval timer implementation.
 ******************************************************************************
 */
#include "pit.hpp"
#include <hel_target>

namespace hel
{

// ---------------------------------------------------------------------------
// Static storage
// ---------------------------------------------------------------------------

iTimer* Pit::s_global_timer = nullptr;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

Pit::Tick Pit::globalNowMs() noexcept
{
    // Extend the 32-bit HAL tick to 64-bit with wrap detection so that
    // Tick arithmetic (now - m_start) stays valid across the ~49-day rollover.
    static uint32_t s_prev  = 0U;
    static Tick     s_hi    = 0U;

    const uint32_t cur = static_cast<uint32_t>(HEL_TARGET_TICK());
    if (cur < s_prev)
    {
        s_hi += static_cast<Tick>(1ULL << 32);
    }
    s_prev = cur;
    return s_hi | static_cast<Tick>(cur);
}

Pit::Tick Pit::globalNowUs() noexcept
{
    return (s_global_timer != nullptr) ? s_global_timer->count() : 0U;
}

Pit::Tick Pit::now() const noexcept
{
    return (m_mode == TimeUnit::Microseconds) ? globalNowUs() : globalNowMs();
}

Pit::Tick Pit::toTicks(uint32_t value, TimeUnit unit) const noexcept
{
    if (m_mode == TimeUnit::Microseconds)
    {
        // µs base: convert ms → µs; keep µs as-is.
        return (unit == TimeUnit::Milliseconds)
            ? static_cast<Tick>(value) * 1'000U
            : static_cast<Tick>(value);
    }

    // ms base: keep ms; convert µs → ms (ceiling).
    return (unit == TimeUnit::Microseconds)
        ? (static_cast<Tick>(value) + 999U) / 1'000U
        : static_cast<Tick>(value);
}

Pit::Tick Pit::elapsedAt(Tick t) const noexcept
{
    const Tick paused = m_paused
        ? (t - m_pause_start + m_pause_accumulated)
        : m_pause_accumulated;
    return (t - m_start) - paused;
}

// ---------------------------------------------------------------------------
// Timer control
// ---------------------------------------------------------------------------

void Pit::start(uint32_t value, TimeUnit unit) noexcept
{
    m_mode              = unit;
    m_period            = toTicks(value, unit);
    m_start             = now();
    m_pause_accumulated = 0U;
    m_paused            = false;
    m_running           = true;
}

void Pit::stop() noexcept
{
    m_running           = false;
    m_paused            = false;
    m_pause_accumulated = 0U;
}

void Pit::pause() noexcept
{
    if (m_running && !m_paused)
    {
        m_pause_start = now();
        m_paused      = true;
    }
}

void Pit::resume() noexcept
{
    if (m_running && m_paused)
    {
        m_pause_accumulated += now() - m_pause_start;
        m_paused             = false;
    }
}

void Pit::setAutoReload(bool ar) noexcept
{
    m_auto_reload = ar;
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

bool Pit::expired() noexcept
{
    if (!m_running)
    {
        return false;
    }

    const Tick t = now();
    const Tick e = elapsedAt(t);

    if (e >= m_period)
    {
        if (m_auto_reload)
        {
            m_start             = t;
            m_pause_accumulated = 0U;
            m_paused            = false;
        }
        else
        {
            m_running = false;
        }
        return true;
    }
    return false;
}

Pit::Tick Pit::elapsed() const noexcept
{
    return m_running ? elapsedAt(now()) : 0U;
}

Pit::Tick Pit::remaining() const noexcept
{
    if (!m_running)
    {
        return 0U;
    }
    const Tick e = elapsedAt(now());
    return (e >= m_period) ? 0U : (m_period - e);
}

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

void Pit::setGlobalTimer(iTimer& timer) noexcept
{
    s_global_timer = &timer;
}

void Pit::delay(uint32_t value, TimeUnit unit) noexcept
{
    if (value == 0U)
    {
        return;
    }

    if (unit == TimeUnit::Microseconds && s_global_timer != nullptr)
    {
        const Tick start = globalNowUs();
        while ((globalNowUs() - start) < static_cast<Tick>(value)) {}
        return;
    }

    // Millisecond path (or µs fallback: ceil to ms).
    const Tick ms = (unit == TimeUnit::Milliseconds)
        ? static_cast<Tick>(value)
        : (static_cast<Tick>(value) + 999U) / 1'000U;

    const Tick start = globalNowMs();
    while ((globalNowMs() - start) < ms) {}
}

} // namespace hel
