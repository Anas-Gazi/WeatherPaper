// weatherpaper/asset_manager/asset_manager.hpp
//
// Module: asset_manager (Section 3.7) + shared download/verify path used by
// updater (Section 3.10: "All update-related network calls go through the
// same HTTPS + checksum/signature verification path as asset_manager
// downloads - do not build two separate download/verify code paths").
//
// SECURITY (Section 5), enforced throughout this module:
//   * every network call goes through weather_fetch::IHttpClient, whose
//     production implementation (CurlHttpClient) rejects non-HTTPS URLs
//     outright - see that module's header. ASSUMPTION: IHttpClient is
//     reused from weather_fetch rather than duplicated here specifically
//     to satisfy the "one download/verify path" requirement above; a
//     dedicated `http_client` module extracted from weather_fetch would be
//     a cleaner long-term home for it (tracked in docs/ARCHITECTURE.md
//     "Known Limitations") but was out of scope to refactor in this pass.
//   * every downloaded file (theme-pack file, update package) is
//     SHA-256-checksummed against a value from a manifest that is itself
//     checksummed and, for official content, signature-verified.
//   * downloaded content is written to disk as data only, NEVER executed,
//     and every relative path from a manifest is validated to reject
//     path traversal (`..` components, absolute paths) BEFORE being joined
//     with an install directory - see validate_relative_path().
//   * uploaded gallery files are type-validated by inspecting the
//     extension against an explicit allow-list (Section 3.7); anything
//     else, including executables, is rejected outright.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "weatherpaper/tag_system/tag_system.hpp"
#include "weatherpaper/weather_fetch/weather_fetch.hpp" // reuses IHttpClient - see note above

namespace weatherpaper::asset_manager {

// ===========================================================================
// Checksum verification (SHA-256, via OpenSSL)
// ===========================================================================
[[nodiscard]] std::optional<std::string> sha256_hex_of_file(const std::string& file_path);
[[nodiscard]] std::string sha256_hex_of_bytes(const std::string& bytes);
[[nodiscard]] bool verify_checksum(const std::string& file_path, const std::string& expected_sha256_hex);

// ===========================================================================
// Signature verification (Section 5: "signature-verified against a public
// key bundled in the app", for official releases/packs)
// ===========================================================================
// Verifies an RSA-SHA256 PKCS#1 signature (`signature_der`, raw DER bytes)
// over `payload` using a PEM-encoded RSA public key (`public_key_pem`).
// ASSUMPTION: RSA-SHA256 was chosen over Ed25519 purely because OpenSSL's
// classic EVP RSA verify API is more uniformly available across the
// OpenSSL versions this project might be built against; swapping to
// Ed25519 later only touches this one function and its two call sites
// (theme-pack catalogs, update manifests).
[[nodiscard]] bool verify_rsa_sha256_signature(
    const std::string& payload,
    const std::string& signature_der,
    const std::string& public_key_pem);

// ===========================================================================
// Path safety (Section 5: reject path traversal from any manifest)
// ===========================================================================
// True only if `relative_path` is a "clean" relative path: no leading '/',
// no ".." path component anywhere, no embedded NUL, non-empty. Used before
// joining any manifest-supplied path with an install directory - this is
// the module's zip-slip / path-traversal defense.
[[nodiscard]] bool is_safe_relative_path(const std::string& relative_path) noexcept;

// ===========================================================================
// Upload validation (Section 3.7: file-type allow-list; Section 3.4a:
// warn-but-allow oversized uploads)
// ===========================================================================
enum class UploadFileType { Image, Video, Rejected };

// Classifies by extension only (case-insensitive) against the spec's
// allow-list: PNG/JPG/JPEG/WebP for images, MP4/WebM for video. Anything
// else - notably executables (.exe, .sh, .bat, .app, no extension, etc.) -
// is Rejected outright (Section 3.7: "reject executables or unknown types
// outright").
[[nodiscard]] UploadFileType classify_upload_extension(const std::string& file_path) noexcept;

struct UploadValidation {
    bool accepted = false;
    UploadFileType type = UploadFileType::Rejected;
    bool exceeds_warning_threshold = false; // Section 3.4a: >4K res or >100MB video
    std::string rejection_reason;           // populated only when accepted == false
};

// `pixel_width`/`pixel_height` may be 0/0 if unknown at validation time
// (e.g. before the image has been decoded) - resolution warnings are then
// simply skipped, file-size warnings still apply.
[[nodiscard]] UploadValidation validate_upload(
    const std::string& file_path,
    std::uint64_t file_size_bytes,
    int pixel_width = 0,
    int pixel_height = 0);

// ===========================================================================
// Remote theme-pack catalog & manifest model (Section 3.7)
// ===========================================================================

struct CatalogEntry {
    std::string id;
    std::string name;
    std::string version;          // independent semver per Section 2.5
    std::string manifest_url;     // HTTPS URL to this pack's ThemePackManifest JSON
    std::string manifest_sha256;  // checksum of the manifest JSON itself
    std::uint64_t total_size_bytes = 0; // for UI display before download
};

struct RemoteCatalog {
    std::vector<CatalogEntry> packs;
};

// Parses the catalog JSON. Does NOT verify the signature - callers must
// call verify_rsa_sha256_signature() on the raw response body themselves
// (kept separate so tests can exercise parsing and signature-checking
// independently).
[[nodiscard]] std::optional<RemoteCatalog> parse_catalog(const std::string& json_body);

struct PackFileEntry {
    std::string relative_path;   // validated via is_safe_relative_path() before use
    std::string sha256_hex;
    std::string download_url;    // HTTPS
    std::uint64_t size_bytes = 0;
};

struct ThemePackManifest {
    std::string id;
    std::string name;
    std::string version;
    std::vector<PackFileEntry> files;
    std::string tags_json_relative_path; // which downloaded file is the
                                           // tag_system-format asset index
                                           // to merge in after install
};

[[nodiscard]] std::optional<ThemePackManifest> parse_pack_manifest(const std::string& json_body);

// ===========================================================================
// ThemePackManager: install/uninstall/list orchestration
// ===========================================================================

enum class InstallResult {
    Success,
    NetworkError,
    ChecksumMismatch,
    SignatureInvalid,
    UnsafePath,      // a manifest entry failed is_safe_relative_path()
    IoError,
    AlreadyInstalled
};

class ThemePackManager {
public:
    // `install_root` is the directory each pack gets its own subdirectory
    // under (Section 3.9: platform-appropriate data dir, e.g.
    // ~/.local/share/weatherpaper/themepacks/<pack-id>/).
    explicit ThemePackManager(std::string install_root) : install_root_(std::move(install_root)) {}

    // Full pipeline: fetch manifest -> verify manifest checksum -> parse ->
    // fetch each file -> verify each file's checksum -> validate every
    // relative path -> write to disk -> merge the pack's tags.json into
    // `index` with source_pack_id = entry.id. On ANY failure, partially
    // written files for this pack are removed (no half-installed packs -
    // Section 2.5 "never leave the app in a broken state").
    [[nodiscard]] InstallResult install(
        const CatalogEntry& entry,
        weather_fetch::IHttpClient& client,
        tag_system::TagIndex& index);

    [[nodiscard]] bool uninstall(const std::string& pack_id, tag_system::TagIndex& index);

    [[nodiscard]] std::vector<std::string> list_installed_pack_ids() const;

private:
    std::string install_root_;
};

} // namespace weatherpaper::asset_manager
