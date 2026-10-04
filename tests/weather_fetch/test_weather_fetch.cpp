#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "weatherpaper/weather_fetch/weather_fetch.hpp"

using namespace weatherpaper::weather_fetch;
using weatherpaper::wallpaper_engine_core::Condition;

namespace {

// Mock transport - no real network access, per module design (see header).
class MockHttpClient : public IHttpClient {
public:
    HttpResponse next_response;
    std::string last_url;

    HttpResponse get(const std::string& url) override {
        last_url = url;
        return next_response;
    }
};

const char* kOpenMeteoFixture = R"JSON({
    "latitude": 23.81,
    "longitude": 90.41,
    "utc_offset_seconds": 21600,
    "timezone": "Asia/Dhaka",
    "current_weather": {
        "temperature": 31.5,
        "windspeed": 10.2,
        "weathercode": 61,
        "is_day": 1,
        "time": "2026-09-03T14:00"
    },
    "daily": {
        "sunrise": ["2026-09-03T05:48"],
        "sunset": ["2026-09-03T18:12"]
    }
})JSON";

const char* kOpenWeatherMapFixture = R"JSON({
    "weather": [{"id": 200, "main": "Thunderstorm", "description": "thunderstorm with light rain"}],
    "main": {"temp": 26.0, "humidity": 88},
    "sys": {"sunrise": 1756871280, "sunset": 1756916520},
    "dt": 1756890000
})JSON";

} // namespace

// --- OpenMeteoProvider ---------------------------------------------------

TEST_CASE("OpenMeteoProvider::parse_response extracts condition, temp, sunrise/sunset") {
    auto snap = OpenMeteoProvider::parse_response(kOpenMeteoFixture);
    REQUIRE(snap.has_value());
    CHECK(snap->condition == Condition::Rain); // weathercode 61 = rain
    CHECK(snap->temperature_c == doctest::Approx(31.5));
    CHECK(snap->is_valid()); // sunrise < sunset
    CHECK(snap->provider_name == "open-meteo");
}

TEST_CASE("OpenMeteoProvider applies utc_offset_seconds so sunrise precedes sunset by a sane margin") {
    auto snap = OpenMeteoProvider::parse_response(kOpenMeteoFixture);
    REQUIRE(snap.has_value());
    auto daylight = snap->sunset - snap->sunrise;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(daylight).count();
    CHECK(hours > 0);
    CHECK(hours < 24);
    // 05:48 to 18:12 local is 12h24m of daylight.
    CHECK(hours == 12);
}

TEST_CASE("OpenMeteoProvider::parse_response rejects malformed/incomplete JSON") {
    CHECK_FALSE(OpenMeteoProvider::parse_response("not json").has_value());
    CHECK_FALSE(OpenMeteoProvider::parse_response(R"({"current_weather": {}})").has_value());
    CHECK_FALSE(OpenMeteoProvider::parse_response("{}").has_value());
}

TEST_CASE("OpenMeteoProvider::fetch rejects a non-200 response") {
    MockHttpClient client;
    client.next_response = {false, 500, "server error"};
    OpenMeteoProvider provider;
    auto snap = provider.fetch(client, 23.81, 90.41);
    CHECK_FALSE(snap.has_value());
    CHECK(client.last_url.rfind("https://api.open-meteo.com", 0) == 0); // HTTPS-only endpoint
}

TEST_CASE("OpenMeteoProvider::fetch rejects a network error and never throws") {
    MockHttpClient client;
    client.next_response = {true, 0, ""};
    OpenMeteoProvider provider;
    CHECK_NOTHROW(provider.fetch(client, 0.0, 0.0));
    CHECK_FALSE(provider.fetch(client, 0.0, 0.0).has_value());
}

TEST_CASE("OpenMeteoProvider::fetch succeeds through the full mocked HTTP path") {
    MockHttpClient client;
    client.next_response = {false, 200, kOpenMeteoFixture};
    OpenMeteoProvider provider;
    auto snap = provider.fetch(client, 23.81, 90.41);
    REQUIRE(snap.has_value());
    CHECK(snap->condition == Condition::Rain);
}

