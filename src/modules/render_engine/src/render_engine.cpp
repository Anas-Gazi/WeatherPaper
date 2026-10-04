#include "weatherpaper/render_engine/render_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(WEATHERPAPER_WITH_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#endif

namespace weatherpaper::render_engine {

// ===========================================================================
// FrameCompositor
// ===========================================================================

FrameBuffer make_solid_frame(int width, int height, std::uint8_t r, std::uint8_t g,
                               std::uint8_t b, std::uint8_t a) {
    FrameBuffer fb;
    if (width <= 0 || height <= 0) return fb;
    fb.width = width;
    fb.height = height;
    fb.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < fb.pixels.size(); i += 4) {
        fb.pixels[i + 0] = r;
        fb.pixels[i + 1] = g;
        fb.pixels[i + 2] = b;
        fb.pixels[i + 3] = a;
    }
    return fb;
}

FrameBuffer crossfade_blend(const FrameBuffer& bottom, const FrameBuffer& top, double opacity) {
    FrameBuffer out;
    if (!bottom.is_valid() || !top.is_valid()) return out;
    if (bottom.width != top.width || bottom.height != top.height) return out; // mismatched sizes

    opacity = std::clamp(opacity, 0.0, 1.0);
    out.width = bottom.width;
    out.height = bottom.height;
    out.pixels.resize(bottom.pixels.size());

    for (std::size_t i = 0; i < out.pixels.size(); i += 4) {
        for (int c = 0; c < 4; ++c) {
            const double b = static_cast<double>(bottom.pixels[i + c]);
            const double t = static_cast<double>(top.pixels[i + c]);
            const double blended = b * (1.0 - opacity) + t * opacity;
            out.pixels[i + c] = static_cast<std::uint8_t>(std::lround(std::clamp(blended, 0.0, 255.0)));
        }
    }
    return out;
}

namespace {

void blit_rect(FrameBuffer& dst, const FrameBuffer& src, const scaling_and_fit::Rect& rect) {
    // Nearest-neighbor sampling (source pixel -> nearest dest pixel).
    // ASSUMPTION: nearest-neighbor rather than bilinear is a deliberate
    // performance/simplicity tradeoff for v1, consistent with the "small
    // footprint / low-end integrated GPU" design goal (Section 2.3) -
    // upgrading to bilinear is a self-contained, easily-tested follow-up
    // (CONTRIBUTING.md "good first issue" candidate) that only touches
    // this one function.
    for (int dy = 0; dy < rect.height; ++dy) {
        const int py = rect.y + dy;
        if (py < 0 || py >= dst.height) continue;
        const int sy = static_cast<int>((static_cast<double>(dy) / rect.height) * src.height);
        const int sy_clamped = std::clamp(sy, 0, src.height - 1);

        for (int dx = 0; dx < rect.width; ++dx) {
            const int px = rect.x + dx;
            if (px < 0 || px >= dst.width) continue;
            const int sx = static_cast<int>((static_cast<double>(dx) / rect.width) * src.width);
            const int sx_clamped = std::clamp(sx, 0, src.width - 1);

            const std::size_t src_idx = (static_cast<std::size_t>(sy_clamped) * src.width + sx_clamped) * 4;
            const std::size_t dst_idx = (static_cast<std::size_t>(py) * dst.width + px) * 4;
            std::memcpy(&dst.pixels[dst_idx], &src.pixels[src_idx], 4);
        }
    }
}

} // namespace

FrameBuffer blit_into(const FrameBuffer& src, const scaling_and_fit::FitResult& fit,
                        int dst_width, int dst_height,
                        std::uint8_t bg_r, std::uint8_t bg_g, std::uint8_t bg_b) {
    FrameBuffer dst = make_solid_frame(dst_width, dst_height, bg_r, bg_g, bg_b, 255);
    if (!src.is_valid() || !dst.is_valid()) return dst;

    if (fit.mode == scaling_and_fit::FitMode::Tile) {
        for (const auto& tile : fit.tiles) {
            blit_rect(dst, src, tile);
        }
        return dst;
    }

    if (fit.placement.width > 0 && fit.placement.height > 0) {
        blit_rect(dst, src, fit.placement);
    }
    return dst;
}

// ===========================================================================
// RenderLoop pause/resume decision logic (Section 3.4)
// ===========================================================================

