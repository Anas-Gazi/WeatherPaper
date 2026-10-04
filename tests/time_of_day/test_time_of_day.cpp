#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "weatherpaper/time_of_day/time_of_day.hpp"

using namespace weatherpaper::time_of_day;
using namespace std::chrono;

namespace {
// Helper: build a system_clock::time_point for a given hour:minute on an
// arbitrary fixed reference day, so tests read like wall-clock times without
// depending on any real calendar/timezone library.
system_clock::time_point at(int hour, int minute) {
    // Reference midnight, arbitrary fixed epoch offset - only relative
    // deltas matter for this module.
    static const auto midnight = system_clock::time_point{} + hours(24) * 20000;
    return midnight + hours(hour) + minutes(minute);
}
} // namespace

TEST_CASE("Morning bucket: from sunrise through sunrise+2h30m") {
    SunTimes sun{at(6, 0), at(20, 0)}; // sunrise 06:00, sunset 20:00
    CHECK(resolve(sun, at(6, 0)) == Bucket::Morning);   // exactly sunrise
    CHECK(resolve(sun, at(7, 30)) == Bucket::Morning);
    CHECK(resolve(sun, at(8, 29)) == Bucket::Morning);  // just before boundary
}

TEST_CASE("Day bucket: from sunrise+2h30m to sunset-1h") {
    SunTimes sun{at(6, 0), at(20, 0)};
    CHECK(resolve(sun, at(8, 30)) == Bucket::Day); // exactly the boundary
    CHECK(resolve(sun, at(12, 0)) == Bucket::Day);
    CHECK(resolve(sun, at(18, 59)) == Bucket::Day); // just before evening
}

TEST_CASE("Evening bucket: sunset-1h to sunset+30m") {
    SunTimes sun{at(6, 0), at(20, 0)};
    CHECK(resolve(sun, at(19, 0)) == Bucket::Evening); // exactly the boundary
    CHECK(resolve(sun, at(20, 0)) == Bucket::Evening); // exactly sunset
    CHECK(resolve(sun, at(20, 29)) == Bucket::Evening);
}

TEST_CASE("Night bucket: after evening ends and before sunrise") {
    SunTimes sun{at(6, 0), at(20, 0)};
    CHECK(resolve(sun, at(20, 30)) == Bucket::Night); // exactly evening end
    CHECK(resolve(sun, at(23, 59)) == Bucket::Night);
    CHECK(resolve(sun, at(0, 0)) == Bucket::Night);
    CHECK(resolve(sun, at(5, 59)) == Bucket::Night); // just before sunrise
}

TEST_CASE("Degenerate sunrise>=sunset falls back to Night (safe default)") {
    SunTimes sun{at(12, 0), at(12, 0)}; // equal - malformed/polar edge case
    CHECK(resolve(sun, at(12, 0)) == Bucket::Night);
    CHECK(resolve(sun, at(0, 0)) == Bucket::Night);

    SunTimes inverted{at(20, 0), at(6, 0)}; // sunrise after sunset - corrupt cache
    CHECK(resolve(inverted, at(21, 0)) == Bucket::Night);
}

TEST_CASE("Very short daylight window squeezes Day out without crashing") {
    // sunrise 06:00, sunset 06:40 -> morning_length (2h30m) alone already
    // overruns evening_start (sunset - 1h = 05:40, which is before sunrise).
    SunTimes sun{at(6, 0), at(6, 40)};
    CHECK_NOTHROW(resolve(sun, at(6, 10)));
    // Should still resolve to *something* sane (Morning, per the clamp rule),
    // never Day with an inverted interval.
    CHECK(resolve(sun, at(6, 10)) == Bucket::Morning);
}

TEST_CASE("Custom BucketWidths are respected") {
    SunTimes sun{at(6, 0), at(20, 0)};
    BucketWidths widths;
    widths.morning_length = minutes(60);       // 1h morning instead of 2h30m
    widths.evening_before_sunset = minutes(30);
    widths.evening_after_sunset = minutes(15);

    CHECK(resolve(sun, at(6, 59), widths) == Bucket::Morning);
    CHECK(resolve(sun, at(7, 0), widths) == Bucket::Day);
    CHECK(resolve(sun, at(19, 30), widths) == Bucket::Evening);
    CHECK(resolve(sun, at(20, 16), widths) == Bucket::Night);
}

TEST_CASE("to_string / from_string round-trip and tag-system compatibility") {
    CHECK(to_string(Bucket::Morning) == "morning");
    CHECK(to_string(Bucket::Day) == "day");
    CHECK(to_string(Bucket::Evening) == "evening");
    CHECK(to_string(Bucket::Night) == "night");

    CHECK(from_string("morning") == Bucket::Morning);
    CHECK(from_string("day") == Bucket::Day);
    CHECK(from_string("evening") == Bucket::Evening);
    CHECK(from_string("night") == Bucket::Night);

    // Unknown tag text degrades to Night rather than throwing (see .cpp).
    CHECK(from_string("bogus-tag") == Bucket::Night);
}
