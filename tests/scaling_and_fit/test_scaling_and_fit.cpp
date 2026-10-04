#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"

using namespace weatherpaper::scaling_and_fit;

TEST_CASE("Stretch always fills the destination exactly") {
    auto r = compute_fit({1920, 1080}, {2560, 1440}, FitMode::Stretch);
    CHECK(r.placement.x == 0);
    CHECK(r.placement.y == 0);
    CHECK(r.placement.width == 2560);
    CHECK(r.placement.height == 1440);
    CHECK(r.tiles.empty());
}

TEST_CASE("Center keeps original resolution and centers it") {
    auto r = compute_fit({800, 600}, {1920, 1080}, FitMode::Center);
    CHECK(r.placement.width == 800);
    CHECK(r.placement.height == 600);
    CHECK(r.placement.x == (1920 - 800) / 2);
    CHECK(r.placement.y == (1080 - 600) / 2);
}

TEST_CASE("Center on an asset larger than the screen still reports true size (crop is caller's job)") {
    auto r = compute_fit({4000, 3000}, {1920, 1080}, FitMode::Center);
    CHECK(r.placement.width == 4000);
    CHECK(r.placement.height == 3000);
    // Negative origin is expected/valid - caller clips against the surface.
    CHECK(r.placement.x < 0);
    CHECK(r.placement.y < 0);
}

TEST_CASE("Fit: wider-than-screen source is width-bound with letterbox top/bottom") {
    // src AR = 2.0, dst AR = 16/9 ~= 1.78 -> src relatively wider -> width-bound
    auto r = compute_fit({2000, 1000}, {1920, 1080}, FitMode::Fit);
    CHECK(r.placement.width == 1920);
    CHECK(r.placement.height < 1080); // letterboxed vertically
    CHECK(r.placement.x == 0);
    CHECK(r.placement.y > 0);
}

TEST_CASE("Fit: taller-than-screen source is height-bound with pillarbox left/right") {
    // src AR = 0.5 (portrait), narrower than dst -> height-bound
    auto r = compute_fit({500, 1000}, {1920, 1080}, FitMode::Fit);
    CHECK(r.placement.height == 1080);
    CHECK(r.placement.width < 1920);
    CHECK(r.placement.x > 0);
    CHECK(r.placement.y == 0);
}

TEST_CASE("Fit: matching aspect ratio produces a full-bleed, no-bar placement") {
    auto r = compute_fit({1920, 1080}, {3840, 2160}, FitMode::Fit);
    CHECK(r.placement.x == 0);
    CHECK(r.placement.y == 0);
    CHECK(r.placement.width == 3840);
    CHECK(r.placement.height == 2160);
}

TEST_CASE("Fill: wider-than-screen source is height-bound and overflows width (crop sides)") {
    auto r = compute_fit({2000, 1000}, {1920, 1080}, FitMode::Fill);
    CHECK(r.placement.height == 1080);
    CHECK(r.placement.width > 1920);  // extends past both edges, caller crops
    CHECK(r.placement.x < 0);
    CHECK(r.placement.y == 0);
}

TEST_CASE("Fill: taller-than-screen source is width-bound and overflows height (crop top/bottom)") {
    auto r = compute_fit({500, 1000}, {1920, 1080}, FitMode::Fill);
    CHECK(r.placement.width == 1920);
    CHECK(r.placement.height > 1080);
    CHECK(r.placement.x == 0);
    CHECK(r.placement.y < 0);
}

TEST_CASE("Tile covers the destination with source-sized repeats, no gaps") {
    auto r = compute_fit({500, 500}, {1200, 900}, FitMode::Tile);
    REQUIRE(!r.tiles.empty());
    // Must cover width: ceil(1200/500) = 3 columns, height: ceil(900/500) = 2 rows
    int max_x = 0, max_y = 0;
    for (const auto& t : r.tiles) {
        CHECK(t.width == 500);
        CHECK(t.height == 500);
        max_x = std::max(max_x, t.x + t.width);
        max_y = std::max(max_y, t.y + t.height);
    }
    CHECK(max_x >= 1200);
    CHECK(max_y >= 900);
    CHECK(r.tiles.size() == 3u * 2u);
}

TEST_CASE("Degenerate zero-size source or destination never crashes, returns empty result") {
    CHECK_NOTHROW(compute_fit({0, 0}, {1920, 1080}, FitMode::Fill));
    auto r = compute_fit({0, 100}, {1920, 1080}, FitMode::Fit);
    CHECK(r.placement.width == 0);
    CHECK(r.tiles.empty());

    CHECK_NOTHROW(compute_fit({1920, 1080}, {0, 0}, FitMode::Fill));
}

TEST_CASE("fit_mode_from_string / to_string round-trip, unknown defaults to Fill") {
    CHECK(fit_mode_from_string("fill") == FitMode::Fill);
    CHECK(fit_mode_from_string("fit") == FitMode::Fit);
    CHECK(fit_mode_from_string("stretch") == FitMode::Stretch);
    CHECK(fit_mode_from_string("center") == FitMode::Center);
    CHECK(fit_mode_from_string("tile") == FitMode::Tile);
    CHECK(fit_mode_from_string("bogus") == FitMode::Fill);
    CHECK(fit_mode_from_string(nullptr) == FitMode::Fill);

    CHECK(std::string(to_string(FitMode::Fill)) == "fill");
    CHECK(std::string(to_string(FitMode::Tile)) == "tile");
}

TEST_CASE("Upload-warning thresholds match spec (>4K resolution, >100MB video)") {
    CHECK_FALSE(exceeds_resolution_warning_threshold({3840, 2160})); // exactly 4K: not over
    CHECK(exceeds_resolution_warning_threshold({3841, 2160}));
    CHECK(exceeds_resolution_warning_threshold({3840, 2161}));

    CHECK_FALSE(exceeds_video_size_warning_threshold(100ull * 1024 * 1024));
    CHECK(exceeds_video_size_warning_threshold(100ull * 1024 * 1024 + 1));
}
