#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"

using namespace weatherpaper::wallpaper_engine_core;
using namespace weatherpaper::tag_system;
using weatherpaper::time_of_day::Bucket;

namespace {
AssetRecord asset(std::string id, std::vector<std::string> tags) {
    AssetRecord r;
    r.id = id;
    r.file_path = "/themes/default/" + id + ".jpg";
    for (auto& t : tags) r.tags.insert(t);
    return r;
}
} // namespace

TEST_CASE("Condition <-> string round-trip matches tag_system's standard vocabulary") {
    CHECK(to_string(Condition::Rain) == "rain");
    CHECK(to_string(Condition::Storm) == "storm");
    CHECK(condition_from_string("snow") == Condition::Snow);
    CHECK(condition_from_string("totally-bogus") == Condition::Clear); // safe default
}

TEST_CASE("Resolver: exact condition+time match is preferred and marked non-fallback") {
    TagIndex idx;
    idx.upsert(asset("rain_night_1", {"rain", "night"}));
    idx.upsert(asset("clear_day_1", {"clear", "day"}));

    Resolver resolver;
    auto r = resolver.resolve(idx, Condition::Rain, Bucket::Night);
    REQUIRE(r.has_value());
    CHECK(r->asset_id == "rain_night_1");
    CHECK_FALSE(r->used_fallback);
}

TEST_CASE("Resolver: falls back to time-of-day-only match, flagged as fallback") {
    TagIndex idx;
    // No asset tagged storm+morning, but one tagged just "morning".
    idx.upsert(asset("generic_morning", {"morning", "cloudy"}));

    Resolver resolver;
    auto r = resolver.resolve(idx, Condition::Storm, Bucket::Morning);
    REQUIRE(r.has_value());
    CHECK(r->asset_id == "generic_morning");
    CHECK(r->used_fallback);
}

TEST_CASE("Resolver: falls back to condition-only match when no time-of-day match exists either") {
    TagIndex idx;
    idx.upsert(asset("rain_only", {"rain"})); // no time tag at all

    Resolver resolver;
    auto r = resolver.resolve(idx, Condition::Rain, Bucket::Evening);
    REQUIRE(r.has_value());
    CHECK(r->asset_id == "rain_only");
    CHECK(r->used_fallback);
}

TEST_CASE("Resolver: returns nullopt when the tag index has nothing usable at all") {
    TagIndex idx;
    idx.upsert(asset("unrelated", {"custom-tag-only"}));

    Resolver resolver;
    auto r = resolver.resolve(idx, Condition::Fog, Bucket::Day);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("Resolver: propagates fit_mode and asset type from the matched AssetRecord") {
    TagIndex idx;
    AssetRecord vid = asset("storm_vid", {"storm", "night"});
    vid.type = AssetType::Video;
    vid.fit_mode = weatherpaper::scaling_and_fit::FitMode::Fit;
    idx.upsert(vid);

    Resolver resolver;
    auto r = resolver.resolve(idx, Condition::Storm, Bucket::Night);
    REQUIRE(r.has_value());
    CHECK(r->type == AssetType::Video);
    CHECK(r->fit_mode == weatherpaper::scaling_and_fit::FitMode::Fit);
}

TEST_CASE("Resolver honors the configured SelectionPolicy") {
    TagIndex idx;
    idx.upsert(asset("a", {"sunny", "day"}));
    idx.upsert(asset("b", {"sunny", "day"}));

    Resolver resolver(SelectionPolicy::Sequential);
    auto first = resolver.resolve(idx, Condition::Sunny, Bucket::Day);
    auto second = resolver.resolve(idx, Condition::Sunny, Bucket::Day);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(first->asset_id != second->asset_id); // sequential cycles a, b
}

// --- CrossfadeScheduler -----------------------------------------------

TEST_CASE("CrossfadeScheduler: first-ever target has no previous asset, shows at full opacity") {
    CrossfadeScheduler sched;
    auto now = std::chrono::steady_clock::now();
    CHECK(sched.set_target("asset_a", now));
    CHECK(sched.progress(now) == doctest::Approx(1.0));
    CHECK_FALSE(sched.is_transitioning(now));
    CHECK(sched.current_asset_id() == "asset_a");
    CHECK_FALSE(sched.previous_asset_id().has_value());
}

TEST_CASE("CrossfadeScheduler: setting the same target twice does not restart the transition") {
    CrossfadeScheduler sched(std::chrono::milliseconds(1000));
    auto t0 = std::chrono::steady_clock::now();
    CHECK(sched.set_target("asset_a", t0));
    CHECK(sched.set_target("asset_b", t0 + std::chrono::milliseconds(500)));
    // Calling again with the SAME id later should be a no-op (return false)
    // and must not reset the in-progress transition's start time.
    auto mid_progress_before = sched.progress(t0 + std::chrono::milliseconds(600));
    CHECK_FALSE(sched.set_target("asset_b", t0 + std::chrono::milliseconds(600)));
    auto mid_progress_after = sched.progress(t0 + std::chrono::milliseconds(600));
    CHECK(mid_progress_before == doctest::Approx(mid_progress_after));
}

TEST_CASE("CrossfadeScheduler: progress interpolates linearly from 0 to 1 over duration") {
    CrossfadeScheduler sched(std::chrono::milliseconds(1000));
    auto t0 = std::chrono::steady_clock::now();
    sched.set_target("a", t0);
    sched.set_target("b", t0); // now there IS a previous asset ("a")

    CHECK(sched.progress(t0) == doctest::Approx(0.0));
    CHECK(sched.progress(t0 + std::chrono::milliseconds(250)) == doctest::Approx(0.25));
    CHECK(sched.progress(t0 + std::chrono::milliseconds(500)) == doctest::Approx(0.5));
    CHECK(sched.progress(t0 + std::chrono::milliseconds(1000)) == doctest::Approx(1.0));
    CHECK(sched.progress(t0 + std::chrono::milliseconds(5000)) == doctest::Approx(1.0)); // clamped
    CHECK(sched.previous_asset_id().value() == "a");
    CHECK(sched.current_asset_id() == "b");
}

TEST_CASE("CrossfadeScheduler: is_transitioning is true strictly during the window") {
    CrossfadeScheduler sched(std::chrono::milliseconds(1000));
    auto t0 = std::chrono::steady_clock::now();
    sched.set_target("a", t0);
    sched.set_target("b", t0);
    CHECK(sched.is_transitioning(t0 + std::chrono::milliseconds(1)));
    CHECK_FALSE(sched.is_transitioning(t0 + std::chrono::milliseconds(1000)));
    CHECK_FALSE(sched.is_transitioning(t0 + std::chrono::milliseconds(2000)));
}
