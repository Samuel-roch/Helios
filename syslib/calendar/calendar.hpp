/**
 ******************************************************************************
 * @file    calendar.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-22
 * @ingroup HELIOS_SYSLIB_CALENDAR
 * @brief   Date/time value type with cron-based RTC alarm management.
 *
 * @details
 *   - Wraps `struct tm` (C standard time) as a singleton bound to an @ref Rtc instance
 *   - String parsing from `"YYYY-MM-DD HH:MM:SS"` via @ref fromString
 *   - String formatting to `"YYYY-MM-DD HH:MM:SS"` via @ref toString
 *   - Recurring cron alarms via @ref setCron; automatically re-arms after each firing
 *   - Field validation via @ref isValid()
 *   - Full set of comparison operators; weekday not compared (derived from date)
 *
 * @note
 *   Obtain the instance via @ref Calendar::instance(Rtc&). The first call binds
 *   the singleton to the provided @ref Rtc; subsequent calls return the same object.
 *   Methods are **not** thread-safe; caller must serialize concurrent access.
 */

#ifndef HELIOS_SYSLIB_CALENDAR_HPP_
#define HELIOS_SYSLIB_CALENDAR_HPP_


#if __has_include("rtc/rtc.hpp")

#include "rtc/rtc.hpp"

#include <hel_target>
#include <hel_callback>
#include <hel_string>
#include <hel_return_code>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <cstddef>

namespace hel
{

// =============================================================================
// detail — internal cron helpers (not part of the public API)
// =============================================================================

enum class TimeZone : int8_t
{
  // Brasil
  FernandoDeNoronha = -2,      // FNT (GMT-2)
  Brasilia = -3,               // BRT (GMT-3) - Horário de Brasília
  Amazonas = -4,               // AMT (GMT-4) - Amazonas (sem horário de verão)
  Acre = -5,                   // ACT (GMT-5) - Acre

  // América do Norte
  Eastern = -5,                // EST (GMT-5) - Nova York, Toronto
  Central = -6,                // CST (GMT-6) - Chicago, Mexico City
  Mountain = -7,               // MST (GMT-7) - Denver, Phoenix
  Pacific = -8,                // PST (GMT-8) - Los Angeles, Vancouver
  Alaska = -9,                 // AKST (GMT-9)
  Hawaii = -10,                // HST (GMT-10)

  // Europa e África
  Azores = -1,                 // AZOT (GMT-1)
  UTC = 0,                     // GMT (GMT+0) - Londres, Lisboa
  CentralEurope = 1,           // CET (GMT+1) - Berlim, Paris, Madrid
  EasternEurope = 2,           // EET (GMT+2) - Atenas, Cairo, Istambul
  Moscow = 3,                  // MSK (GMT+3) - Moscou, Nairobi

  // Ásia
  Gulf = 4,                    // GST (GMT+4) - Dubai, Baku
  Pakistan = 5,                // PKT (GMT+5) - Islamabad, Karachi
  Bangladesh = 6,              // BST (GMT+6) - Dhaka
  Indochina = 7,               // ICT (GMT+7) - Bangkok, Hanoi
  China = 8,                   // CST (GMT+8) - Pequim, Xangai, Singapura
  Japan = 9,                   // JST (GMT+9) - Tóquio, Seul
  AustraliaEast = 10,          // AEST (GMT+10) - Sydney, Brisbane
  AustraliaWest = 8,           // AWST (GMT+8) - Perth

