#include "weatherpaper/weather_fetch/weather_fetch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#if defined(WEATHERPAPER_WITH_CURL)
  #include <curl/curl.h>
#endif

namespace weatherpaper::weather_fetch {

using json = nlohmann::json;
using wallpaper_engine_core::Condition;

// ===========================================================================
// CurlHttpClient (production HTTP transport)
// ===========================================================================
#if defined(WEATHERPAPER_WITH_CURL)
namespace {
size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
} // namespace

HttpResponse CurlHttpClient::get(const std::string& url) {
    HttpResponse response;

    // SECURITY (Section 5): reject non-HTTPS endpoints outright, no
    // exceptions, regardless of caller intent.
    if (url.rfind("https://", 0) != 0) {
        response.network_error = true;
        return response;
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        response.network_error = true;
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);       // never hang the poll loop forever
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WeatherPaper/1.0 (+https://github.com/weatherpaper)");
    // SECURITY: never disable TLS verification, even for convenience.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        response.network_error = true;
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.status_code = static_cast<int>(status);
    }
    curl_easy_cleanup(curl);
    return response;
}
#endif // WEATHERPAPER_WITH_CURL

// ===========================================================================
// Shared parsing helpers
// ===========================================================================
namespace {

// Parses "YYYY-MM-DDTHH:MM" (Open-Meteo's local-time ISO8601-without-offset
// format) as a *naive* wall-clock instant (i.e. treats the numeric fields as
// if they were UTC), then subtracts utc_offset_seconds to produce the true
// UTC instant. This avoids needing a full IANA timezone database while still
// being numerically correct, because Open-Meteo (with timezone=auto) always
// supplies a matching top-level "utc_offset_seconds" for the same request.
//
// KNOWN LIMITATION (documented in docs/ARCHITECTURE.md): this assumes the
// offset is constant across the whole cached window, which is true for any
// single poll but would need a real tz database to correctly predict DST
// transitions far in the future. Not a concern at a 15-30 minute poll
// interval (Section 3.1).
std::optional<std::chrono::system_clock::time_point> parse_local_iso8601(
    const std::string& s, int utc_offset_seconds) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) != 5) {
        return std::nullopt;
    }
    using namespace std::chrono;
    const auto days_part = sys_days{year{y} / month{static_cast<unsigned>(mo)} /
                                     day{static_cast<unsigned>(d)}};
    auto naive_utc = time_point_cast<system_clock::duration>(days_part) +
                      hours(h) + minutes(mi);
    return naive_utc - seconds(utc_offset_seconds);
}

Condition wmo_weathercode_to_condition(int code) {
    // WMO Weather interpretation codes, as used by Open-Meteo.
    if (code == 0) return Condition::Clear;
    if (code == 1) return Condition::Sunny;                 // mainly clear
    if (code == 2 || code == 3) return Condition::Cloudy;   // partly cloudy / overcast
    if (code == 45 || code == 48) return Condition::Fog;
    if (code >= 51 && code <= 67) return Condition::Rain;   // drizzle + rain
    if (code >= 71 && code <= 77) return Condition::Snow;   // snow fall/grains
    if (code == 80 || code == 81 || code == 82) return Condition::Rain; // showers
    if (code == 85 || code == 86) return Condition::Snow;   // snow showers
    if (code >= 95 && code <= 99) return Condition::Storm;  // thunderstorm
    return Condition::Cloudy; // ASSUMPTION: unmapped/future WMO codes
                               // default to Cloudy (visually neutral)
                               // rather than Clear, being conservative about
                               // not showing a falsely-sunny wallpaper.
}

Condition owm_id_to_condition(int id) {
    if (id >= 200 && id <= 232) return Condition::Storm;
    if (id >= 300 && id <= 321) return Condition::Rain;   // drizzle
    if (id >= 500 && id <= 531) return Condition::Rain;
    if (id >= 600 && id <= 622) return Condition::Snow;
    if (id >= 701 && id <= 781) return Condition::Fog;    // mist/haze/dust/etc grouped as fog
    if (id == 800) return Condition::Clear;
    if (id == 801) return Condition::Sunny;                // few clouds
    if (id >= 802 && id <= 804) return Condition::Cloudy;
    return Condition::Cloudy; // ASSUMPTION: same conservative default as above
}