bool should_render_animated_frame(const RenderLoopConfig& config, bool is_locked,
                                    bool is_fullscreen_app_active,
                                    platform_common::PowerState power_state) {
    if (!config.animated_enabled) return false; // static mode / opt-in not enabled
    if (config.pause_when_locked && is_locked) return false;
    if (config.pause_when_fullscreen && is_fullscreen_app_active) return false;
    if (config.pause_on_battery_saver &&
        power_state == platform_common::PowerState::OnBatterySaver) {
        return false;
    }
    return true;
}

// ===========================================================================
// FfmpegVideoDecoder (Section 3.4 - opt-in video decode path)
// ===========================================================================
#if defined(WEATHERPAPER_WITH_FFMPEG)

struct FfmpegVideoDecoder::Impl {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgba_frame = nullptr;
    AVPacket* packet = nullptr;
    int video_stream_index = -1;
    std::string path; // retained for automatic looping (Section 4: "short looped video")

    void reset() {
        if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
        if (frame) { av_frame_free(&frame); }
        if (rgba_frame) { av_frame_free(&rgba_frame); }
        if (packet) { av_packet_free(&packet); }
        if (codec_ctx) { avcodec_free_context(&codec_ctx); }
        if (fmt_ctx) { avformat_close_input(&fmt_ctx); }
        video_stream_index = -1;
    }

    bool open_internal(const std::string& file_path) {
        reset();
        path = file_path;

        if (avformat_open_input(&fmt_ctx, file_path.c_str(), nullptr, nullptr) != 0) return false;
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return false;

        for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = static_cast<int>(i);
                break;
            }
        }
        if (video_stream_index < 0) return false;

        const AVCodecParameters* params = fmt_ctx->streams[video_stream_index]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(params->codec_id);
        if (codec == nullptr) return false;

        codec_ctx = avcodec_alloc_context3(codec);
        if (codec_ctx == nullptr) return false;
        if (avcodec_parameters_to_context(codec_ctx, params) < 0) return false;
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;

        frame = av_frame_alloc();
        rgba_frame = av_frame_alloc();
        packet = av_packet_alloc();
        return frame != nullptr && rgba_frame != nullptr && packet != nullptr;
    }
};

FfmpegVideoDecoder::~FfmpegVideoDecoder() { close(); }

bool FfmpegVideoDecoder::open(const std::string& file_path) {
    impl_ = std::make_unique<Impl>();
    return impl_->open_internal(file_path);
}

std::optional<FrameBuffer> FfmpegVideoDecoder::next_frame() {
    if (!impl_ || impl_->fmt_ctx == nullptr) return std::nullopt;

    while (true) {
        int read_result = av_read_frame(impl_->fmt_ctx, impl_->packet);
        if (read_result < 0) {
            // End of stream - loop automatically (Section 4: "short looped
            // video/GIF" is the whole point of a wallpaper video).
            av_seek_frame(impl_->fmt_ctx, impl_->video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(impl_->codec_ctx);
            continue;
        }
        if (impl_->packet->stream_index != impl_->video_stream_index) {
            av_packet_unref(impl_->packet);
            continue;
        }

        if (avcodec_send_packet(impl_->codec_ctx, impl_->packet) < 0) {
            av_packet_unref(impl_->packet);
            return std::nullopt; // genuine decode error
        }
        av_packet_unref(impl_->packet);

        int recv = avcodec_receive_frame(impl_->codec_ctx, impl_->frame);
        if (recv == AVERROR(EAGAIN)) continue; // need more packets
        if (recv < 0) return std::nullopt;

        // Convert to packed RGBA8888 to match FrameBuffer's format.
        const int w = impl_->frame->width;
        const int h = impl_->frame->height;
        impl_->sws_ctx = sws_getCachedContext(
            impl_->sws_ctx, w, h, static_cast<AVPixelFormat>(impl_->frame->format),
            w, h, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (impl_->sws_ctx == nullptr) return std::nullopt;

        FrameBuffer out;
        out.width = w;
        out.height = h;
        out.pixels.resize(static_cast<std::size_t>(w) * h * 4);
        std::uint8_t* dst_slices[1] = {out.pixels.data()};
        int dst_strides[1] = {w * 4};
        sws_scale(impl_->sws_ctx, impl_->frame->data, impl_->frame->linesize, 0, h,
                   dst_slices, dst_strides);
        return out;
    }
}

void FfmpegVideoDecoder::close() {
    if (impl_) { impl_->reset(); impl_.reset(); }
}

#endif // WEATHERPAPER_WITH_FFMPEG

} // namespace weatherpaper::render_engine
