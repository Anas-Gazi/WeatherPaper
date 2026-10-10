#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#if defined(WEATHERPAPER_WITH_OPENSSL)
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

#include "weatherpaper/asset_manager/asset_manager.hpp"

using namespace weatherpaper::asset_manager;
namespace fs = std::filesystem;

namespace {
class MapHttpClient : public weatherpaper::weather_fetch::IHttpClient {
public:
    std::unordered_map<std::string, weatherpaper::weather_fetch::HttpResponse> responses;
    weatherpaper::weather_fetch::HttpResponse get(const std::string& url) override {
        auto it = responses.find(url);
        if (it == responses.end()) return {true, 0, ""};
        return it->second;
    }
};
} // namespace

// --- Checksums --------------------------------------------------------

TEST_CASE("sha256_hex_of_bytes is deterministic and matches a known test vector") {
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    auto hash = sha256_hex_of_bytes("");
    CHECK(hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // SHA-256("abc") is a widely published NIST test vector.
    auto hash_abc = sha256_hex_of_bytes("abc");
    CHECK(hash_abc == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("sha256_hex_of_file / verify_checksum round-trip on a real file") {
    const std::string path = (fs::temp_directory_path() / "wp_am_checksum_test.bin").string();
    { std::ofstream out(path, std::ios::binary); out << "hello weatherpaper"; }

    auto hash = sha256_hex_of_file(path);
    REQUIRE(hash.has_value());
    CHECK(verify_checksum(path, *hash));
    CHECK(verify_checksum(path, *hash)); // idempotent
    CHECK_FALSE(verify_checksum(path, "0000000000000000000000000000000000000000000000000000000000000"));

    std::remove(path.c_str());
}

TEST_CASE("sha256_hex_of_file on a missing file returns nullopt") {
    CHECK_FALSE(sha256_hex_of_file("/nonexistent/wp_am_missing.bin").has_value());
}

// --- Path safety (Section 5 zip-slip defense) -----------------------------

TEST_CASE("is_safe_relative_path accepts ordinary relative paths") {
    CHECK(is_safe_relative_path("images/rain_night.jpg"));
    CHECK(is_safe_relative_path("tags.json"));
    CHECK(is_safe_relative_path("my..file.jpg")); // ".." substring but not a path component
}

TEST_CASE("is_safe_relative_path rejects traversal, absolute paths, and drive letters") {
    CHECK_FALSE(is_safe_relative_path("../../etc/passwd"));
    CHECK_FALSE(is_safe_relative_path("images/../../../etc/passwd"));
    CHECK_FALSE(is_safe_relative_path("/etc/passwd"));
    CHECK_FALSE(is_safe_relative_path("C:\\Windows\\System32\\evil.dll"));
    CHECK_FALSE(is_safe_relative_path(""));
}

// --- Upload validation (Section 3.7 / 3.4a) -------------------------------

TEST_CASE("classify_upload_extension accepts exactly PNG/JPG/JPEG/WebP and MP4/WebM") {
    CHECK(classify_upload_extension("photo.png") == UploadFileType::Image);
    CHECK(classify_upload_extension("photo.JPG") == UploadFileType::Image); // case-insensitive
    CHECK(classify_upload_extension("photo.jpeg") == UploadFileType::Image);
    CHECK(classify_upload_extension("photo.webp") == UploadFileType::Image);
    CHECK(classify_upload_extension("clip.mp4") == UploadFileType::Video);
    CHECK(classify_upload_extension("clip.webm") == UploadFileType::Video);
}

TEST_CASE("classify_upload_extension rejects executables and unknown types outright") {
    CHECK(classify_upload_extension("malware.exe") == UploadFileType::Rejected);
    CHECK(classify_upload_extension("script.sh") == UploadFileType::Rejected);
    CHECK(classify_upload_extension("archive.zip") == UploadFileType::Rejected);
    CHECK(classify_upload_extension("noextension") == UploadFileType::Rejected);
}

TEST_CASE("validate_upload accepts a normal image with no warnings") {
    auto v = validate_upload("photo.jpg", 2 * 1024 * 1024, 1920, 1080);
    CHECK(v.accepted);
    CHECK(v.type == UploadFileType::Image);
    CHECK_FALSE(v.exceeds_warning_threshold);
}

TEST_CASE("validate_upload warns but still accepts an oversized video") {
    auto v = validate_upload("huge.mp4", 200ull * 1024 * 1024);
    CHECK(v.accepted); // "warn in the UI ... but let the user proceed" - Section 3.4a
    CHECK(v.exceeds_warning_threshold);
}

TEST_CASE("validate_upload rejects a disguised executable with a clear reason") {
    auto v = validate_upload("totally_a_wallpaper.exe", 1000);
    CHECK_FALSE(v.accepted);
    CHECK_FALSE(v.rejection_reason.empty());
}

// --- Catalog / manifest parsing -------------------------------------------

const char* kCatalogJson = R"JSON({
    "packs": [
        {"id": "sakura-pack", "name": "Sakura Seasons", "version": "1.2.0",
         "manifest_url": "https://cdn.example.com/packs/sakura/manifest.json",
         "manifest_sha256": "abc123", "total_size_bytes": 5242880},
        {"id": "insecure-pack", "name": "Bad", "version": "1.0.0",
         "manifest_url": "http://not-https.example.com/manifest.json",
         "manifest_sha256": "def456", "total_size_bytes": 100}
    ]
})JSON";

TEST_CASE("parse_catalog extracts valid HTTPS entries and drops non-HTTPS ones") {
    auto catalog = parse_catalog(kCatalogJson);
    REQUIRE(catalog.has_value());
    REQUIRE(catalog->packs.size() == 1); // insecure-pack silently dropped (Section 5)
    CHECK(catalog->packs[0].id == "sakura-pack");
    CHECK(catalog->packs[0].manifest_url.rfind("https://", 0) == 0);
}

TEST_CASE("parse_catalog rejects malformed JSON") {
    CHECK_FALSE(parse_catalog("not json").has_value());
    CHECK_FALSE(parse_catalog("{}").has_value());
}

const char* kManifestJson = R"JSON({
    "id": "sakura-pack", "name": "Sakura Seasons", "version": "1.2.0",
    "tags_json": "tags.json",
    "files": [
        {"path": "tags.json", "sha256": "TAGSHASH", "url": "https://cdn.example.com/sakura/tags.json", "size_bytes": 200},
        {"path": "images/sunny_day.jpg", "sha256": "IMGHASH", "url": "https://cdn.example.com/sakura/sunny_day.jpg", "size_bytes": 90000}
    ]
})JSON";

