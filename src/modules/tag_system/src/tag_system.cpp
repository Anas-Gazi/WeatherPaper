#include "weatherpaper/tag_system/tag_system.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>

#include <nlohmann/json.hpp>

// PATH RESOLUTION NOTE (see AssetRecord::file_path in the header):
// Bundled/downloaded theme-pack assets store a path *relative* to their
// pack's install directory so packs remain relocatable; user-gallery
// assets store an absolute path into ~/.local/share/weatherpaper/gallery
// (Linux) or %LOCALAPPDATA%\WeatherPaper\gallery (Windows). Resolving
// relative -> absolute is asset_manager's job (it knows install
// directories); tag_system stores whatever string it was given verbatim.

namespace weatherpaper::tag_system {

using json = nlohmann::json;

const char* to_string(AssetType type) noexcept {
    switch (type) {
        case AssetType::Image: return "image";
        case AssetType::Video: return "video";
    }
    return "image";
}

AssetType asset_type_from_string(const char* s) noexcept {
    if (s != nullptr && std::strcmp(s, "video") == 0) return AssetType::Video;
    return AssetType::Image; // ASSUMPTION: unknown type strings default to Image
                              // (the more common/lower-risk asset kind).
}

namespace standard_tags {
const std::vector<std::string>& all_weather_tags() {
    static const std::vector<std::string> v = {
        kSunny, kCloudy, kRain, kSnow, kStorm, kFog, kClear
    };
    return v;
}
const std::vector<std::string>& all_time_tags() {
    static const std::vector<std::string> v = {
        kMorning, kDay, kEvening, kNight
    };
    return v;
}
} // namespace standard_tags

namespace {

json record_to_json(const AssetRecord& r) {
    json j;
    j["id"] = r.id;
    j["file"] = r.file_path;
    j["type"] = to_string(r.type);
    j["fit_mode"] = scaling_and_fit::to_string(r.fit_mode);
    j["source_pack_id"] = r.source_pack_id;
    j["tags"] = json::array();
    // Sort tags for deterministic, diff-friendly JSON output (nicer for
    // contributors reviewing hand-edited theme-pack manifests in PRs).
    std::vector<std::string> sorted_tags(r.tags.begin(), r.tags.end());
    std::sort(sorted_tags.begin(), sorted_tags.end());
    for (const auto& t : sorted_tags) j["tags"].push_back(t);
    return j;
}

std::optional<AssetRecord> record_from_json(const json& j) {
    if (!j.is_object()) return std::nullopt;
    AssetRecord r;
    // "id" is optional in hand-authored theme-pack manifests; default to
    // the filename so pack authors don't have to invent ids by hand.
    r.id = j.value("id", j.value("file", std::string{}));
    r.file_path = j.value("file", std::string{});
    if (r.id.empty() || r.file_path.empty()) return std::nullopt; // malformed entry, skip
    r.type = asset_type_from_string(j.value("type", std::string("image")).c_str());
    r.fit_mode = scaling_and_fit::fit_mode_from_string(
        j.value("fit_mode", std::string("fill")).c_str());
    r.source_pack_id = j.value("source_pack_id", std::string{});
    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) {
            if (t.is_string()) r.tags.insert(t.get<std::string>());
        }
    }
    return r;
}

} // namespace

TagIndex TagIndex::load_from_file(const std::string& path, bool* out_ok) {
    TagIndex idx;
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        // First launch / no user gallery yet - not an error.
        if (out_ok) *out_ok = true;
        return idx;
    }
    json j;
    try {
        in >> j;
    } catch (const std::exception&) {
        // Malformed JSON on disk: return an empty index rather than
        // crashing the whole app over one corrupt/hand-edited file.
        if (out_ok) *out_ok = false;
        return idx;
    }
    if (!j.contains("assets") || !j["assets"].is_array()) {
        if (out_ok) *out_ok = false;
        return idx;
    }
    for (const auto& entry : j["assets"]) {
        if (auto rec = record_from_json(entry)) {
            idx.records_.push_back(std::move(*rec));
        }
    }
    if (out_ok) *out_ok = true;
    return idx;
}

