#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "weatherpaper/updater/updater.hpp"
#include "weatherpaper/asset_manager/asset_manager.hpp"

using namespace weatherpaper::updater;
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

// --- Semver ----------------------------------------------------------

TEST_CASE("parse_semver handles well-formed and partial version strings") {
    auto v = parse_semver("1.2.3");
    REQUIRE(v.has_value());
    CHECK(v->major == 1);
    CHECK(v->minor == 2);
    CHECK(v->patch == 3);

    auto partial = parse_semver("2");
    REQUIRE(partial.has_value());
    CHECK(partial->major == 2);
    CHECK(partial->minor == 0);
}

TEST_CASE("parse_semver returns nullopt for garbage input") {
    CHECK_FALSE(parse_semver("not-a-version").has_value());
}

TEST_CASE("compare_semver orders major, then minor, then patch") {
    CHECK(compare_semver({1, 0, 0}, {2, 0, 0}) < 0);
    CHECK(compare_semver({2, 5, 0}, {2, 4, 9}) > 0);
    CHECK(compare_semver({1, 2, 3}, {1, 2, 3}) == 0);
    CHECK(compare_semver({1, 2, 3}, {1, 2, 4}) < 0);
}

TEST_CASE("is_newer_version compares current vs remote correctly") {
    CHECK(is_newer_version("1.0.0", "1.0.1"));
    CHECK_FALSE(is_newer_version("1.0.1", "1.0.0"));
    CHECK_FALSE(is_newer_version("1.0.0", "1.0.0"));
    // Malformed current version is treated as 0.0.0 - any valid remote
    // version looks newer, which is the safe default (prefer to offer an
    // update rather than silently never updating due to a corrupt local
    // version string).
    CHECK(is_newer_version("garbage", "1.0.0"));
}

// --- Manifest parsing ---------------------------------------------------

const char* kManifestJson = R"JSON({
    "components": [
        {"component_id": "core", "version": "1.1.0",
         "download_url": "https://cdn.example.com/updates/core-1.1.0.bin",
         "sha256": "CORESHA", "signature_base64": "", "size_bytes": 5000000},
        {"component_id": "ui", "version": "1.0.5",
         "download_url": "https://cdn.example.com/updates/ui-1.0.5.bin",
         "sha256": "UISHA", "signature_base64": "", "size_bytes": 2000000},
        {"component_id": "asset_catalog", "version": "2.0.1",
         "download_url": "http://not-https.example.com/catalog.bin",
         "sha256": "CATSHA", "size_bytes": 100}
    ]
})JSON";

TEST_CASE("parse_version_manifest extracts HTTPS components and drops non-HTTPS ones") {
    auto manifest = parse_version_manifest(kManifestJson);
    REQUIRE(manifest.has_value());
    CHECK(manifest->latest_by_component.count("core") == 1);
    CHECK(manifest->latest_by_component.count("ui") == 1);
    CHECK(manifest->latest_by_component.count("asset_catalog") == 0); // non-HTTPS, dropped (Section 5)
}

TEST_CASE("parse_version_manifest rejects malformed JSON") {
    CHECK_FALSE(parse_version_manifest("not json").has_value());
    CHECK_FALSE(parse_version_manifest("{}").has_value());
}

// --- check_for_updates ---------------------------------------------------

TEST_CASE("check_for_updates flags only components with a newer remote version") {
    auto manifest = parse_version_manifest(kManifestJson);
    REQUIRE(manifest.has_value());

    ComponentVersions current;
    current.core = "1.0.0";           // remote 1.1.0 -> update available
    current.ui = "1.0.5";             // remote 1.0.5 -> up to date
    current.asset_catalog = "1.0.0";  // not in manifest (dropped) -> no update reported

    auto result = check_for_updates(*manifest, current);
    REQUIRE(result.components_with_updates.size() == 1);
    CHECK(result.components_with_updates[0] == "core");
}

// --- base64 ---------------------------------------------------------------

TEST_CASE("base64_decode round-trips known test vectors") {
    // "hello" -> "aGVsbG8="
    auto decoded = base64_decode("aGVsbG8=");
    REQUIRE(decoded.has_value());
    CHECK(*decoded == "hello");
}

TEST_CASE("base64_decode rejects invalid characters rather than guessing") {
    CHECK_FALSE(base64_decode("not@@valid!!base64").has_value());
}

// --- UpdateManager: staged apply + rollback -------------------------------