  // Pacífico
  Vladivostok = 10,            // VLAT (GMT+10)
  Magadan = 11,                // MAGT (GMT+11)
  Fiji = 12,                   // FJT (GMT+12)
  NewZealand = 12,             // NZST (GMT+12) - Wellington, Auckland
  Samoa = 13                   // WST (GMT+13)
};



namespace detail
{

/**
 * @brief  A single field of a cron expression.
 * @details
 *   - Any   : matches every value in the field's range (`*`).
 *   - Fixed : matches exactly one value (`N`).
 *   - Every : matches multiples of step starting from 0 (`* /N`).
 */
struct CronField
{
  enum class Type : uint8_t
  {
    Any,    // matches every value in the field's range (`*`)
    Fixed,  // matches exactly one value (`N`)
    Every   // matches multiples of step starting from 0 (`* /N`)
  };
  Type type = Type::Any;
  int16_t value = 0;
};

/**
 * @brief  Parsed representation of a 5- or 6-field cron expression.
 * @details Field order (6-field): `sec min hour dom month dow`
 *          Field order (5-field): `min hour dom month dow` — sec defaults to Fixed(0).
 */
struct CronExpr
{
  CronField sec { }; /*!< Seconds   [0, 59].           */
  CronField min { }; /*!< Minutes   [0, 59].           */
  CronField hour { }; /*!< Hours     [0, 23].           */
  CronField dom { }; /*!< Day-of-month [1, 31].        */
  CronField month { }; /*!< Month     [1, 12].           */
  CronField dow { }; /*!< Day-of-week [0, 6] Sun=0.   */
};

/** @brief Returns `true` when @p v satisfies @p f. */
inline bool matchField(const CronField& f, int v) noexcept
{
  switch (f.type)
  {
    case CronField::Type::Any :
      return true;
    case CronField::Type::Fixed :
      return v == static_cast<int>(f.value);
    case CronField::Type::Every :
      return (f.value > 0) && ((v % f.value) == 0);
    default :
      return false;
  }
}

/**
 * @brief  Returns the smallest value in [current, maxv] that satisfies @p f.
 * @return Matching value, or -1 if none exists in the range.
 */
inline int nextMatch(const CronField& f, int current, int maxv) noexcept
{
  for ( int v = current; v <= maxv; ++v )
  {
    if (matchField(f, v))
      return v;
  }
  return -1;
}

/**
 * @brief  Parse one cron token into @p out.
 * @details Accepts `*`, `N`, or `* /N` (without the space — space shown to avoid
 *          Doxygen interpreting it as a comment continuation).
 * @return `true` on success.
 */
inline bool parseCronField(
  const char* tok, CronField& out, int minv, int maxv) noexcept
{
  if (tok == nullptr || tok[0] == '\0')
    return false;

  if (tok[0] == '*')
  {
    if (tok[1] == '\0')
    {
      out = { CronField::Type::Any, 0 };
      return true;
    }
    if (tok[1] == '/' && tok[2] != '\0')
    {
      int step = 0;
      const char *p = tok + 2;
      while (*p >= '0' && *p <= '9')
      {
        step = step * 10 + (*p - '0');
        ++p;
      }

      if (*p != '\0' || step <= 0 || step > (maxv - minv + 1))
      {
        return false;
      }

      out = { CronField::Type::Every, static_cast<int16_t>(step) };

      return true;
    }
    return false;
  }

  int val = 0;
  const char *p = tok;
  if (*p < '0' || *p > '9')
  {
    return false;
  }

  while (*p >= '0' && *p <= '9')
  {
    val = val * 10 + (*p - '0');
    ++p;
  }

  if (*p != '\0' || val < minv || val > maxv)
  {
    return false;
  }

  out = { CronField::Type::Fixed, static_cast<int16_t>(val) };

  return true;
}

/**
 * @brief  Parse a 5- or 6-field cron expression into @p out.
 * @details 5-field: `min hour dom month dow` — sec defaults to `Fixed(0)`.
 *          6-field: `sec min hour dom month dow`.
 * @return `true` on success.
 */
inline bool parseCronExpr(const char* expr, CronExpr& out) noexcept
{
  if (expr == nullptr)
    return false;

  char buf[64];
  std::size_t len = 0;
  while (expr[len] != '\0' && len < sizeof(buf) - 1U)
  {
    buf[len] = expr[len];
    ++len;
  }
  buf[len] = '\0';

  const char *tok[6] = { };
  int n = 0;
  char *p = buf;
  while (*p != '\0' && n < 6)
  {
    while (*p == ' ')
      ++p;
    if (*p == '\0')
      break;
    tok[n++] = p;
    while (*p != ' ' && *p != '\0')
      ++p;
    if (*p == ' ')
    {
      *p = '\0';
      ++p;
    }
  }

  if (n == 5)
  {
    out.sec = { CronField::Type::Fixed, 0 };
    return parseCronField(tok[0], out.min, 0, 59)
        && parseCronField(tok[1], out.hour, 0, 23)
        && parseCronField(tok[2], out.dom, 1, 31)
        && parseCronField(tok[3], out.month, 1, 12)
        && parseCronField(tok[4], out.dow, 0, 6);
  }
  if (n == 6)
  {
    return parseCronField(tok[0], out.sec, 0, 59)
        && parseCronField(tok[1], out.min, 0, 59)
        && parseCronField(tok[2], out.hour, 0, 23)
        && parseCronField(tok[3], out.dom, 1, 31)
        && parseCronField(tok[4], out.month, 1, 12)
        && parseCronField(tok[5], out.dow, 0, 6);
  }
  return false;
}

/**
 * @brief  Compute the next `struct tm` that satisfies @p cron after @p from.
 * @details Advances field-by-field (month → day → hour → minute → second) resetting
 *          less-significant fields when a more-significant field is advanced.
 *          For dom/dow: if both are specified (not `*`), either match is accepted
 *          (standard cron behaviour). Searches up to ~1 year ahead.
 * @param[in]  from  Starting point (inclusive of the next second).
 * @param[in]  cron  Parsed cron expression.
 * @param[out] next  Next matching instant.
 * @return `true` if a match was found within the search window.
 */
inline bool computeNext(
  const struct tm& from, const CronExpr& cron, struct tm& next) noexcept
{
  struct tm t = from;
  t.tm_sec += 1;
  t.tm_isdst = -1;
  mktime(&t);

  for ( int attempts = 0; attempts < (366 * 25); ++attempts )
  {
    // ---- month (tm_mon is 0-based; cron is 1-based) ----
    const int cm = t.tm_mon + 1;
    const int nm = nextMatch(cron.month, cm, 12);
    if (nm < 0)
    {
      t.tm_year++;
      t.tm_mon = 0;
      t.tm_mday = 1;
      t.tm_hour = t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }
    if (nm != cm)
    {
      t.tm_mon = nm - 1;
      t.tm_mday = 1;
      t.tm_hour = t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }

    // ---- day (dom and/or dow) ----
    const bool domAny = (cron.dom.type == CronField::Type::Any);
    const bool dowAny = (cron.dow.type == CronField::Type::Any);
    const bool domOk = domAny || matchField(cron.dom, t.tm_mday);
    const bool dowOk = dowAny || matchField(cron.dow, t.tm_wday);
    const bool dayOk = (domAny && dowAny) ? true : (domAny) ? dowOk :
                       (dowAny) ? domOk : (domOk || dowOk);
    if (!dayOk)
    {
      t.tm_mday++;
      t.tm_hour = t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }

    // ---- hour ----
    const int nh = nextMatch(cron.hour, t.tm_hour, 23);
    if (nh < 0)
    {
      t.tm_mday++;
      t.tm_hour = t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }
    if (nh != t.tm_hour)
    {
      t.tm_hour = nh;
      t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }

    // ---- minute ----
    const int nmi = nextMatch(cron.min, t.tm_min, 59);
    if (nmi < 0)
    {
      t.tm_hour++;
      t.tm_min = t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }
    if (nmi != t.tm_min)
    {
      t.tm_min = nmi;
      t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }

    // ---- second ----
    const int ns = nextMatch(cron.sec, t.tm_sec, 59);
    if (ns < 0)
    {
      t.tm_min++;
      t.tm_sec = 0;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }
    if (ns != t.tm_sec)
    {
      t.tm_sec = ns;
      t.tm_isdst = -1;
      mktime(&t);
      continue;
    }

    next = t;
    return true;
  }
  return false;
}

} // namespace detail

// =============================================================================
// Calendar
// =============================================================================

/**
 * @class  Calendar
 * @brief  Date/time value type with cron-based RTC alarm management.
 * @ingroup HELIOS_SYSLIB_CALENDAR
 *
 * @details
 *   - Singleton bound to an @ref Rtc peripheral; obtain via @ref instance
 *   - Wraps `struct tm` (C standard) for compatibility with the C time library
 *   - @ref sync reads the current RTC time into the internal `struct tm`
 *   - @ref commit writes the internal `struct tm` back to the RTC
 *   - @ref fromString / @ref toString handle `"YYYY-MM-DD HH:MM:SS"` format
 *   - @ref setCron arms a recurring alarm that re-arms automatically after each firing
 *   - Comparison operators compare (year, month, day, hour, minute, second);
 *     weekday is not compared because it is derived from the date
 *
 * @note  Thread-safety: individual methods are **not** thread-safe on the same instance;
 *        caller must serialize concurrent access.
 *
 * @code
 * // Bind to RTC once at startup
 * hel::Calendar& cal = hel::Calendar::instance(rtc);
 *
 * // Parse a fixed time and commit it to the RTC
 * cal.fromString("2026-01-01 00:00:00");
 * cal.commit();
 *
 * // Set a daily alarm at 08:00
 * cal.setCron("0 8 * * *", [](hel::ReturnCode) { triggerDailyTask(); });
 * @endcode
 */
class Calendar
{
public:

