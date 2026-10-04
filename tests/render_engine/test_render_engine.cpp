#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "weatherpaper/render_engine/render_engine.hpp"

using namespace weatherpaper::render_engine;
using weatherpaper::scaling_and_fit::compute_fit;
using weatherpaper::scaling_and_fit::FitMode;
using weatherpaper::scaling_and_fit::Size;

TEST_CASE("make_solid_frame produces a valid, uniformly-colored buffer") {
    auto fb = make_solid_frame(4, 3, 10, 20, 30, 255);
    REQUIRE(fb.is_valid());
    CHECK(fb.pixels.size() == 4u * 3u * 4u);
    for (std::size_t i = 0; i < fb.pixels.size(); i += 4) {
        CHECK(fb.pixels[i + 0] == 10);
        CHECK(fb.pixels[i + 1] == 20);
        CHECK(fb.pixels[i + 2] == 30);
        CHECK(fb.pixels[i + 3] == 255);
    }
}

TEST_CASE("crossfade_blend at opacity 0 returns bottom, at 1 returns top") {
    auto bottom = make_solid_frame(2, 2, 255, 0, 0, 255);
    auto top = make_solid_frame(2, 2, 0, 255, 0, 255);

    auto at0 = crossfade_blend(bottom, top, 0.0);
    REQUIRE(at0.is_valid());
    CHECK(at0.pixels[0] == 255);
    CHECK(at0.pixels[1] == 0);

    auto at1 = crossfade_blend(bottom, top, 1.0);
    CHECK(at1.pixels[0] == 0);
    CHECK(at1.pixels[1] == 255);
}

TEST_CASE("crossfade_blend at 0.5 is the midpoint average") {
    auto bottom = make_solid_frame(1, 1, 0, 0, 0, 255);
    auto top = make_solid_frame(1, 1, 200, 200, 200, 255);
    auto mid = crossfade_blend(bottom, top, 0.5);
    REQUIRE(mid.is_valid());
    CHECK(mid.pixels[0] == 100);
}

TEST_CASE("crossfade_blend clamps out-of-range opacity") {
    auto bottom = make_solid_frame(1, 1, 0, 0, 0, 255);
    auto top = make_solid_frame(1, 1, 255, 255, 255, 255);
    auto below = crossfade_blend(bottom, top, -5.0);
    CHECK(below.pixels[0] == 0); // clamped to 0.0 -> pure bottom
    auto above = crossfade_blend(bottom, top, 5.0);
    CHECK(above.pixels[0] == 255); // clamped to 1.0 -> pure top
}

TEST_CASE("crossfade_blend rejects mismatched dimensions rather than crashing") {
    auto a = make_solid_frame(4, 4, 0, 0, 0, 255);
    auto b = make_solid_frame(8, 8, 0, 0, 0, 255);
    auto result = crossfade_blend(a, b, 0.5);
    CHECK_FALSE(result.is_valid());
}

TEST_CASE("blit_into with Stretch fills the entire destination with source colors") {
    auto src = make_solid_frame(10, 10, 42, 42, 42, 255);
    auto fit = compute_fit(Size{10, 10}, Size{20, 20}, FitMode::Stretch);
    auto dst = blit_into(src, fit, 20, 20);
    REQUIRE(dst.is_valid());
    // Sample center pixel - should be the source color, not the background.
    std::size_t center = (static_cast<std::size_t>(10) * 20 + 10) * 4;
    CHECK(dst.pixels[center] == 42);
}

TEST_CASE("blit_into with Fit leaves the background color visible in letterbox bars") {
    // Source is much wider than destination -> letterboxed top/bottom.
    auto src = make_solid_frame(2000, 500, 255, 0, 0, 255); // wide red source
    auto fit = compute_fit(Size{2000, 500}, Size{1000, 1000}, FitMode::Fit);
    auto dst = blit_into(src, fit, 1000, 1000, /*bg*/ 0, 0, 0);
    REQUIRE(dst.is_valid());
    // Top-left corner should be background (black), since Fit here only
    // covers the vertical middle band.
    CHECK(dst.pixels[0] == 0);
    // A pixel in the vertical center should be the source color (red).
    std::size_t mid_row = static_cast<std::size_t>(500);
    std::size_t idx = (mid_row * 1000 + 500) * 4;
    CHECK(dst.pixels[idx] == 255);
}

TEST_CASE("blit_into with Tile covers the whole destination with repeated tiles") {
    auto src = make_solid_frame(5, 5, 9, 9, 9, 255);
    auto fit = compute_fit(Size{5, 5}, Size{12, 12}, FitMode::Tile);
    auto dst = blit_into(src, fit, 12, 12, 0, 0, 0);
    REQUIRE(dst.is_valid());
    // Bottom-right corner (within the last partial tile) should still be
    // the tile color, not background - Tile must cover every pixel.
    std::size_t idx = (static_cast<std::size_t>(11) * 12 + 11) * 4;
    CHECK(dst.pixels[idx] == 9);
}

// --- should_render_animated_frame (Section 3.4 pause/resume) -------------

TEST_CASE("should_render_animated_frame: disabled config never renders") {
    RenderLoopConfig cfg;
    cfg.animated_enabled = false;
    CHECK_FALSE(should_render_animated_frame(cfg, false, false,
                                               weatherpaper::platform_common::PowerState::OnACPower));
}

TEST_CASE("should_render_animated_frame: pauses when locked") {
    RenderLoopConfig cfg;
    cfg.animated_enabled = true;
    cfg.pause_when_locked = true;
    CHECK_FALSE(should_render_animated_frame(cfg, /*locked=*/true, false,
                                               weatherpaper::platform_common::PowerState::OnACPower));
    CHECK(should_render_animated_frame(cfg, /*locked=*/false, false,
                                         weatherpaper::platform_common::PowerState::OnACPower));
}

TEST_CASE("should_render_animated_frame: pauses on fullscreen app") {
    RenderLoopConfig cfg;
    cfg.animated_enabled = true;
    cfg.pause_when_fullscreen = true;
    CHECK_FALSE(should_render_animated_frame(cfg, false, /*fullscreen=*/true,
                                               weatherpaper::platform_common::PowerState::OnACPower));
}

TEST_CASE("should_render_animated_frame: pauses on battery saver, not on normal battery") {
    RenderLoopConfig cfg;
    cfg.animated_enabled = true;
    cfg.pause_on_battery_saver = true;
    CHECK_FALSE(should_render_animated_frame(cfg, false, false,
                                               weatherpaper::platform_common::PowerState::OnBatterySaver));
    CHECK(should_render_animated_frame(cfg, false, false,
                                         weatherpaper::platform_common::PowerState::OnBatteryNormal));
}

TEST_CASE("should_render_animated_frame: user can disable individual pause toggles") {
    RenderLoopConfig cfg;
    cfg.animated_enabled = true;
    cfg.pause_when_locked = false;
    CHECK(should_render_animated_frame(cfg, /*locked=*/true, false,
                                         weatherpaper::platform_common::PowerState::OnACPower));
}