TEST_CASE("UpdateManager::apply_component_update verifies checksum and activates the package") {
    const std::string staging = (fs::temp_directory_path() / "wp_upd_staging").string();
    const std::string install = (fs::temp_directory_path() / "wp_upd_install").string();
    fs::remove_all(staging);
    fs::remove_all(install);

    const std::string package_bytes = "FAKE-CORE-BINARY-CONTENT-v1.1.0";
    MapHttpClient client;
    client.responses["https://cdn.example.com/updates/core-1.1.0.bin"] = {false, 200, package_bytes};

    weatherpaper::updater::PackageInfo pkg;
    pkg.component_id = "core";
    pkg.version = "1.1.0";
    pkg.download_url = "https://cdn.example.com/updates/core-1.1.0.bin";
    pkg.sha256_hex = weatherpaper::asset_manager::sha256_hex_of_bytes(package_bytes);

    UpdateManager mgr(staging, install, /*public_key_pem=*/"");
    auto result = mgr.apply_component_update(pkg, client);
    CHECK(result == ApplyResult::Success);

    auto active_path = mgr.current_package_path("core");
    REQUIRE_FALSE(active_path.empty());
    std::ifstream in(active_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(content == package_bytes);

    fs::remove_all(staging);
    fs::remove_all(install);
}

TEST_CASE("UpdateManager::apply_component_update rejects a checksum mismatch, leaves nothing active") {
    const std::string staging = (fs::temp_directory_path() / "wp_upd_staging2").string();
    const std::string install = (fs::temp_directory_path() / "wp_upd_install2").string();
    fs::remove_all(staging);
    fs::remove_all(install);

    MapHttpClient client;
    client.responses["https://cdn.example.com/updates/core-bad.bin"] = {false, 200, "some bytes"};

    weatherpaper::updater::PackageInfo pkg;
    pkg.component_id = "core";
    pkg.version = "9.9.9";
    pkg.download_url = "https://cdn.example.com/updates/core-bad.bin";
    pkg.sha256_hex = "0000000000000000000000000000000000000000000000000000000000000"; // wrong

    UpdateManager mgr(staging, install, "");
    auto result = mgr.apply_component_update(pkg, client);
    CHECK(result == ApplyResult::ChecksumMismatch);
    CHECK(mgr.current_package_path("core").empty());

    fs::remove_all(staging);
    fs::remove_all(install);
}

TEST_CASE("UpdateManager: applying a second update backs up the first, and rollback restores it") {
    const std::string staging = (fs::temp_directory_path() / "wp_upd_staging3").string();
    const std::string install = (fs::temp_directory_path() / "wp_upd_install3").string();
    fs::remove_all(staging);
    fs::remove_all(install);

    MapHttpClient client;
    const std::string v1_bytes = "CORE-V1";
    const std::string v2_bytes = "CORE-V2";
    client.responses["https://cdn.example.com/core-v1.bin"] = {false, 200, v1_bytes};
    client.responses["https://cdn.example.com/core-v2.bin"] = {false, 200, v2_bytes};

    weatherpaper::updater::PackageInfo pkg1;
    pkg1.component_id = "core";
    pkg1.version = "1.0.0";
    pkg1.download_url = "https://cdn.example.com/core-v1.bin";
    pkg1.sha256_hex = weatherpaper::asset_manager::sha256_hex_of_bytes(v1_bytes);

    weatherpaper::updater::PackageInfo pkg2 = pkg1;
    pkg2.version = "2.0.0";
    pkg2.download_url = "https://cdn.example.com/core-v2.bin";
    pkg2.sha256_hex = weatherpaper::asset_manager::sha256_hex_of_bytes(v2_bytes);

    UpdateManager mgr(staging, install, "");
    REQUIRE(mgr.apply_component_update(pkg1, client) == ApplyResult::Success);
    REQUIRE(mgr.apply_component_update(pkg2, client) == ApplyResult::Success);

    // v2 is now active.
    {
        std::ifstream in(mgr.current_package_path("core"), std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(content == v2_bytes);
    }

    // Simulate v2 turning out to be broken - roll back.
    REQUIRE(mgr.rollback_component("core"));
    {
        std::ifstream in(mgr.current_package_path("core"), std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(content == v1_bytes); // back to v1
    }

    fs::remove_all(staging);
    fs::remove_all(install);
}

TEST_CASE("UpdateManager::rollback_component returns false when there is nothing to roll back to") {
    const std::string staging = (fs::temp_directory_path() / "wp_upd_staging4").string();
    const std::string install = (fs::temp_directory_path() / "wp_upd_install4").string();
    fs::remove_all(staging);
    fs::remove_all(install);
    UpdateManager mgr(staging, install, "");
    CHECK_FALSE(mgr.rollback_component("core")); // never installed, no backup
}
