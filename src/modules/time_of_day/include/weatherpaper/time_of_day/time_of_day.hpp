// weatherpaper/time_of_day/time_of_day.hpp
//
// Module: time_of_day
// Layer:  pure logic (Section 3.2) - NO OS clock access, NO network.
//
// Converts (sunrise, sunset, "now") -> one of {Morning, Day, Evening, Night}.
// All three timestamps are supplied by the caller (weather_fetch supplies
// sunrise/sunset from the API response; the platform layer supplies "now"),
// which is precisely what makes this module trivially unit-testable with
// fake timestamps and independent of any real clock or network access.
//
// Bucket rule (spec Section 3.2):
//   Morning : [sunrise, sunrise + 2h30m)
//   Day     : [sunrise + 2h30m, sunset - 1h)
//   Evening : [sunset - 1h, sunset + 30m)
//   Night   : everything else (incl. before sunrise and after evening ends)
//
// Edge cases handled explicitly (see .cpp for reasoning):
//   * "now" before sunrise on the same calendar day            -> Night
//   * degenerate/polar inputs where sunrise >= sunset           -> Night bucket
//     is used as the safe fallback (see ASSUMPTION comment in .cpp)
//   * sunrise/sunset that roll past midnight are handled because everything
//     here operates on std::chrono::system_clock::time_point (absolute
//     instants), not on wall-clock "hour of day" - so no timezone bugs.
#pragma once

#include <chrono>
#include <string_view>

namespace weatherpaper::time_of_day {

enum class Bucket {
    Morning,
    Day,
    Evening,
    Night
};

// Human-readable / tag-system-compatible lowercase string, e.g. "morning".
// Matches the tag vocabulary used by tag_system (Section 3.8).
[[nodiscard]] std::string_view to_string(Bucket bucket) noexcept;

// Parses the inverse of to_string(). Returns Night (with success=false via
// the optional overload below) if the string is unrecognized, callers that
// need to detect bad input should prefer try_from_string.
[[nodiscard]] Bucket from_string(std::string_view s) noexcept;

struct SunTimes {
    std::chrono::system_clock::time_point sunrise;
    std::chrono::system_clock::time_point sunset;
};

// Tunable bucket widths, defaulted to the spec's values. Exposed as
// parameters (not hardcoded constants) so config.md's "advanced" users could
// eventually override them without touching this module's logic - though no
// UI currently exposes this.
struct BucketWidths {
    std::chrono::minutes morning_length{150};   // 2h30m after sunrise
    std::chrono::minutes evening_before_sunset{60}; // Evening starts 1h before sunset
    std::chrono::minutes evening_after_sunset{30};  // Evening ends 30m after sunset
};

// Core pure function: given real sunrise/sunset and the current instant,
// return which bucket "now" falls into.
[[nodiscard]] Bucket resolve(const SunTimes& sun,
                              std::chrono::system_clock::time_point now,
                              const BucketWidths& widths = BucketWidths{}) noexcept;

} // namespace weatherpaper::time_of_day