  /**
   * @brief Callable type for recurring cron alarm notifications.
   *
   * @details Signature: `void handler(ReturnCode code) noexcept`
   *   - @p code  — @ref ReturnCode::AnsweredRequest on a normal alarm event,
   *                error code on peripheral fault.
   */
  using AlarmCallback = Callback<void()>;

  // -------------------------------------------------------------------------
  // Singleton access
  // -------------------------------------------------------------------------

  /**
   * @brief     Return the singleton Calendar bound to @p rtc.
   * @details   The first call initializes the singleton with @p rtc.
   *            Subsequent calls return the same instance; @p rtc is ignored.
   * @param[in] rtc  RTC peripheral used for time read/write and alarm control.
   * @return    Reference to the single Calendar instance.
   */
  static Calendar& instance() noexcept
  {
    static Calendar inst(Rtc::instance());
    return inst;
  }

  // -------------------------------------------------------------------------
  // RTC synchronization
  // -------------------------------------------------------------------------

  /**
   * @brief  Read the current RTC time into the internal `struct tm`.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Internal state updated with current RTC time.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; internal state unchanged.
   */
  [[nodiscard]] ReturnCode sync() noexcept
  {
    return m_rtc.getTime(m_datetime);
  }

  /**
   * @brief  Write the internal `struct tm` to the RTC.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : RTC updated successfully.
   *   - @ref ReturnCode::ErrorParam      : Internal state contains an out-of-range field.
   *   - @ref ReturnCode::ErrorGeneral   : Peripheral fault; RTC time unchanged.
   */
  [[nodiscard]] ReturnCode commit() noexcept
  {
    return m_rtc.setTime(m_datetime);
  }