bool TagIndex::save_to_file(const std::string& path) const {
    json j;
    j["format_version"] = 1; // tag_system's own on-disk schema version -
                              // independent of core/assets/ui semver
                              // (see docs/ARCHITECTURE.md "Versioning").
    j["assets"] = json::array();
    for (const auto& r : records_) j["assets"].push_back(record_to_json(r));

    // Atomic write: temp file + rename, so a crash/power-loss mid-write
    // never leaves a truncated/corrupt index on disk (Section 2.5 spirit:
    // never leave the app in a broken state).
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << j.dump(2);
        if (!out.good()) return false;
    }
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }
    return true;
}

void TagIndex::upsert(AssetRecord record) {
    for (auto& r : records_) {
        if (r.id == record.id) {
            r = std::move(record);
            return;
        }
    }
    records_.push_back(std::move(record));
}

bool TagIndex::remove(const std::string& id) {
    const auto before = records_.size();
    records_.erase(std::remove_if(records_.begin(), records_.end(),
                                   [&](const AssetRecord& r) { return r.id == id; }),
                    records_.end());
    return records_.size() != before;
}

std::optional<AssetRecord> TagIndex::get(const std::string& id) const {
    for (const auto& r : records_) {
        if (r.id == id) return r;
    }
    return std::nullopt;
}

void TagIndex::set_tags(const std::string& id, std::unordered_set<std::string> tags) {
    for (auto& r : records_) {
        if (r.id == id) { r.tags = std::move(tags); return; }
    }
}

void TagIndex::add_tag(const std::string& id, const std::string& tag) {
    for (auto& r : records_) {
        if (r.id == id) { r.tags.insert(tag); return; }
    }
}

void TagIndex::remove_tag(const std::string& id, const std::string& tag) {
    for (auto& r : records_) {
        if (r.id == id) { r.tags.erase(tag); return; }
    }
}

void TagIndex::set_fit_mode(const std::string& id, scaling_and_fit::FitMode mode) {
    for (auto& r : records_) {
        if (r.id == id) { r.fit_mode = mode; return; }
    }
}

std::vector<AssetRecord> TagIndex::find_matching_all(
    const std::vector<std::string>& required_tags) const {
    std::vector<AssetRecord> out;
    for (const auto& r : records_) {
        bool all_present = true;
        for (const auto& tag : required_tags) {
            if (!r.has_tag(tag)) { all_present = false; break; }
        }
        if (all_present) out.push_back(r);
    }
    // Deterministic ordering (by id) so Sequential selection is stable
    // across process restarts even though records_ insertion order may
    // vary depending on which theme packs were installed in which order.
    std::sort(out.begin(), out.end(),
              [](const AssetRecord& a, const AssetRecord& b) { return a.id < b.id; });
    return out;
}

std::optional<AssetRecord> TagIndex::resolve_selection(
    const std::string& lookup_key,
    const std::vector<std::string>& required_tags,
    SelectionPolicy policy) {
    auto matches = find_matching_all(required_tags);
    if (matches.empty()) return std::nullopt;

    if (policy == SelectionPolicy::Pinned) {
        auto it = pins_.find(lookup_key);
        if (it != pins_.end()) {
            for (const auto& m : matches) {
                if (m.id == it->second) return m;
            }
            // Pinned asset no longer matches (e.g. its tags changed or it
            // was deleted) - fall through to Sequential as a safe default
            // rather than returning nothing.
        }
        policy = SelectionPolicy::Sequential;
    }

    if (policy == SelectionPolicy::Random) {
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> dist(0, matches.size() - 1);
        return matches[dist(rng)];
    }

    // Sequential (default): advance a per-key cursor each call, so repeated
    // resolutions of the same weather+time key cycle through all matches
    // instead of always showing the same one.
    auto& cursor = round_robin_cursor_[lookup_key];
    const auto pick = cursor % matches.size();
    cursor = (cursor + 1) % matches.size();
    return matches[pick];
}

void TagIndex::pin(const std::string& lookup_key, const std::string& asset_id) {
    pins_[lookup_key] = asset_id;
}

void TagIndex::unpin(const std::string& lookup_key) {
    pins_.erase(lookup_key);
}

} // namespace weatherpaper::tag_system