// --- OpenWeatherMapProvider (fallback) ------------------------------------

TEST_CASE("OpenWeatherMapProvider::parse_response maps condition id and uses exact unix timestamps") {
    auto snap = OpenWeatherMapProvider::parse_response(kOpenWeatherMapFixture);
    REQUIRE(snap.has_value());
    CHECK(snap->condition == Condition::Storm); // id 200 = thunderstorm
    CHECK(snap->temperature_c == doctest::Approx(26.0));
    CHECK(snap->is_valid());
    CHECK(snap->provider_name == "openweathermap");
    CHECK(snap->is_severe); // storm -> severe (Section 3.13)
}

TEST_CASE("OpenWeatherMapProvider::fetch includes the API key and stays HTTPS") {
    MockHttpClient client;
    client.next_response = {false, 200, kOpenWeatherMapFixture};
    OpenWeatherMapProvider provider("secret-key-123");
    auto snap = provider.fetch(client, 23.81, 90.41);
    REQUIRE(snap.has_value());
    CHECK(client.last_url.rfind("https://", 0) == 0);
    CHECK(client.last_url.find("secret-key-123") != std::string::npos);
}

// --- Disk cache ------------------------------------------------------------

TEST_CASE("write_cache / read_cache round-trips a snapshot") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_weather_cache_test.json").string();
    std::remove(path.c_str());

    auto snap = OpenMeteoProvider::parse_response(kOpenMeteoFixture);
    REQUIRE(snap.has_value());
    REQUIRE(write_cache(path, *snap));

    auto loaded = read_cache(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->condition == snap->condition);
    CHECK(loaded->temperature_c == doctest::Approx(snap->temperature_c));
    CHECK(loaded->sunrise == snap->sunrise);
    CHECK(loaded->sunset == snap->sunset);

    std::remove(path.c_str());
}

TEST_CASE("read_cache on a missing or corrupt file returns nullopt, never throws") {
    CHECK_FALSE(read_cache("/nonexistent/wp_cache_missing.json").has_value());

    const std::string path = (std::filesystem::temp_directory_path() / "wp_weather_cache_bad.json").string();
    { std::ofstream(path) << "{ broken"; }
    CHECK_NOTHROW(read_cache(path));
    CHECK_FALSE(read_cache(path).has_value());
    std::remove(path.c_str());
}

// --- Backoff policy ---------------------------------------------------

TEST_CASE("next_backoff_interval doubles per failure and is capped") {
    using namespace std::chrono;
    CHECK(next_backoff_interval(minutes(20), 0) == minutes(20));   // no failures yet
    CHECK(next_backoff_interval(minutes(20), 1) == minutes(40));
    CHECK(next_backoff_interval(minutes(20), 2) == minutes(80));
    CHECK(next_backoff_interval(minutes(20), 3) == minutes(160));
    CHECK(next_backoff_interval(minutes(20), 10) == minutes(240)); // capped at 4h
}

// --- Seasonal fallback (Section 3.1 offline first-launch case) ----------

TEST_CASE("season_for_date maps months to Northern Hemisphere meteorological seasons") {
    using namespace std::chrono;
    CHECK(season_for_date(year{2026}/January/15) == Season::Winter);
    CHECK(season_for_date(year{2026}/April/1) == Season::Spring);
    CHECK(season_for_date(year{2026}/July/4) == Season::Summer);
    CHECK(season_for_date(year{2026}/October/31) == Season::Autumn);
    CHECK(season_for_date(year{2026}/December/25) == Season::Winter);
}

TEST_CASE("Season to_string produces tag-friendly lowercase strings") {
    CHECK(std::string(to_string(Season::Spring)) == "spring");
    CHECK(std::string(to_string(Season::Winter)) == "winter");
}