TEST_CASE("parse_pack_manifest extracts all file entries") {
    auto manifest = parse_pack_manifest(kManifestJson);
    REQUIRE(manifest.has_value());
    CHECK(manifest->id == "sakura-pack");
    REQUIRE(manifest->files.size() == 2);
    CHECK(manifest->files[0].relative_path == "tags.json");
    CHECK(manifest->tags_json_relative_path == "tags.json");
}

// --- ThemePackManager: full install/uninstall pipeline via MockHttpClient -

TEST_CASE("ThemePackManager::install downloads, verifies, writes files, and merges tags") {
    const std::string root = (fs::temp_directory_path() / "wp_am_install_test").string();
    fs::remove_all(root);
    fs::create_directories(root);

    const std::string manifest_body = kManifestJson;
    const std::string tags_body = R"({"format_version":1,"assets":[
        {"id":"sunny1","file":"images/sunny_day.jpg","type":"image","fit_mode":"fill","tags":["sunny","day"]}
    ]})";
    const std::string image_body = "FAKEJPEGBYTES";

    MapHttpClient client;

    // Patch the manifest's per-file checksums to match our fixture bytes
    // exactly (the literal manifest JSON above has placeholder hashes).
    std::string patched_manifest = manifest_body;
    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replace_all(patched_manifest, "TAGSHASH", sha256_hex_of_bytes(tags_body));
    replace_all(patched_manifest, "IMGHASH", sha256_hex_of_bytes(image_body));

    client.responses["https://cdn.example.com/packs/sakura/manifest.json"] = {false, 200, patched_manifest};
    client.responses["https://cdn.example.com/sakura/tags.json"] = {false, 200, tags_body};
    client.responses["https://cdn.example.com/sakura/sunny_day.jpg"] = {false, 200, image_body};

    CatalogEntry entry;
    entry.id = "sakura-pack";
    entry.manifest_url = "https://cdn.example.com/packs/sakura/manifest.json";
    entry.manifest_sha256 = sha256_hex_of_bytes(patched_manifest);

    weatherpaper::tag_system::TagIndex index;
    ThemePackManager mgr(root);
    auto result = mgr.install(entry, client, index);
    CHECK(result == InstallResult::Success);

    // Files were written to disk.
    CHECK(fs::exists(root + "/sakura-pack/images/sunny_day.jpg"));
    CHECK(fs::exists(root + "/sakura-pack/tags.json"));

    // Tag index was merged with a namespaced id and absolute, resolved path.
    auto all = index.list_all();
    REQUIRE(all.size() == 1);
    CHECK(all[0].id == "sakura-pack:sunny1");
    CHECK(all[0].source_pack_id == "sakura-pack");
    CHECK(all[0].file_path == root + "/sakura-pack/images/sunny_day.jpg");
    CHECK(all[0].has_tag("sunny"));

    auto installed = mgr.list_installed_pack_ids();
    REQUIRE(installed.size() == 1);
    CHECK(installed[0] == "sakura-pack");

    // Uninstall removes both the files and the tag entries.
    CHECK(mgr.uninstall("sakura-pack", index));
    CHECK_FALSE(fs::exists(root + "/sakura-pack"));
    CHECK(index.list_all().empty());

    fs::remove_all(root);
}

