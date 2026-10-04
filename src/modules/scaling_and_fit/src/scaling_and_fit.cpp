#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"

#include <algorithm>
#include <cstring>

namespace weatherpaper::scaling_and_fit {

const char* to_string(FitMode mode) noexcept {
    switch (mode) {
        case FitMode::Fill:    return "fill";
        case FitMode::Fit:     return "fit";
        case FitMode::Stretch: return "stretch";
        case FitMode::Center:  return "center";
        case FitMode::Tile:    return "tile";
    }
    return "fill";
}

FitMode fit_mode_from_string(const char* s) noexcept {
    if (s == nullptr) return FitMode::Fill;
    if (std::strcmp(s, "fill") == 0)    return FitMode::Fill;
    if (std::strcmp(s, "fit") == 0)     return FitMode::Fit;
    if (std::strcmp(s, "stretch") == 0) return FitMode::Stretch;
    if (std::strcmp(s, "center") == 0)  return FitMode::Center;
    if (std::strcmp(s, "tile") == 0)    return FitMode::Tile;
    // ASSUMPTION: unknown/corrupt fit_mode strings in a hand-edited JSON
    // asset index fall back to Fill, matching the documented default for
    // "newly added assets" (Section 3.4a) rather than throwing - a bad tag
    // file should degrade gracefully, not crash the engine.
    return FitMode::Fill;
}

namespace {

Rect centered_rect(int w, int h, Size dst) {
    return Rect{
        (dst.width - w) / 2,
        (dst.height - h) / 2,
        w,
        h
    };
}

} // namespace

FitResult compute_fit(Size src, Size dst, FitMode mode) noexcept {
    FitResult result;
    result.mode = mode;

    if (src.width <= 0 || src.height <= 0 || dst.width <= 0 || dst.height <= 0) {
        // Degenerate input (e.g. a frame that hasn't finished decoding yet).
        // Return an all-zero placement; caller should skip drawing.
        return result;
    }

    const double src_ar = static_cast<double>(src.width) / static_cast<double>(src.height);
    const double dst_ar = static_cast<double>(dst.width) / static_cast<double>(dst.height);

    switch (mode) {
        case FitMode::Stretch: {
            result.placement = Rect{0, 0, dst.width, dst.height};
            break;
        }

        case FitMode::Center: {
            result.placement = centered_rect(src.width, src.height, dst);
            break;
        }

        case FitMode::Fit: {
            // Scale down (or up) so the whole source is visible, letterboxed.
            int w, h;
            if (src_ar > dst_ar) {
                // Source is relatively wider than destination -> width-bound.
                w = dst.width;
                h = static_cast<int>(static_cast<double>(dst.width) / src_ar + 0.5);
            } else {
                h = dst.height;
                w = static_cast<int>(static_cast<double>(dst.height) * src_ar + 0.5);
            }
            result.placement = centered_rect(w, h, dst);
            break;
        }

        case FitMode::Fill: {
            // Scale up so the source covers the destination, then let the
            // rect extend past dst bounds symmetrically (caller clips).
            int w, h;
            if (src_ar > dst_ar) {
                // Source relatively wider -> height-bound, width overflows.
                h = dst.height;
                w = static_cast<int>(static_cast<double>(dst.height) * src_ar + 0.5);
            } else {
                w = dst.width;
                h = static_cast<int>(static_cast<double>(dst.width) / src_ar + 0.5);
            }
            result.placement = centered_rect(w, h, dst);
            break;
        }

        case FitMode::Tile: {
            // Repeat the asset at native resolution, top-left anchored,
            // covering the whole destination surface.
            result.placement = Rect{0, 0, src.width, src.height};
            for (int y = 0; y < dst.height; y += src.height) {
                for (int x = 0; x < dst.width; x += src.width) {
                    result.tiles.push_back(Rect{x, y, src.width, src.height});
                }
            }
            break;
        }
    }

    return result;
}

bool exceeds_resolution_warning_threshold(Size src) noexcept {
    return src.width > kHighResWarningWidth || src.height > kHighResWarningHeight;
}

bool exceeds_video_size_warning_threshold(std::uint64_t file_bytes) noexcept {
    return file_bytes > kLargeVideoWarningBytes;
}

} // namespace weatherpaper::scaling_and_fit
