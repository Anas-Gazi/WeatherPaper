#include "weatherpaper/updater/updater.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "weatherpaper/asset_manager/asset_manager.hpp" // reused checksum/signature verification

namespace weatherpaper::updater {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ===========================================================================
// Semantic versioning
// ===========================================================================
std::optional<SemVer> parse_semver(const std::string& s) noexcept {
    int major = 0, minor = 0, patch = 0;
    if (std::sscanf(s.c_str(), "%d.%d.%d", &major, &minor, &patch) < 1) {
        return std::nullopt;
    }
    return SemVer{major, minor, patch};
}

int compare_semver(const SemVer& a, const SemVer& b) noexcept {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

bool is_newer_version(const std::string& current, const std::string& remote) noexcept {
    SemVer c = parse_semver(current).value_or(SemVer{0, 0, 0});
    SemVer r = parse_semver(remote).value_or(SemVer{0, 0, 0});
    return compare_semver(r, c) > 0;
}

// ===========================================================================
// Manifest parsing
// ===========================================================================
std::optional<VersionManifest> parse_version_manifest(const std::string& json_body) {
    json j;
    try {
        j = json::parse(json_body);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!j.contains("components") || !j["components"].is_array()) return std::nullopt;

    VersionManifest manifest;
    for (const auto& entry : j["components"]) {
        if (!entry.is_object()) continue;
        PackageInfo pkg;
        pkg.component_id = entry.value("component_id", std::string{});
        pkg.version = entry.value("version", std::string{});
        pkg.download_url = entry.value("download_url", std::string{});
        pkg.sha256_hex = entry.value("sha256", std::string{});
        pkg.signature_base64 = entry.value("signature_base64", std::string{});
        pkg.size_bytes = entry.value<std::uint64_t>("size_bytes", 0);
        if (pkg.component_id.empty() || pkg.download_url.empty() || pkg.sha256_hex.empty()) continue;
        if (pkg.download_url.rfind("https://", 0) != 0) continue; // Section 5
        manifest.latest_by_component[pkg.component_id] = pkg;
    }
    return manifest;
}

// ===========================================================================
// Base64
// ===========================================================================
std::optional<std::string> base64_decode(const std::string& encoded) {
    static const std::string kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto decode_char = [](char c) -> int {
        auto pos = kAlphabet.find(c);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    };

    std::string out;
    int val = 0, bits = -8;
    for (char c : encoded) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        int d = decode_char(c);
        if (d < 0) return std::nullopt; // invalid character - reject rather than guess
        val = (val << 6) + d;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ===========================================================================
// CheckResult
// ===========================================================================
CheckResult check_for_updates(const VersionManifest& manifest, const ComponentVersions& current) {
    CheckResult result;
    result.manifest = manifest;
    for (const auto& [component_id, current_version] : current.as_map()) {
        auto it = manifest.latest_by_component.find(component_id);
        if (it == manifest.latest_by_component.end()) continue;
        if (is_newer_version(current_version, it->second.version)) {
            result.components_with_updates.push_back(component_id);
        }
    }
    std::sort(result.components_with_updates.begin(), result.components_with_updates.end());
    return result;
}

// ===========================================================================
// UpdateManager
// ===========================================================================
namespace {
bool write_bytes(const std::string& path, const std::string& bytes) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}
} // namespace

ApplyResult UpdateManager::apply_component_update(const PackageInfo& package,
                                                     weather_fetch::IHttpClient& client) {
    auto resp = client.get(package.download_url);
    if (resp.network_error || resp.status_code != 200) return ApplyResult::NetworkError;

    const std::string actual_hash = asset_manager::sha256_hex_of_bytes(resp.body);
    std::string expected = package.sha256_hex, actual_lower = actual_hash;
    std::transform(expected.begin(), expected.end(), expected.begin(), ::tolower);
    std::transform(actual_lower.begin(), actual_lower.end(), actual_lower.begin(), ::tolower);
    if (actual_lower != expected) return ApplyResult::ChecksumMismatch;

    if (!public_key_pem_.empty() && !package.signature_base64.empty()) {
        auto sig_bytes = base64_decode(package.signature_base64);
        if (!sig_bytes.has_value() ||
            !asset_manager::verify_rsa_sha256_signature(resp.body, *sig_bytes, public_key_pem_)) {
            return ApplyResult::SignatureInvalid;
        }
    }

    // Stage to a temp location first (Section 2.5: never touch the active
    // install location with unverified/partial data).
    const std::string staged_path = staging_root_ + "/" + package.component_id + "/" +
                                      package.version + "/package.bin";
    if (!write_bytes(staged_path, resp.body)) return ApplyResult::IoError;

    // Back up the currently-active package (if any) before activating the
    // new one, so rollback_component() has something to restore.
    const std::string active_dir = install_root_ + "/" + package.component_id;
    const std::string backup_dir = install_root_ + "/" + package.component_id + ".backup";
    std::error_code ec;
    if (fs::exists(active_dir, ec)) {
        fs::remove_all(backup_dir, ec);
        fs::rename(active_dir, backup_dir, ec);
        if (ec) return ApplyResult::IoError;
    }

    fs::create_directories(active_dir, ec);
    const std::string active_package_path = active_dir + "/current_package.bin";
    if (!write_bytes(active_package_path, resp.body)) {
        // Roll back immediately - restore the backup so the app is never
        // left with a missing/broken active package (Section 2.5).
        fs::remove_all(active_dir, ec);
        if (fs::exists(backup_dir, ec)) fs::rename(backup_dir, active_dir, ec);
        return ApplyResult::IoError;
    }

    // Record the version string alongside the package for
    // current_package_path()/future manifest comparisons.
    write_bytes(active_dir + "/VERSION", package.version);

    return ApplyResult::Success;
}

bool UpdateManager::rollback_component(const std::string& component_id) {
    const std::string active_dir = install_root_ + "/" + component_id;
    const std::string backup_dir = install_root_ + "/" + component_id + ".backup";
    std::error_code ec;
    if (!fs::exists(backup_dir, ec)) return false;

    fs::remove_all(active_dir, ec);
    fs::rename(backup_dir, active_dir, ec);
    return !ec;
}

std::string UpdateManager::current_package_path(const std::string& component_id) const {
    const std::string path = install_root_ + "/" + component_id + "/current_package.bin";
    std::error_code ec;
    if (!fs::exists(path, ec)) return {};
    return path;
}

} // namespace weatherpaper::updater
