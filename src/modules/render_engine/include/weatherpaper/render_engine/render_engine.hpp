// weatherpaper/render_engine/render_engine.hpp
//
// Module: render_engine (Section 3.4)
// Layer:  mixed - the pixel-compositing math (FrameCompositor) is pure and
//         fully unit-testable; RenderLoop is OS-integration orchestration
//         that depends on platform_common's interfaces (never a concrete
//         Windows/Linux header) plus wallpaper_engine_core for resolution/
//         timing and scaling_and_fit for placement geometry.
//
// STATIC MODE (Section 3.4, "universal fallback"): RenderLoop calls
// IPlatformWallpaper::set_static_wallpaper() directly - the OS does the
// actual image scaling/display, we never touch pixels ourselves. This is
// the fully-implemented, always-available path.
//
// ANIMATED/VIDEO MODE (Section 3.4, "opt-in higher tier"): decoding is via
// FFmpeg (IVideoDecoder, guarded by WEATHERPAPER_WITH_FFMPEG) and frames
// are composited into destination rectangles computed by
// scaling_and_fit::compute_fit(), then handed to an IVideoSurface - a
// platform-specific always-behind-icons window (the "WorkerW technique" on
// Windows Lively Wallpaper popularized, an X11 override-redirect/desktop
// window on Linux, or a layer-shell surface on Wayland via
// swaybg/mpvpaper's own approach - see backend.hpp). IVideoSurface's
// concrete Windows/Linux implementations are NOT included in this drop:
// they are real windowing/compositor integration work (creating and
// z-ordering a borderless window behind desktop icons) that needs a real
// display server to develop and test interactively, unlike everything
// else in this codebase. See docs/ARCHITECTURE.md "Verification status"
// and CONTRIBUTING.md "Implementing IVideoSurface" for exactly what's
// needed and why it's flagged as the top priority next contribution rather
// than shipped as an untested guess.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "weatherpaper/platform_common/platform_common.hpp"
#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"
#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"

namespace weatherpaper::render_engine {

// ===========================================================================
// FrameCompositor: pure pixel-level crossfade blending
// ===========================================================================

// Simple packed RGBA8888 frame buffer, row-major, no padding. Deliberately
// minimal (no external image-decoding dependency in this struct itself) so
// FrameCompositor is testable with hand-built synthetic buffers.
struct FrameBuffer {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // size == width*height*4

    [[nodiscard]] bool is_valid() const noexcept {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }
};

// Allocates a solid-color frame - primarily useful for tests and for the
// letterbox/pillarbox/off-surface fill color behind a Fit-mode placement.
[[nodiscard]] FrameBuffer make_solid_frame(int width, int height,
                                             std::uint8_t r, std::uint8_t g,
                                             std::uint8_t b, std::uint8_t a = 255);

// Alpha-blends `top` over `bottom` at the given opacity in [0,1] (0 = only
// bottom visible, 1 = only top visible), writing into a same-sized output
// buffer. `top` and `bottom` must be the same dimensions (this function
// does not resize - resizing/placement is scaling_and_fit's job, called
// beforehand by the caller to place each source frame into a
// destination-sized buffer via blit_into()). Returns an invalid (0x0)
// FrameBuffer if the two inputs' dimensions don't match.
[[nodiscard]] FrameBuffer crossfade_blend(const FrameBuffer& bottom, const FrameBuffer& top,
                                            double opacity);

// Blits `src` into a dst_width x dst_height destination buffer at the
// placement computed by scaling_and_fit::compute_fit(), filling any
// uncovered area (Fit mode's letterbox/pillarbox bars) with `background`.
// For FitMode::Tile, `placement` is applied repeatedly using the
// FitResult's `tiles` list - callers pass the full FitResult, not just one
// Rect, via the overload below.
[[nodiscard]] FrameBuffer blit_into(const FrameBuffer& src,
                                      const scaling_and_fit::FitResult& fit,
                                      int dst_width, int dst_height,
                                      std::uint8_t bg_r = 0, std::uint8_t bg_g = 0,
                                      std::uint8_t bg_b = 0);

// ===========================================================================
// Video decode (Section 3.4 animated/opt-in tier)
// ===========================================================================

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;
    virtual bool open(const std::string& file_path) = 0;
    // Decodes and returns the next frame, looping back to the start
    // automatically at end-of-stream (Section 4: "a short looped video/
    // GIF"). Returns nullopt only on a genuine decode error.
    virtual std::optional<FrameBuffer> next_frame() = 0;
    virtual void close() = 0;
};

#if defined(WEATHERPAPER_WITH_FFMPEG)
// Real decoder, implemented against libavformat/libavcodec/libswscale
// (Section 3.4: "lightweight decode path (e.g. FFmpeg for decode)").
class FfmpegVideoDecoder : public IVideoDecoder {
public:
    ~FfmpegVideoDecoder() override;
    bool open(const std::string& file_path) override;
    std::optional<FrameBuffer> next_frame() override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif

// ===========================================================================
// IVideoSurface: platform-specific always-behind-icons render target
// ===========================================================================
//
// NOT IMPLEMENTED IN THIS DROP - see file header "Verification status"
// note. The interface is specified now so render_engine's orchestration
// logic (RenderLoop below) and its pause/resume behavior can be fully
// written, wired, and unit-tested today against a MockVideoSurface,
// leaving exactly one well-defined, well-documented integration task per
// OS for a follow-up contribution.
class IVideoSurface {
public:
    virtual ~IVideoSurface() = default;
    virtual bool create(const platform_common::MonitorInfo& monitor) = 0;
    virtual void present(const FrameBuffer& frame) = 0;
    virtual void destroy() = 0;
};

// ===========================================================================
// RenderLoop: orchestration (Section 3.4 pause/resume behavior)
// ===========================================================================

struct RenderLoopConfig {
    bool animated_enabled = false;       // Section 2.3/4: opt-in higher tier
    bool pause_when_locked = true;       // Section 3.4
    bool pause_when_fullscreen = true;   // Section 3.4
    bool pause_on_battery_saver = true;  // Section 3.4
};

// Pure decision function (easily unit-tested without any real hooks/OS
// state): given the current PlatformHooks-reported conditions and the
// user's RenderLoopConfig, should animated rendering run right now?
[[nodiscard]] bool should_render_animated_frame(
    const RenderLoopConfig& config,
    bool is_locked,
    bool is_fullscreen_app_active,
    platform_common::PowerState power_state);

} // namespace weatherpaper::render_engine