  // -------------------------------------------------------------------------
  // String conversion
  // -------------------------------------------------------------------------

  /**
   * @brief     Parse a datetime string into the internal `struct tm`.
   * @details   Expected format: `"YYYY-MM-DD HH:MM:SS"` (exactly 19 characters).
   *            Call @ref commit to write the parsed time to the RTC.
   * @param[in] str  Null-terminated datetime string.
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Parsed successfully; internal state updated.
   *   - @ref ReturnCode::ErrorParam      : @p str is `nullptr`, wrong length, or malformed.
   */
  [[nodiscard]] ReturnCode fromString(String& str) noexcept
  {
    if (str == nullptr)
      return ReturnCode::ErrorParam;

    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    // NOLINTNEXTLINE(cert-err34-c)
    const int parsed = std::sscanf(str.raw(), "%4d-%2d-%2d %2d:%2d:%2d", &y,
        &mo, &d, &h, &mi, &s);
    if (parsed != 6)
      return ReturnCode::ErrorParam;

    m_datetime.tm_year = y - 1900;
    m_datetime.tm_mon = mo - 1;
    m_datetime.tm_mday = d;
    m_datetime.tm_hour = h;
    m_datetime.tm_min = mi;
    m_datetime.tm_sec = s;
    m_datetime.tm_isdst = -1;
    mktime(&m_datetime);

    if (!isValid())
      return ReturnCode::ErrorParam;
    return ReturnCode::AnsweredRequest;
  }

