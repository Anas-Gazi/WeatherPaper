#include "weatherpaper/asset_manager/asset_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#if defined(WEATHERPAPER_WITH_OPENSSL)
  #include <openssl/evp.h>
  #include <openssl/pem.h>
  #include <openssl/x509.h>
#endif

namespace weatherpaper::asset_manager {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ===========================================================================
// Checksums
// ===========================================================================
#if defined(WEATHERPAPER_WITH_OPENSSL)

namespace {
std::string bytes_to_hex(const unsigned char* data, unsigned int len) {
    static const char* kHex = "0123456789abcdef";
    std::string out(static_cast<std::size_t>(len) * 2, '0');
    for (unsigned int i = 0; i < len; ++i) {
        out[2 * i] = kHex[(data[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[data[i] & 0xF];
    }
    return out;
}
} // namespace

std::string sha256_hex_of_bytes(const std::string& bytes) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, bytes.data(), bytes.size());
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);
    return bytes_to_hex(digest, len);
}

std::optional<std::string> sha256_hex_of_file(const std::string& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in.is_open()) return std::nullopt;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

    char buf[1 << 16];
    while (in.good()) {
        in.read(buf, sizeof(buf));
        std::streamsize got = in.gcount();
        if (got > 0) EVP_DigestUpdate(ctx, buf, static_cast<std::size_t>(got));
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    EVP_DigestFinal_ex(ctx, digest, &len);
    EVP_MD_CTX_free(ctx);
    return bytes_to_hex(digest, len);
}

bool verify_rsa_sha256_signature(const std::string& payload, const std::string& signature_der,
                                   const std::string& public_key_pem) {
    BIO* bio = BIO_new_mem_buf(public_key_pem.data(), static_cast<int>(public_key_pem.size()));
    if (bio == nullptr) return false;
    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (pkey == nullptr) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1) {
        int rc = EVP_DigestVerify(
            ctx, reinterpret_cast<const unsigned char*>(signature_der.data()), signature_der.size(),
            reinterpret_cast<const unsigned char*>(payload.data()), payload.size());
        ok = (rc == 1);
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

#else // !WEATHERPAPER_WITH_OPENSSL

std::string sha256_hex_of_bytes(const std::string&) {
    return {}; // ASSUMPTION: builds without OpenSSL can parse manifests but
               // cannot verify anything - callers must treat an empty
               // digest as "verification unavailable", never as "verified".
}
std::optional<std::string> sha256_hex_of_file(const std::string&) { return std::nullopt; }
bool verify_rsa_sha256_signature(const std::string&, const std::string&, const std::string&) {
    return false; // fail closed when crypto is unavailable
}

#endif // WEATHERPAPER_WITH_OPENSSL

bool verify_checksum(const std::string& file_path, const std::string& expected_sha256_hex) {
    auto actual = sha256_hex_of_file(file_path);
    if (!actual.has_value()) return false;
    // Case-insensitive compare - manifests may use either case for hex digits.
    std::string a = *actual, b = expected_sha256_hex;
    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    return a == b;
}

// ===========================================================================
// Path safety
// ===========================================================================
bool is_safe_relative_path(const std::string& relative_path) noexcept {
    if (relative_path.empty()) return false;
    if (relative_path.front() == '/' || relative_path.front() == '\\') return false;
    if (relative_path.find('\0') != std::string::npos) return false;
    // Reject any ".." path component (zip-slip / path traversal defense -
    // Section 5). Checking token-by-token (split on '/' and '\\') is
    // deliberately stricter than a plain substring search for ".." (which
    // would also reject legitimate names like "my..file.jpg").
    std::string normalized = relative_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::istringstream iss(normalized);
    std::string token;
    while (std::getline(iss, token, '/')) {
        if (token == "..") return false;
        // A drive letter like "C:" sneaking in as a component would only
        // be meaningful on Windows, but we reject it everywhere for
        // consistency and defense-in-depth.
        if (token.size() >= 2 && token[1] == ':') return false;
    }
    return true;
}

// ===========================================================================
// Upload validation
// ===========================================================================
namespace {
std::string lowercase_extension(const std::string& path) {
    auto pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}
} // namespace

UploadFileType classify_upload_extension(const std::string& file_path) noexcept {
    std::string ext = lowercase_extension(file_path);
    // Section 3.7: "accepted image types: PNG/JPG/WebP; accepted video
    // types: MP4/WebM; reject executables or unknown types outright".
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp") return UploadFileType::Image;
    if (ext == "mp4" || ext == "webm") return UploadFileType::Video;
    return UploadFileType::Rejected;
}

UploadValidation validate_upload(const std::string& file_path, std::uint64_t file_size_bytes,
                                    int pixel_width, int pixel_height) {
    UploadValidation v;
    v.type = classify_upload_extension(file_path);
    if (v.type == UploadFileType::Rejected) {
        v.accepted = false;
        v.rejection_reason = "Unsupported file type. Accepted: PNG, JPG, WebP (images), "
                              "MP4, WebM (video).";
        return v;
    }
    v.accepted = true;

    if (v.type == UploadFileType::Video &&
        scaling_and_fit::exceeds_video_size_warning_threshold(file_size_bytes)) {
        v.exceeds_warning_threshold = true;
    }
    if (pixel_width > 0 && pixel_height > 0 &&
        scaling_and_fit::exceeds_resolution_warning_threshold({pixel_width, pixel_height})) {
        v.exceeds_warning_threshold = true;
    }
    return v;
}

// ===========================================================================
// Catalog & manifest parsing
// ===========================================================================
std::optional<RemoteCatalog> parse_catalog(const std::string& json_body) {
    json j;
    try {
        j = json::parse(json_body);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!j.contains("packs") || !j["packs"].is_array()) return std::nullopt;

    RemoteCatalog catalog;
    for (const auto& entry : j["packs"]) {
        if (!entry.is_object()) continue;
        CatalogEntry c;
        c.id = entry.value("id", std::string{});
        c.name = entry.value("name", std::string{});
        c.version = entry.value("version", std::string{});
        c.manifest_url = entry.value("manifest_url", std::string{});
        c.manifest_sha256 = entry.value("manifest_sha256", std::string{});
        c.total_size_bytes = entry.value<std::uint64_t>("total_size_bytes", 0);
        if (c.id.empty() || c.manifest_url.empty()) continue; // malformed, skip
        // SECURITY (Section 5): reject any catalog entry pointing at a
        // non-HTTPS manifest URL outright, at parse time - defense in
        // depth alongside IHttpClient's own HTTPS-only enforcement.
        if (c.manifest_url.rfind("https://", 0) != 0) continue;
        catalog.packs.push_back(std::move(c));
    }
    return catalog;
}

std::optional<ThemePackManifest> parse_pack_manifest(const std::string& json_body) {
    json j;
    try {
        j = json::parse(json_body);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!j.contains("id") || !j.contains("files") || !j["files"].is_array()) return std::nullopt;

    ThemePackManifest m;
    m.id = j.value("id", std::string{});
    m.name = j.value("name", std::string{});
    m.version = j.value("version", std::string{});
    m.tags_json_relative_path = j.value("tags_json", std::string{"tags.json"});
    if (m.id.empty()) return std::nullopt;

    for (const auto& f : j["files"]) {
        if (!f.is_object()) continue;
        PackFileEntry pf;
        pf.relative_path = f.value("path", std::string{});
        pf.sha256_hex = f.value("sha256", std::string{});
        pf.download_url = f.value("url", std::string{});
        pf.size_bytes = f.value<std::uint64_t>("size_bytes", 0);
        if (pf.relative_path.empty() || pf.download_url.empty() || pf.sha256_hex.empty()) continue;
        if (pf.download_url.rfind("https://", 0) != 0) continue; // Section 5
        m.files.push_back(std::move(pf));
    }
    if (m.files.empty()) return std::nullopt;
    return m;
}

// ===========================================================================
// ThemePackManager
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

InstallResult ThemePackManager::install(const CatalogEntry& entry, weather_fetch::IHttpClient& client,
                                          tag_system::TagIndex& index) {
    const std::string pack_dir = install_root_ + "/" + entry.id;
    if (fs::exists(pack_dir)) return InstallResult::AlreadyInstalled;

    // 1. Fetch and verify the manifest itself.
    auto manifest_resp = client.get(entry.manifest_url);
    if (manifest_resp.network_error || manifest_resp.status_code != 200) {
        return InstallResult::NetworkError;
    }
    if (!entry.manifest_sha256.empty()) {
        const std::string actual = sha256_hex_of_bytes(manifest_resp.body);
        std::string expected = entry.manifest_sha256;
        std::transform(expected.begin(), expected.end(), expected.begin(), ::tolower);
        std::string actual_lower = actual;
        std::transform(actual_lower.begin(), actual_lower.end(), actual_lower.begin(), ::tolower);
        if (actual_lower != expected) return InstallResult::ChecksumMismatch;
    }

    auto manifest = parse_pack_manifest(manifest_resp.body);
    if (!manifest.has_value()) return InstallResult::IoError;

    // 2. Fetch, verify, and write every file - abort and clean up on any
    // single failure (Section 2.5: never leave a partially-installed pack).
    std::error_code ec;
    fs::create_directories(pack_dir, ec);

    for (const auto& file_entry : manifest->files) {
        if (!is_safe_relative_path(file_entry.relative_path)) {
            fs::remove_all(pack_dir, ec);
            return InstallResult::UnsafePath;
        }
        auto resp = client.get(file_entry.download_url);
        if (resp.network_error || resp.status_code != 200) {
            fs::remove_all(pack_dir, ec);
            return InstallResult::NetworkError;
        }
        const std::string actual = sha256_hex_of_bytes(resp.body);
        std::string expected = file_entry.sha256_hex;
        std::transform(expected.begin(), expected.end(), expected.begin(), ::tolower);
        std::string actual_lower = actual;
        std::transform(actual_lower.begin(), actual_lower.end(), actual_lower.begin(), ::tolower);
        if (actual_lower != expected) {
            fs::remove_all(pack_dir, ec);
            return InstallResult::ChecksumMismatch;
        }

        const std::string dest_path = pack_dir + "/" + file_entry.relative_path;
        if (!write_bytes(dest_path, resp.body)) {
            fs::remove_all(pack_dir, ec);
            return InstallResult::IoError;
        }
    }

    // 3. Merge the pack's tag index (tags.json, in the same JSON shape as
    // tag_system::TagIndex::save_to_file) into the caller's live index,
    // stamping every asset with source_pack_id so uninstall() can find them
    // again and file_path rewritten to the on-disk absolute location.
    const std::string tags_path = pack_dir + "/" + manifest->tags_json_relative_path;
    bool tags_ok = false;
    tag_system::TagIndex pack_index = tag_system::TagIndex::load_from_file(tags_path, &tags_ok);
    for (auto rec : pack_index.list_all()) {
        rec.source_pack_id = entry.id;
        rec.file_path = pack_dir + "/" + rec.file_path; // resolve pack-relative -> absolute
        rec.id = entry.id + ":" + rec.id; // namespace ids by pack to avoid cross-pack collisions
        index.upsert(std::move(rec));
    }

    return InstallResult::Success;
}

bool ThemePackManager::uninstall(const std::string& pack_id, tag_system::TagIndex& index) {
    const std::string pack_dir = install_root_ + "/" + pack_id;
    std::error_code ec;
    if (!fs::exists(pack_dir, ec)) return false;

    // Remove every tag_system entry this pack owns before deleting files,
    // so a crash mid-uninstall never leaves dangling AssetRecords pointing
    // at files that no longer exist.
    std::vector<std::string> ids_to_remove;
    for (const auto& rec : index.list_all()) {
        if (rec.source_pack_id == pack_id) ids_to_remove.push_back(rec.id);
    }
    for (const auto& id : ids_to_remove) index.remove(id);

    fs::remove_all(pack_dir, ec);
    return !ec;
}

std::vector<std::string> ThemePackManager::list_installed_pack_ids() const {
    std::vector<std::string> ids;
    std::error_code ec;
    if (!fs::exists(install_root_, ec)) return ids;
    for (const auto& entry : fs::directory_iterator(install_root_, ec)) {
        if (entry.is_directory()) ids.push_back(entry.path().filename().string());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace weatherpaper::asset_manager
