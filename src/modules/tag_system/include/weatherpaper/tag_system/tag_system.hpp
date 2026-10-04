// weatherpaper/tag_system/tag_system.hpp
//
// Module: tag_system (Section 3.8)
// Layer:  pure logic + local file I/O (JSON on disk). No network, no OS
//         wallpaper APIs.
//
// This is the *single* source of truth for "which files exist as wallpaper
// assets and what tags/fit-mode do they carry". Every asset - bundled theme
// image, downloaded theme-pack image, or user-uploaded custom file - gets
// exactly one AssetRecord here, keyed by a stable string id.
//
// wallpaper_engine_core NEVER touches the filesystem directly to look for
// assets; it always queries this module (Section 3.3), so a future
// contributor can swap the storage backend (e.g. JSON -> SQLite, as the
// spec allows either) without touching engine-resolution logic, as long as
// this header's contract is preserved.
//
// ASSUMPTION: storage backend is JSON, not SQLite. The spec allows either
// ("local JSON/SQLite index"). JSON was chosen to avoid adding a SQLite
// build dependency to a project whose #2 design goal is a <25MB installer
// and minimal footprint; a personal wallpaper library is at most a few
// thousand rows, well within JSON's comfortable range. If contributors need
// SQLite-scale querying later, TagIndex's public interface below is small
// enough to reimplement against SQLite without changing callers.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"

namespace weatherpaper::tag_system {

enum class AssetType {
    Image,
    Video
};

[[nodiscard]] const char* to_string(AssetType type) noexcept;
[[nodiscard]] AssetType asset_type_from_string(const char* s) noexcept; // default: Image

// Standard tag vocabulary (Sections 3.1/3.2/3.8). Assets are not restricted
// to these - "free-text custom tags for advanced users" (Section 3.8) are
// fully supported by storing arbitrary strings - but the settings_ui tag
// editor presents these as the default checkbox list.
namespace standard_tags {
    // Weather condition tags (mirrors weather_fetch::Condition, Section 3.1)
    inline constexpr const char* kSunny  = "sunny";
    inline constexpr const char* kCloudy = "cloudy";
    inline constexpr const char* kRain   = "rain";
    inline constexpr const char* kSnow   = "snow";
    inline constexpr const char* kStorm  = "storm";
    inline constexpr const char* kFog    = "fog";
    inline constexpr const char* kClear  = "clear";
    // Time-of-day tags (mirrors time_of_day::Bucket, Section 3.2)
    inline constexpr const char* kMorning = "morning";
    inline constexpr const char* kDay     = "day";
    inline constexpr const char* kEvening = "evening";
    inline constexpr const char* kNight   = "night";

    [[nodiscard]] const std::vector<std::string>& all_weather_tags();
    [[nodiscard]] const std::vector<std::string>& all_time_tags();
} // namespace standard_tags

struct AssetRecord {
    std::string id;                 // stable, unique (e.g. sanitized filename or uuid)
    std::string file_path;          // absolute path on disk, or path relative to the
                                     // owning theme pack's install directory - see
                                     // "path resolution" note in tag_system.cpp
    AssetType type = AssetType::Image;
    std::unordered_set<std::string> tags;
    scaling_and_fit::FitMode fit_mode = scaling_and_fit::FitMode::Fill; // Section 3.4a default
    std::string source_pack_id;     // "" for user gallery assets, else owning theme pack id

    [[nodiscard]] bool has_tag(const std::string& tag) const noexcept {
        return tags.find(tag) != tags.end();
    }
};

// How to pick when multiple assets match the same weather+time key
// (Section 3.8: "engine picks one - sequentially, randomly, or user-pinned,
// configurable"). Lives here (not wallpaper_engine_core) because "pinned"
// requires per-key state that belongs with the index; wallpaper_engine_core
// asks TagIndex::resolve_selection() for the final pick rather than
// re-implementing the policy itself.
enum class SelectionPolicy {
    Sequential, // round-robin through matches, advancing each time the
                // same key is resolved (state kept per-key in TagIndex)
    Random,
    Pinned      // always the user's explicitly chosen asset id for that key,
                // set via TagIndex::pin()
};

class TagIndex {
public:
    TagIndex() = default;

    // Loads a JSON index file. Returns an empty-but-valid TagIndex (never
    // throws) if the file doesn't exist yet - first-run behavior. Malformed
    // JSON is logged by the caller (this module has no logging dependency
    // by design) and also yields an empty index rather than crashing.
    [[nodiscard]] static TagIndex load_from_file(const std::string& path, bool* out_ok = nullptr);

    // Writes the index atomically (write to temp file + rename) so a crash
    // mid-write never corrupts the on-disk index.
    [[nodiscard]] bool save_to_file(const std::string& path) const;

    // --- CRUD -------------------------------------------------------------
    void upsert(AssetRecord record);
    bool remove(const std::string& id);
    [[nodiscard]] std::optional<AssetRecord> get(const std::string& id) const;
    [[nodiscard]] const std::vector<AssetRecord>& list_all() const noexcept { return records_; }

    void set_tags(const std::string& id, std::unordered_set<std::string> tags);
    void add_tag(const std::string& id, const std::string& tag);
    void remove_tag(const std::string& id, const std::string& tag);
    void set_fit_mode(const std::string& id, scaling_and_fit::FitMode mode);

    // --- Querying -----------------------------------------------------
    // Returns every asset whose tag set is a superset of `required_tags`
    // (AND match - Section 3.3's "rainy_night" lookup key is really two
    // required tags: the weather condition and the time bucket).
    [[nodiscard]] std::vector<AssetRecord> find_matching_all(
        const std::vector<std::string>& required_tags) const;

    // Applies `policy` to pick a single asset out of find_matching_all()'s
    // result for a given lookup key string (e.g. "rain+night", used only as
    // the Sequential/Pinned state key - callers typically pass
    // weather_tag + "+" + time_tag). Returns nullopt if there are no
    // matches at all.
    [[nodiscard]] std::optional<AssetRecord> resolve_selection(
        const std::string& lookup_key,
        const std::vector<std::string>& required_tags,
        SelectionPolicy policy);

    // Explicitly pins an asset id for a lookup key (SelectionPolicy::Pinned).
    void pin(const std::string& lookup_key, const std::string& asset_id);
    void unpin(const std::string& lookup_key);

private:
    std::vector<AssetRecord> records_;
    // Round-robin cursor per lookup key, for SelectionPolicy::Sequential.
    std::unordered_map<std::string, std::size_t> round_robin_cursor_;
    // Explicit pins per lookup key, for SelectionPolicy::Pinned.
    std::unordered_map<std::string, std::string> pins_;
};

} // namespace weatherpaper::tag_system
