#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"

namespace weatherpaper::wallpaper_engine_core {

std::string_view to_string(Condition c) noexcept {
    switch (c) {
        case Condition::Sunny:  return tag_system::standard_tags::kSunny;
        case Condition::Cloudy: return tag_system::standard_tags::kCloudy;
        case Condition::Rain:   return tag_system::standard_tags::kRain;
        case Condition::Snow:   return tag_system::standard_tags::kSnow;
        case Condition::Storm:  return tag_system::standard_tags::kStorm;
        case Condition::Fog:    return tag_system::standard_tags::kFog;
        case Condition::Clear:  return tag_system::standard_tags::kClear;
    }
    return tag_system::standard_tags::kClear;
}

Condition condition_from_string(std::string_view s) noexcept {
    if (s == tag_system::standard_tags::kSunny)  return Condition::Sunny;
    if (s == tag_system::standard_tags::kCloudy) return Condition::Cloudy;
    if (s == tag_system::standard_tags::kRain)   return Condition::Rain;
    if (s == tag_system::standard_tags::kSnow)   return Condition::Snow;
    if (s == tag_system::standard_tags::kStorm)  return Condition::Storm;
    if (s == tag_system::standard_tags::kFog)    return Condition::Fog;
    return Condition::Clear; // ASSUMPTION: unrecognized condition strings
                              // (e.g. a weather_fetch provider returning a
                              // code we don't map yet) degrade to Clear,
                              // the most visually-neutral bundled theme,
                              // rather than throwing mid-poll.
}

namespace {

ResolvedWallpaper to_resolved(const tag_system::AssetRecord& r, bool used_fallback) {
    ResolvedWallpaper out;
    out.asset_id = r.id;
    out.file_path = r.file_path;
    out.type = r.type;
    out.fit_mode = r.fit_mode;
    out.matched_tags.assign(r.tags.begin(), r.tags.end());
    out.used_fallback = used_fallback;
    return out;
}

} // namespace

std::optional<ResolvedWallpaper> Resolver::resolve(
    tag_system::TagIndex& index,
    Condition condition,
    time_of_day::Bucket bucket) const {

    const std::string condition_tag(to_string(condition));
    const std::string time_tag(time_of_day::to_string(bucket));

    // 1. Exact match: both weather condition and time-of-day bucket.
    {
        const std::string key = condition_tag + "+" + time_tag;
        if (auto pick = index.resolve_selection(key, {condition_tag, time_tag}, policy_)) {
            return to_resolved(*pick, /*used_fallback=*/false);
        }
    }

    // 2. Fallback: time-of-day only (any weather, right time of day).
    //    Keeps the wallpaper temporally correct even if the user's active
    //    theme pack doesn't cover every weather condition.
    {
        const std::string key = "any+" + time_tag;
        if (auto pick = index.resolve_selection(key, {time_tag}, policy_)) {
            return to_resolved(*pick, /*used_fallback=*/true);
        }
    }

    // 3. Fallback: condition only (right weather, any time of day).
    {
        const std::string key = condition_tag + "+any";
        if (auto pick = index.resolve_selection(key, {condition_tag}, policy_)) {
            return to_resolved(*pick, /*used_fallback=*/true);
        }
    }

    // 4. Nothing at all tagged appropriately - caller must apply the
    // seasonal-default / last-known-good fallback (Section 3.1), which
    // needs filesystem/date context this module intentionally doesn't have.
    return std::nullopt;
}

bool CrossfadeScheduler::set_target(const std::string& new_asset_id,
                                     std::chrono::steady_clock::time_point now) {
    if (has_target_ && new_asset_id == current_id_) {
        return false; // same target - do not restart the crossfade
    }
    previous_id_ = has_target_ ? std::optional<std::string>(current_id_) : std::nullopt;
    current_id_ = new_asset_id;
    transition_start_ = now;
    has_target_ = true;
    return true;
}

double CrossfadeScheduler::progress(std::chrono::steady_clock::time_point now) const {
    if (!has_target_ || !previous_id_.has_value()) {
        // No prior asset to blend from (first-ever wallpaper set) -> show
        // the target immediately at full opacity, no crossfade needed.
        return 1.0;
    }
    if (duration_.count() <= 0) return 1.0;

    const auto elapsed = now - transition_start_;
    if (elapsed <= std::chrono::steady_clock::duration::zero()) return 0.0;

    const double ratio = static_cast<double>(elapsed.count()) /
                          static_cast<double>(std::chrono::duration_cast<
                              std::chrono::steady_clock::duration>(duration_).count());
    if (ratio >= 1.0) return 1.0;
    if (ratio <= 0.0) return 0.0;
    return ratio;
}

} // namespace weatherpaper::wallpaper_engine_core