TEST_CASE("ThemePackManager::install fails cleanly on checksum mismatch, no partial install left behind") {
    const std::string root = (fs::temp_directory_path() / "wp_am_install_fail_test").string();
    fs::remove_all(root);
    fs::create_directories(root);

    MapHttpClient client;
    client.responses["https://cdn.example.com/bad/manifest.json"] = {false, 200, kManifestJson};

    CatalogEntry entry;
    entry.id = "bad-pack";
    entry.manifest_url = "https://cdn.example.com/bad/manifest.json";
    entry.manifest_sha256 = "0000000000000000000000000000000000000000000000000000000000000"; // wrong on purpose

    weatherpaper::tag_system::TagIndex index;
    ThemePackManager mgr(root);
    auto result = mgr.install(entry, client, index);
    CHECK(result == InstallResult::ChecksumMismatch);
    CHECK_FALSE(fs::exists(root + "/bad-pack")); // cleaned up, no partial install
    CHECK(index.list_all().empty());

    fs::remove_all(root);
}

TEST_CASE("ThemePackManager::install fails cleanly on a network error") {
    const std::string root = (fs::temp_directory_path() / "wp_am_install_neterr_test").string();
    fs::remove_all(root);

    MapHttpClient client; // no responses registered -> network_error for any URL
    CatalogEntry entry;
    entry.id = "offline-pack";
    entry.manifest_url = "https://cdn.example.com/offline/manifest.json";

    weatherpaper::tag_system::TagIndex index;
    ThemePackManager mgr(root);
    auto result = mgr.install(entry, client, index);
    CHECK(result == InstallResult::NetworkError);
}

#if defined(WEATHERPAPER_WITH_OPENSSL)

TEST_CASE("verify_rsa_sha256_signature: real end-to-end sign/verify round trip") {
    const std::string payload = "official-catalog-payload-v1";

    EVP_PKEY_CTX* keygen_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    REQUIRE(keygen_ctx != nullptr);
    REQUIRE(EVP_PKEY_keygen_init(keygen_ctx) > 0);
    REQUIRE(EVP_PKEY_CTX_set_rsa_keygen_bits(keygen_ctx, 2048) > 0);

    EVP_PKEY* key = nullptr;
    const int keygen_result = EVP_PKEY_keygen(keygen_ctx, &key);
    EVP_PKEY_CTX_free(keygen_ctx);

    REQUIRE(keygen_result > 0);
    REQUIRE(key != nullptr);

    BIO* public_key_bio = BIO_new(BIO_s_mem());
    REQUIRE(public_key_bio != nullptr);
    REQUIRE(PEM_write_bio_PUBKEY(public_key_bio, key) > 0);

    char* public_key_data = nullptr;
    const long public_key_length = BIO_get_mem_data(public_key_bio, &public_key_data);
    REQUIRE(public_key_length > 0);
    REQUIRE(public_key_data != nullptr);

    const std::string public_key_pem(
        public_key_data,
        static_cast<std::size_t>(public_key_length)
    );
    BIO_free(public_key_bio);

    EVP_MD_CTX* sign_ctx = EVP_MD_CTX_new();
    REQUIRE(sign_ctx != nullptr);
    REQUIRE(EVP_DigestSignInit(sign_ctx, nullptr, EVP_sha256(), nullptr, key) > 0);
    REQUIRE(EVP_DigestSignUpdate(sign_ctx, payload.data(), payload.size()) > 0);

    std::size_t signature_length = 0;
    REQUIRE(EVP_DigestSignFinal(sign_ctx, nullptr, &signature_length) > 0);
    REQUIRE(signature_length > 0);

    std::string signature(signature_length, '\0');
    REQUIRE(EVP_DigestSignFinal(
        sign_ctx,
        reinterpret_cast<unsigned char*>(signature.data()),
        &signature_length
    ) > 0);

    signature.resize(signature_length);

    EVP_MD_CTX_free(sign_ctx);
    EVP_PKEY_free(key);

    CHECK(verify_rsa_sha256_signature(payload, signature, public_key_pem));
    CHECK_FALSE(verify_rsa_sha256_signature(
        "TAMPERED-payload", signature, public_key_pem
    ));
}
#endif