  /**
   * @brief      Format the internal `struct tm` into a datetime string.
   * @details    Writes `"YYYY-MM-DD HH:MM:SS"` (19 chars + null terminator).
   *             @p buf must be at least 20 bytes.
   * @param[out] buf   Destination buffer.
   * @param[in]  size  Size of @p buf in bytes (must be ≥ 20).
   * @return `true` on success, `false` if @p buf is too small or `nullptr`.
   */
  [[nodiscard]] bool toString(char* buf, std::size_t size) const noexcept
  {
    if (buf == nullptr || size < 20U)
      return false;
    const int n = std::snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d",
        year(), month(), day(), hour(), minute(), second());
    return n == 19;
  }

  // -------------------------------------------------------------------------
  // Raw struct tm access
  // -------------------------------------------------------------------------

  /** @brief Return a const reference to the internal `struct tm`. */
  [[nodiscard]] const struct tm& get() const noexcept
  {
    return m_datetime;
  }

  /**
   * @brief     Replace the internal `struct tm` with @p dt.
   * @param[in] dt  New datetime value (normalized with `mktime` internally).
   */
  void set(const struct tm& dt) noexcept
  {
    m_datetime = dt;
    m_datetime.tm_isdst = -1;
    mktime(&m_datetime);
  }

  // -------------------------------------------------------------------------
  // Field getters (human-friendly)
  // -------------------------------------------------------------------------

  /** @brief Full year (e.g. 2026). */
  [[nodiscard]] int year() const noexcept
  {
    return m_datetime.tm_year + 1900;
  }

  /** @brief Month of the year [1, 12]. */
  [[nodiscard]] int month() const noexcept
  {
    return m_datetime.tm_mon + 1;
  }

  /** @brief Day of the month [1, 31]. */
  [[nodiscard]] int day() const noexcept
  {
    return m_datetime.tm_mday;
  }

  /** @brief Day of the week [0, 6], Sunday = 0. */
  [[nodiscard]] int weekday() const noexcept
  {
    return m_datetime.tm_wday;
  }

  /** @brief Hour [0, 23]. */
  [[nodiscard]] int hour() const noexcept
  {
    return m_datetime.tm_hour;
  }

  /** @brief Minute [0, 59]. */
  [[nodiscard]] int minute() const noexcept
  {
    return m_datetime.tm_min;
  }

  /** @brief Second [0, 59]. */
  [[nodiscard]] int second() const noexcept
  {
    return m_datetime.tm_sec;
  }

  // -------------------------------------------------------------------------
  // Field setters (human-friendly; normalizes with mktime)
  // -------------------------------------------------------------------------

  /** @brief Set the year (e.g. 2026). */
  void setYear(int y) noexcept
  {
    m_datetime.tm_year = y - 1900;
    mktime(&m_datetime);
  }

  /** @brief Set the month [1, 12]. */
  void setMonth(int mo) noexcept
  {
    m_datetime.tm_mon = mo - 1;
    mktime(&m_datetime);
  }

  /** @brief Set the day of the month [1, 31]. */
  void setDay(int d) noexcept
  {
    m_datetime.tm_mday = d;
    mktime(&m_datetime);
  }

  /** @brief Set the hour [0, 23]. */
  void setHour(int h) noexcept
  {
    m_datetime.tm_hour = h;
    mktime(&m_datetime);
  }

