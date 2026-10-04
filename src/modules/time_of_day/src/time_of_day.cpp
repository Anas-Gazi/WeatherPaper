#include "weatherpaper/time_of_day/time_of_day.hpp"

namespace weatherpaper::time_of_day {

std::string_view to_string(Bucket bucket) noexcept {
    switch (bucket) {
        case Bucket::Morning: return "morning";
        case Bucket::Day:     return "day";
        case Bucket::Evening: return "evening";
        case Bucket::Night:   return "night";
    }
    return "night"; // unreachable, keeps -Wreturn-type happy on all compilers
}

Bucket from_string(std::string_view s) noexcept {
    if (s == "morning") return Bucket::Morning;
    if (s == "day")     return Bucket::Day;
    if (s == "evening") return Bucket::Evening;
    return Bucket::Night; // ASSUMPTION: unknown/garbage tag strings degrade
                          // to Night rather than throwing, because this is
                          // called from tag-matching hot paths where an
                          // exception would be disproportionate to a typo
                          // in a user-edited tag.
}

Bucket resolve(const SunTimes& sun,
               std::chrono::system_clock::time_point now,
               const BucketWidths& widths) noexcept {
    // ASSUMPTION: if sunrise/sunset data is degenerate (e.g. sunrise >=
    // sunset, which can legitimately happen from a stale/corrupt cache, or
    // near-polar latitudes where the API may report equal or missing
    // values), we cannot compute meaningful buckets. Falling back to Night
    // is the safest default: Night wallpaper tags are guaranteed to exist
    // in the bundled theme (calm/dark imagery reads acceptably at any time
    // of day), whereas guessing Morning/Day could show a bright sunrise
    // image at 2am.
    if (sun.sunrise >= sun.sunset) {
        return Bucket::Night;
    }

    const auto morning_end = sun.sunrise + widths.morning_length;
    const auto evening_start = sun.sunset - widths.evening_before_sunset;
    const auto evening_end = sun.sunset + widths.evening_after_sunset;

    // ASSUMPTION: if the bucket widths are configured such that
    // morning_end would run past evening_start (extremely short daylight,
    // e.g. high-latitude winter), clamp Day to an empty range rather than
    // producing an inverted [morning_end, evening_start) interval — treat
    // that squeezed period as still-Morning until Evening begins.
    const auto day_start = morning_end;
    const auto day_end = (evening_start > day_start) ? evening_start : day_start;

    if (now < sun.sunrise) {
        return Bucket::Night;
    }
    if (now < day_start) {
        return Bucket::Morning;
    }
    if (now < day_end) {
        return Bucket::Day;
    }
    if (now < evening_end) {
        return Bucket::Evening;
    }
    return Bucket::Night;
}

} // namespace weatherpaper::time_of_day