bool is_severe(Condition c, double temperature_c) {
    // Section 3.13: "severe weather (storms, extreme temperature)".
    // ASSUMPTION: -10C/40C chosen as generically "extreme" thresholds; a
    // future contributor may want these user-configurable per-climate
    // (e.g. -10C is unremarkable in northern climates) - tracked as a
    // config.md TODO rather than hardcoded further than this one constant
    // pair, which is why they're named constants here, not magic numbers.
    constexpr double kExtremeColdC = -10.0;
    constexpr double kExtremeHotC = 40.0;
    return c == Condition::Storm || temperature_c <= kExtremeColdC || temperature_c >= kExtremeHotC;
}

} // namespace

// ===========================================================================
// OpenMeteoProvider
// ===========================================================================
std::optional<WeatherSnapshot> OpenMeteoProvider::parse_response(const std::string& body) {
    json j;
    try {
        j = json::parse(body);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    const json* cw_ptr = nullptr;
    if (j.contains("current") && j["current"].is_object()) {
        cw_ptr = &j["current"];
    } else if (j.contains("current_weather") && j["current_weather"].is_object()) {
        cw_ptr = &j["current_weather"];
    } else {
        return std::nullopt;
    }
    if (!j.contains("daily")) return std::nullopt;

    const int utc_offset = j.value("utc_offset_seconds", 0);
    const auto& cw = *cw_ptr;
    const auto& daily = j["daily"];
    if (!daily.contains("sunrise") || !daily.contains("sunset") ||
        !daily["sunrise"].is_array() || daily["sunrise"].empty() ||
        !daily["sunset"].is_array() || daily["sunset"].empty()) {
        return std::nullopt;
    }

    auto sunrise = parse_local_iso8601(daily["sunrise"][0].get<std::string>(), utc_offset);
    auto sunset = parse_local_iso8601(daily["sunset"][0].get<std::string>(), utc_offset);
    if (!sunrise || !sunset) return std::nullopt;

    WeatherSnapshot snap;
    snap.sunrise = *sunrise;
    snap.sunset = *sunset;
    if (cw.contains("temperature_2m")) {
        snap.temperature_c = cw["temperature_2m"].get<double>();
    } else if (cw.contains("temperature")) {
        snap.temperature_c = cw["temperature"].get<double>();
    } else {
        snap.temperature_c = 0.0;
    }

    int code = 3;
    if (cw.contains("weather_code")) {
        code = cw["weather_code"].get<int>();
    } else if (cw.contains("weathercode")) {
        code = cw["weathercode"].get<int>();
    }
    snap.condition = wmo_weathercode_to_condition(code);
    snap.fetched_at = std::chrono::system_clock::now();
    snap.provider_name = "open-meteo";
    snap.is_severe = is_severe(snap.condition, snap.temperature_c);
    return snap;
}

std::optional<WeatherSnapshot> OpenMeteoProvider::fetch(
    IHttpClient& client, double latitude, double longitude) const {
    std::ostringstream url;
    url << "https://api.open-meteo.com/v1/forecast?latitude=" << latitude
        << "&longitude=" << longitude
        << "&current=weather_code,temperature_2m,is_day&daily=sunrise,sunset&timezone=auto";
    auto resp = client.get(url.str());
    if (resp.network_error || resp.status_code != 200) return std::nullopt;
    return parse_response(resp.body);
}

// ===========================================================================
// OpenWeatherMapProvider
// ===========================================================================
std::optional<WeatherSnapshot> OpenWeatherMapProvider::parse_response(const std::string& body) {
    json j;
    try {
        j = json::parse(body);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (!j.contains("weather") || !j["weather"].is_array() || j["weather"].empty() ||
        !j.contains("sys") || !j.contains("main")) {
        return std::nullopt;
    }

    WeatherSnapshot snap;
    // OpenWeatherMap's sys.sunrise/sunset are true Unix epoch seconds (UTC),
    // no timezone ambiguity - simpler and more precise than the Open-Meteo
    // local-string parsing above.
    snap.sunrise = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(j["sys"].value("sunrise", 0)));
    snap.sunset = std::chrono::system_clock::from_time_t(
        static_cast<std::time_t>(j["sys"].value("sunset", 0)));
    snap.temperature_c = j["main"].value("temp", 0.0);
    snap.condition = owm_id_to_condition(j["weather"][0].value("id", 800));
    snap.fetched_at = std::chrono::system_clock::now();
    snap.provider_name = "openweathermap";
    snap.is_severe = is_severe(snap.condition, snap.temperature_c);
    return snap;
}

std::optional<WeatherSnapshot> OpenWeatherMapProvider::fetch(
    IHttpClient& client, double latitude, double longitude) const {
    std::ostringstream url;
    url << "https://api.openweathermap.org/data/2.5/weather?lat=" << latitude
        << "&lon=" << longitude << "&units=metric&appid=" << api_key_;
    auto resp = client.get(url.str());
    if (resp.network_error || resp.status_code != 200) return std::nullopt;
    return parse_response(resp.body);
}

// ===========================================================================
// Disk cache
// ===========================================================================
bool write_cache(const std::string& path, const WeatherSnapshot& snap) {
    json j;
    j["condition"] = std::string(wallpaper_engine_core::to_string(snap.condition));
    j["temperature_c"] = snap.temperature_c;
    j["sunrise_unix"] = std::chrono::duration_cast<std::chrono::seconds>(
        snap.sunrise.time_since_epoch()).count();
    j["sunset_unix"] = std::chrono::duration_cast<std::chrono::seconds>(
        snap.sunset.time_since_epoch()).count();
    j["fetched_at_unix"] = std::chrono::duration_cast<std::chrono::seconds>(
        snap.fetched_at.time_since_epoch()).count();
    j["is_severe"] = snap.is_severe;
    j["provider_name"] = snap.provider_name;

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << j.dump(2);
        if (!out.good()) return false;
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

std::optional<WeatherSnapshot> read_cache(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return std::nullopt;
    json j;
    try {
        in >> j;
    } catch (const std::exception&) {
        return std::nullopt; // corrupt cache - treat as "no cache"
    }
    if (!j.contains("condition") || !j.contains("sunrise_unix") || !j.contains("sunset_unix")) {
        return std::nullopt;
    }
    WeatherSnapshot snap;
    snap.condition = wallpaper_engine_core::condition_from_string(
        j.value("condition", std::string("clear")));
    snap.temperature_c = j.value("temperature_c", 0.0);
    snap.sunrise = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value<long long>("sunrise_unix", 0)));
    snap.sunset = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value<long long>("sunset_unix", 0)));
    snap.fetched_at = std::chrono::system_clock::time_point(
        std::chrono::seconds(j.value<long long>("fetched_at_unix", 0)));
    snap.is_severe = j.value("is_severe", false);
    snap.provider_name = j.value("provider_name", std::string{});
    return snap;
}