  /** @brief Set the minute [0, 59]. */
  void setMinute(int mi) noexcept
  {
    m_datetime.tm_min = mi;
    mktime(&m_datetime);
  }

  /** @brief Set the second [0, 59]. */
  void setSecond(int s) noexcept
  {
    m_datetime.tm_sec = s;
    mktime(&m_datetime);
  }

  // -------------------------------------------------------------------------
  // Validation
  // -------------------------------------------------------------------------

  /**
   * @brief  Check that all fields are within their valid ranges.
   * @details Validates: year [2000, 2099], month [1, 12], day [1, 31],
   *          weekday [0, 6], hour [0, 23], minute [0, 59], second [0, 59].
   * @return `true` if every field is in its valid range.
   */
  [[nodiscard]] bool isValid() const noexcept
  {
    return (year() >= 2000 && year() <= 2099) && (month() >= 1 && month() <= 12)
        && (day() >= 1 && day() <= 31) && (weekday() >= 0 && weekday() <= 6)
        && (hour() >= 0 && hour() <= 23) && (minute() >= 0 && minute() <= 59)
        && (second() >= 0 && second() <= 59);
  }

  // -------------------------------------------------------------------------
  // Cron alarm management
  // -------------------------------------------------------------------------

  /**
   * @brief     Arm a recurring cron alarm.
   * @details   Parses @p expr and schedules the next matching RTC alarm. After each
   *            firing the callback is invoked, the current RTC time is synced, and
   *            the alarm is automatically re-armed for the following occurrence.
   *
   *            Supported cron syntax (fields separated by spaces):
   *            | Format  | Fields                              |
   *            |---------|-------------------------------------|
   *            | 5-field | `min hour dom month dow`           |
   *            | 6-field | `sec min hour dom month dow`       |
   *
   *            Field values:
   *            | Token   | Meaning                            |
   *            |---------|------------------------------------|
   *            | `*`     | Every value in range               |
   *            | `N`     | Exactly value N                    |
   *            | `* /N`  | Every N-th value starting from 0  |
   *
   *            **Examples:**
   *            @code
   *            setCron("0 8 * * *",   cb); // every day at 08:00:00
   *///         setCron("*/5 * * * *", cb); // every 5 minutes at :00 seconds
  /*            setCron("30 * * * * *",cb); // every minute at :30 seconds (6-field)
   *            setCron("0 0 1 * *",   cb); // first day of each month at midnight
   *            setCron("0 9 * * 1",   cb); // every Monday at 09:00:00 (dow 1=Monday)
   *            @endcode
   *
   * @param[in] expr      Null-terminated cron expression string.
   * @param[in] callback  Invoked on each alarm firing (@ref AlarmCallback).
   * @return    ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Alarm armed; first occurrence scheduled.
   *   - @ref ReturnCode::ErrorParam      : @p expr is malformed or no future occurrence exists.
   *   - @ref ReturnCode::ErrorGeneral   : RTC fault; alarm not set.
   */
  [[nodiscard]] ReturnCode setCron(
    const char* expr, AlarmCallback callback) noexcept
  {
    detail::CronExpr cron { };
    if (!detail::parseCronExpr(expr, cron))
      return ReturnCode::ErrorParam;

    m_cron = cron;
    m_alarmCb = callback;
    m_cronActive = true;
    return armNextAlarm();
  }

  /**
   * @brief  Disarm the current cron alarm.
   * @details No callback is fired. Safe to call when no alarm is active.
   * @return ReturnCode
   *   - @ref ReturnCode::AnsweredRequest : Alarm disarmed (or was already inactive).
   *   - @ref ReturnCode::ErrorGeneral   : RTC fault; alarm state is unspecified.
   */
  [[nodiscard]] ReturnCode clearCron() noexcept
  {
    m_cronActive = false;
    m_alarmCb = AlarmCallback { };
    return m_rtc.clearAlarm();
  }

  // -------------------------------------------------------------------------
  // Comparison operators
  // -------------------------------------------------------------------------

