// weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp
//
// Module: wallpaper_engine_core (Section 3.3)
// Layer:  pure logic. Depends only on time_of_day, scaling_and_fit and
//         tag_system (all pure/local-file modules) - explicitly NO
//         dependency on weather_fetch, no OS headers, no network.
//
// This module owns:
//   1. Condition - the weather-condition vocabulary. It lives HERE (not in
//      weather_fetch) so that this pure-logic layer never has to depend on
//      the networking module; weather_fetch instead depends on and returns
//      this type (see weather_fetch.hpp). That keeps the dependency arrow
//      pointing from "network" -> "pure logic", never the reverse, which is
//      what makes wallpaper_engine_core unit-testable with zero network and
//      zero OS access.
//   2. Resolver - combines Condition + time_of_day::Bucket into a lookup
//      key, queries tag_system::TagIndex, and returns a fully-resolved
//      wallpaper choice (file path + per-asset fit mode) for the platform
//      layer to display. It never opens files or touches pixels.
//   3. CrossfadeScheduler - pure timing state machine for transition
//      logic ("when to start blending from one state to the next" -
//      Section 3.3). Actual pixel blending happens in render_engine;
//      this only computes *when* and *how far along* a transition is.
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "weatherpaper/tag_system/tag_system.hpp"
#include "weatherpaper/time_of_day/time_of_day.hpp"

namespace weatherpaper::wallpaper_engine_core {

enum class Condition {
    Sunny,
    Cloudy,
    Rain,
    Snow,
    Storm,
    Fog,
    Clear
};

[[nodiscard]] std::string_view to_string(Condition c) noexcept;
[[nodiscard]] Condition condition_from_string(std::string_view s) noexcept; // default: Clear

// -----------------------------------------------------------------------
// Resolver: weather + time -> concrete asset
// -----------------------------------------------------------------------

struct ResolvedWallpaper {
    std::string asset_id;
    std::string file_path;
    tag_system::AssetType type = tag_system::AssetType::Image;
    scaling_and_fit::FitMode fit_mode = scaling_and_fit::FitMode::Fill;
    std::vector<std::string> matched_tags;
    // True if no asset was tagged with BOTH the exact condition and exact
    // time bucket, and the resolver had to widen the search (see .cpp for
    // the fallback chain). Surfaced so the UI/log can tell the user
    // "no rainy_night wallpaper found, showing a night wallpaper instead".
    bool used_fallback = false;
};

class Resolver {
public:
    explicit Resolver(tag_system::SelectionPolicy policy = tag_system::SelectionPolicy::Sequential)
        : policy_(policy) {}

    // Looks up (condition, bucket) against `index`. Fallback chain, widest
    // to narrowest match required, is:
    //   1. exact:      {condition_tag, time_tag}
    //   2. time-only:  {time_tag}          (any weather, right time of day)
    //   3. condition-only: {condition_tag} (right weather, any time)
    //   4. nullopt - caller (render_engine/asset_manager) is responsible
    //      for the ultimate seasonal-default fallback (Section 3.1), which
    //      requires filesystem/date access this pure module doesn't have.
    [[nodiscard]] std::optional<ResolvedWallpaper> resolve(
        tag_system::TagIndex& index,
        Condition condition,
        time_of_day::Bucket bucket) const;

    void set_selection_policy(tag_system::SelectionPolicy policy) { policy_ = policy; }
    [[nodiscard]] tag_system::SelectionPolicy selection_policy() const noexcept { return policy_; }

private:
    tag_system::SelectionPolicy policy_;
};

// -----------------------------------------------------------------------
// CrossfadeScheduler: pure transition timing (Section 3.3)
// -----------------------------------------------------------------------

class CrossfadeScheduler {
public:
    explicit CrossfadeScheduler(std::chrono::milliseconds duration = std::chrono::milliseconds(1200))
        : duration_(duration) {}

    // Call whenever the Resolver produces a new target asset id. If the id
    // differs from the currently-displayed one, this starts (or restarts)
    // a transition and returns true. If it's the same id, this is a no-op
    // and returns false - callers should NOT restart a crossfade just
    // because resolve() was called again with the same result (e.g. on
    // every weather poll where the condition didn't actually change).
    bool set_target(const std::string& new_asset_id, std::chrono::steady_clock::time_point now);

    // Blend factor in [0.0, 1.0]: 0.0 = fully showing previous_asset_id(),
    // 1.0 = fully showing current_asset_id() (transition complete).
    [[nodiscard]] double progress(std::chrono::steady_clock::time_point now) const;

    [[nodiscard]] bool is_transitioning(std::chrono::steady_clock::time_point now) const {
        return progress(now) < 1.0;
    }

    [[nodiscard]] const std::string& current_asset_id() const noexcept { return current_id_; }
    [[nodiscard]] std::optional<std::string> previous_asset_id() const noexcept { return previous_id_; }

    void set_duration(std::chrono::milliseconds d) { duration_ = d; }
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept { return duration_; }

private:
    std::string current_id_;
    std::optional<std::string> previous_id_;
    std::chrono::steady_clock::time_point transition_start_{};
    std::chrono::milliseconds duration_;
    bool has_target_ = false;
};

} // namespace weatherpaper::wallpaper_engine_core
