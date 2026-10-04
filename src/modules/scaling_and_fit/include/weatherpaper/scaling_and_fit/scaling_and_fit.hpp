// weatherpaper/scaling_and_fit/scaling_and_fit.hpp
//
// Module: scaling_and_fit (Section 3.4a)
// Layer:  pure logic. No image/video decoding, no OS calls.
//
// Computes the geometry needed to place an asset of size (src_w, src_h) into
// a target surface of size (dst_w, dst_h) under one of five fit modes. This
// module is deliberately dumb: it never touches pixels. Two very different
// call sites reuse the same math:
//
//   * Images: render_engine/platform layer passes the resulting FitResult's
//     *mode* straight through to the native OS wallpaper API where possible
//     (Windows IDesktopWallpaper::SetPosition, GNOME "picture-options", KDE
//     "FillMode", XFCE "image-style") - the OS does the actual resampling.
//     See PLACEMENT NOTE below.
//   * Video: render_engine cannot delegate to the OS (frames are decoded and
//     composited manually - Section 3.4/3.4a), so it calls compute_fit()
//     directly, once per output size, and blits each decoded frame into the
//     returned destination rectangle, tiling/repeating as instructed for
//     Tile mode.
//
// PLACEMENT NOTE: OS-native wallpaper style enums are similar but not
// identical across platforms (e.g. GNOME's "spanned" has no Windows
// equivalent). platform_windows/platform_linux each own a small mapping
// table from FitMode to their native enum - that mapping is OS glue and
// intentionally lives in the platform module, not here, to keep this module
// free of any platform headers.
#pragma once

#include <cstdint>
#include <vector>

namespace weatherpaper::scaling_and_fit {

enum class FitMode {
    Fill,     // crop to fill, preserve aspect ratio, no distortion
    Fit,      // show whole asset, letterbox/pillarbox as needed
    Stretch,  // force exact size, may distort aspect ratio
    Center,   // original resolution, centered, unscaled
    Tile      // repeat original-resolution asset to fill the surface
};

[[nodiscard]] const char* to_string(FitMode mode) noexcept;
[[nodiscard]] FitMode fit_mode_from_string(const char* s) noexcept; // default: Fill

struct Size {
    int width = 0;
    int height = 0;
};

// Axis-aligned rectangle in destination-surface pixel coordinates. May be
// negative/oversized (e.g. a Fill crop extends past the surface bounds on
// purpose before clipping) - callers that blit must clip against
// [0,0,dst_w,dst_h] themselves; this module only computes geometry.
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// One tile placement, used only for FitMode::Tile, where the asset repeats
// at its native resolution across the destination surface.
struct FitResult {
    FitMode mode = FitMode::Fill;

    // For Fill / Fit / Stretch / Center: the single rectangle (in
    // destination coordinates) the *entire source image* should be drawn
    // into. For Fit, this rect is smaller than the destination and centered
    // (the caller fills the remaining letterbox/pillarbox area with black,
    // or a user-configurable bar color - render_engine's job, not ours).
    Rect placement;

    // For FitMode::Tile only: the list of destination rectangles (all the
    // same size as the source) needed to cover the destination surface.
    // Empty for every other mode.
    std::vector<Rect> tiles;
};

// Core pure function. dst must be > 0 in both dimensions; src of 0 in either
// dimension returns a degenerate empty FitResult (placement all-zero,
// no tiles) rather than dividing by zero - callers should treat that as
// "nothing to draw this frame" (e.g. a still-decoding video frame).
[[nodiscard]] FitResult compute_fit(Size src, Size dst, FitMode mode) noexcept;

// Convenience threshold checks used by asset_manager's upload-warning UI
// (Section 3.4a: "warn if >4K resolution or >100MB for video, but don't
// silently reject"). Kept here because the thresholds are fit/perf-related,
// not asset-management logic.
constexpr int kHighResWarningWidth = 3840;   // 4K UHD width
constexpr int kHighResWarningHeight = 2160;
constexpr std::uint64_t kLargeVideoWarningBytes = 100ull * 1024 * 1024; // 100 MB

[[nodiscard]] bool exceeds_resolution_warning_threshold(Size src) noexcept;
[[nodiscard]] bool exceeds_video_size_warning_threshold(std::uint64_t file_bytes) noexcept;

} // namespace weatherpaper::scaling_and_fit
