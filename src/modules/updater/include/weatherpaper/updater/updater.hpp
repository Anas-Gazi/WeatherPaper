// weatherpaper/updater/updater.hpp
//
// Module: updater (Section 3.10)
// Layer:  network + local disk staging. Reuses weather_fetch::IHttpClient
// and asset_manager's checksum/signature verification functions rather
// than duplicating them (Section 3.10: "All update-related network calls
// go through the same HTTPS + checksum/signature verification path as
// asset_manager downloads - do not build two separate download/verify code
// paths").
//
// INDEPENDENT VERSIONING (Section 2.5 / 3.10): core engine, UI, and asset
// catalog are versioned and updated independently. This module identifies
// each by a plain string "component_id" ("core", "ui", "asset_catalog")
// rather than a closed enum, so a future contributor can introduce a new
// independently-updatable component without an ABI-breaking header change.
//
// APPLYING AN UPDATE - SCOPE NOTE: actually replacing a *running* native
// executable is fundamentally OS/packaging-specific (Windows typically
// needs a separate elevated updater process or an installer like Squirrel/
// MSIX; Linux typically defers to the system package manager or an
// AppImage's own self-update mechanism). This module implements and fully
// unit-tests the OS-agnostic parts - version comparison, manifest parsing,
// download+verify, staging, and rollback bookkeeping via plain directory
// swaps - and stops at "verified update content is staged on disk,
// atomically swapped into the install location, with the previous version
// kept as a backup for rollback". Wiring that into a real installer per OS
// is flagged as follow-up integration work (see CONTRIBUTING.md
// "Implementing the platform installer/updater launcher").
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "weatherpaper/weather_fetch/weather_fetch.hpp" // reused IHttpClient

namespace weatherpaper::updater {

// ===========================================================================
// Semantic version comparison (pure logic)
// ===========================================================================
struct SemVer {
    int major = 0, minor = 0, patch = 0;
};

[[nodiscard]] std::optional<SemVer> parse_semver(const std::string& s) noexcept;
// Returns <0 if a<b, 0 if equal, >0 if a>b. Unparseable input sorts as
// "oldest possible" (0.0.0) rather than throwing, so a malformed version
// string never blocks an update check from at least trying the comparison.
[[nodiscard]] int compare_semver(const SemVer& a, const SemVer& b) noexcept;
[[nodiscard]] bool is_newer_version(const std::string& current, const std::string& remote) noexcept;

// ===========================================================================
// Version manifest model
// ===========================================================================
struct PackageInfo {
    std::string component_id; // "core" / "ui" / "asset_catalog" / future components
    std::string version;
    std::string download_url;      // HTTPS
    std::string sha256_hex;
    std::string signature_base64;  // RSA-SHA256 signature over the raw package bytes
    std::uint64_t size_bytes = 0;
};

struct VersionManifest {
    std::unordered_map<std::string, PackageInfo> latest_by_component;
};

[[nodiscard]] std::optional<VersionManifest> parse_version_manifest(const std::string& json_body);

struct ComponentVersions {
    std::string core;
    std::string ui;
    std::string asset_catalog;

    [[nodiscard]] std::unordered_map<std::string, std::string> as_map() const {
        return {{"core", core}, {"ui", ui}, {"asset_catalog", asset_catalog}};
    }
};

// ===========================================================================
// Base64 (needed to turn a manifest's signature_base64 into raw bytes for
// asset_manager::verify_rsa_sha256_signature)
// ===========================================================================
[[nodiscard]] std::optional<std::string> base64_decode(const std::string& encoded);

// ===========================================================================
// UpdateManager: check / apply / rollback orchestration
// ===========================================================================

struct CheckResult {
    std::vector<std::string> components_with_updates; // e.g. {"core", "asset_catalog"}
    VersionManifest manifest;
};

[[nodiscard]] CheckResult check_for_updates(const VersionManifest& manifest,
                                              const ComponentVersions& current);

enum class ApplyResult {
    Success,
    NetworkError,
    ChecksumMismatch,
    SignatureInvalid,
    IoError,
    NoSuchComponent
};

class UpdateManager {
public:
    // `staging_root`/`install_root` follow the same "own subdirectory per
    // component" convention as asset_manager::ThemePackManager - e.g.
    // install_root/core/current_package.bin, install_root/core.backup/...
    UpdateManager(std::string staging_root, std::string install_root, std::string public_key_pem)
        : staging_root_(std::move(staging_root)),
          install_root_(std::move(install_root)),
          public_key_pem_(std::move(public_key_pem)) {}

    // Full pipeline for one component: download -> verify checksum ->
    // verify signature -> back up the currently-installed package -> stage
    // the new one -> atomically activate it. On ANY failure, the
    // previously-installed package (if any) is left untouched and still
    // active (Section 2.5: "never leave the app in a broken/unlaunchable
    // state").
    [[nodiscard]] ApplyResult apply_component_update(
        const PackageInfo& package, weather_fetch::IHttpClient& client);

    // Restores the most recent backup for `component_id` over the current
    // (presumed broken) package - Section 3.10: "Supports rollback if an
    // update fails to apply cleanly". Returns false if there is no backup
    // to roll back to.
    [[nodiscard]] bool rollback_component(const std::string& component_id);

    // Path to the currently-active package file for a component, or empty
    // if none has ever been installed via this manager.
    [[nodiscard]] std::string current_package_path(const std::string& component_id) const;

private:
    std::string staging_root_;
    std::string install_root_;
    std::string public_key_pem_;
};

} // namespace weatherpaper::updater