  /**
   * @brief  Return `true` when both instances represent the same instant.
   * @note   Weekday is not compared; it is derived from the date.
   */
  [[nodiscard]] bool operator==(const Calendar& rhs) const noexcept
  {
    return year() == rhs.year() && month() == rhs.month() && day() == rhs.day()
        && hour() == rhs.hour() && minute() == rhs.minute()
        && second() == rhs.second();
  }

  /** @brief Return `true` when the two instances represent different instants. */
  [[nodiscard]] bool operator!=(const Calendar& rhs) const noexcept
  {
    return !(*this == rhs);
  }

  /** @brief Return `true` when this instant is strictly earlier than @p rhs. */
  [[nodiscard]] bool operator<(const Calendar& rhs) const noexcept
  {
    if (year() != rhs.year())
      return year() < rhs.year();
    if (month() != rhs.month())
      return month() < rhs.month();
    if (day() != rhs.day())
      return day() < rhs.day();
    if (hour() != rhs.hour())
      return hour() < rhs.hour();
    if (minute() != rhs.minute())
      return minute() < rhs.minute();
    return second() < rhs.second();
  }

  /** @brief Return `true` when this instant is earlier than or equal to @p rhs. */
  [[nodiscard]] bool operator<=(const Calendar& rhs) const noexcept
  {
    return !(rhs < *this);
  }

  /** @brief Return `true` when this instant is strictly later than @p rhs. */
  [[nodiscard]] bool operator>(const Calendar& rhs) const noexcept
  {
    return rhs < *this;
  }

  /** @brief Return `true` when this instant is later than or equal to @p rhs. */
  [[nodiscard]] bool operator>=(const Calendar& rhs) const noexcept
  {
    return !(*this < rhs);
  }

private:

  // -------------------------------------------------------------------------
  // Construction (private — use instance())
  // -------------------------------------------------------------------------

  explicit Calendar(Rtc& rtc) noexcept
      :
      m_rtc(rtc)
  {
//    m_rtc.setCallback(RtcCallback { [this](ReturnCode code) noexcept
//    {
//      onAlarm(code);
//    }});
  }

  Calendar(const Calendar&) = delete;
  Calendar& operator=(const Calendar&) = delete;
  Calendar(Calendar&&) = delete;
  Calendar& operator=(Calendar&&) = delete;

  // -------------------------------------------------------------------------
  // Internal alarm handling
  // -------------------------------------------------------------------------

  /**
   * @brief  Called by the Rtc callback when an alarm fires.
   * @details Invokes the user callback, syncs the current RTC time, then
   *          re-arms the next occurrence if the cron is still active.
   */
  void onAlarm() noexcept
  {
    if (m_alarmCb)
      m_alarmCb();
    if (m_cronActive)
    {
      (void) sync();
      (void) armNextAlarm();
    }
  }

  /**
   * @brief  Compute the next cron occurrence and arm the RTC alarm.
   * @return ReturnCode forwarded from @ref Rtc::setAlarm, or
   *         @ref ReturnCode::ErrorParam if no occurrence exists within the search window.
   */
  [[nodiscard]]
  ReturnCode armNextAlarm() noexcept
  {
    struct tm next { };

    if (!detail::computeNext(m_datetime, m_cron, next))
    {
      return ReturnCode::ErrorParam;
    }

    return m_rtc.setAlarm(next);
  }

  // -------------------------------------------------------------------------
  // Attributes
  // -------------------------------------------------------------------------

  Rtc &m_rtc; /*!< Bound RTC peripheral.            */
  struct tm m_datetime { }; /*!< Internal date/time state.         */
  detail::CronExpr m_cron { }; /*!< Active cron expression.           */
  bool m_cronActive { false }; /*!< `true` when a cron alarm is armed.*/
  AlarmCallback m_alarmCb { }; /*!< User callback for alarm events.   */
};


} // namespace hel

#endif /* __has_include("rtc/rct.hpp") */

#endif // HELIOS_SYSLIB_CALENDAR_HPP_