// ===========================================================================
// Seasonal fallback
// ===========================================================================
Season season_for_date(std::chrono::year_month_day date) noexcept {
    // ASSUMPTION: Northern Hemisphere meteorological seasons (Dec-Feb =
    // Winter, Mar-May = Spring, Jun-Aug = Summer, Sep-Nov = Autumn). This is
    // a documented simplification (see header comment) - hemisphere-correct
    // seasons would require the user's latitude sign, which this pure/
    // date-only function deliberately doesn't take, to keep it trivially
    // unit-testable. The app-orchestration layer may flip Spring<->Autumn
    // and Summer<->Winter when Location.latitude < 0 before using the tag.
    const unsigned m = static_cast<unsigned>(date.month());
    if (m == 12 || m == 1 || m == 2) return Season::Winter;
    if (m >= 3 && m <= 5) return Season::Spring;
    if (m >= 6 && m <= 8) return Season::Summer;
    return Season::Autumn; // 9,10,11
}

const char* to_string(Season s) noexcept {
    switch (s) {
        case Season::Spring: return "spring";
        case Season::Summer: return "summer";
        case Season::Autumn: return "autumn";
        case Season::Winter: return "winter";
    }
    return "winter";
}

// ===========================================================================
// Backoff policy
// ===========================================================================
std::chrono::minutes next_backoff_interval(std::chrono::minutes normal_interval,
                                             int consecutive_failures) noexcept {
    if (consecutive_failures <= 0) return normal_interval;
    // Exponential backoff, doubling per failure, capped at 4 hours so the
    // app always retries within a bounded window even after a long outage,
    // and re-tries immediately on an OS network-reconnect event regardless
    // (that trigger lives in the platform layer, Section 3.1).
    constexpr std::chrono::minutes kCap{240};
    long long multiplier = 1LL << std::min(consecutive_failures, 6); // avoid overflow; caps well before 6
    std::chrono::minutes backoff{normal_interval.count() * multiplier};
    return std::min(backoff, kCap);
}

} // namespace weatherpaper::weather_fetch
