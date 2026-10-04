// weatherpaper/weather_fetch/weather_fetch.hpp
//
// Module: weather_fetch (Section 3.1)
// Layer:  network + local disk cache. Depends on wallpaper_engine_core
//         (for the Condition enum - see that header's top comment for why
//         the dependency points this direction) but never on any platform/
//         OS module.
//
// Design for testability without live network access (a core requirement
// per the spec's emphasis on unit-testable modules): all HTTP I/O goes
// through the tiny IHttpClient interface below. Production code links a
// libcurl-backed implementation (CurlHttpClient, guarded by
// WEATHERPAPER_WITH_CURL); tests inject a MockHttpClient that returns
// canned fixture JSON, so provider parsing and the polling/backoff/cache
// state machine are fully covered with zero real network calls.
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"

namespace weatherpaper::weather_fetch {

// --- HTTP abstraction ----------------------------------------------------

struct HttpResponse {
    bool network_error = false; // true = could not reach the server at all
                                 // (DNS/timeout/TLS failure) - distinct from
                                 // a reachable server returning a non-200.
    int status_code = 0;
    std::string body;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    // SECURITY (Section 5): implementations MUST reject non-HTTPS URLs
    // outright (return network_error=true) rather than silently downgrading.
    virtual HttpResponse get(const std::string& url) = 0;
};

#if defined(WEATHERPAPER_WITH_CURL)
// Production HTTP client. Enforces HTTPS-only (Section 5) and reasonable
// timeouts so a hung request never blocks the poll loop indefinitely.
class CurlHttpClient : public IHttpClient {
public:
    HttpResponse get(const std::string& url) override;
};
#endif

// --- Weather data model ---------------------------------------------------

struct WeatherSnapshot {
    wallpaper_engine_core::Condition condition = wallpaper_engine_core::Condition::Clear;
    double temperature_c = 0.0;
    std::chrono::system_clock::time_point sunrise;
    std::chrono::system_clock::time_point sunset;
    std::chrono::system_clock::time_point fetched_at;
    bool is_severe = false; // storm / extreme temperature - drives notify (3.13)
    std::string provider_name; // "open-meteo" / "openweathermap" - for logs/UI, never PII

    [[nodiscard]] bool is_valid() const noexcept { return sunrise < sunset; }
};

// --- Providers --------------------------------------------------------
// Both concrete providers implement the same interface so
// WeatherFetchService can try the primary and fall back to the secondary
// transparently (Section 3.1: "Support OpenWeatherMap as a secondary/
// fallback provider behind the same interface").
class IWeatherProvider {
public:
    virtual ~IWeatherProvider() = default;
    virtual std::string name() const = 0;
    // Builds the request URL for this provider (HTTPS only), issues it via
    // `client`, and parses the response. Returns nullopt on any failure
    // (network error, non-200, or unparseable body) - never throws.
    virtual std::optional<WeatherSnapshot> fetch(
        IHttpClient& client, double latitude, double longitude) const = 0;
};

// Open-Meteo: no API key required (Section 3.1), used as the default/
// primary provider. https://open-meteo.com
class OpenMeteoProvider : public IWeatherProvider {
public:
    std::string name() const override { return "open-meteo"; }
    std::optional<WeatherSnapshot> fetch(
        IHttpClient& client, double latitude, double longitude) const override;

    // Exposed for unit testing the parser directly against fixture JSON
    // without going through IHttpClient at all.
    static std::optional<WeatherSnapshot> parse_response(const std::string& json_body);
};

// OpenWeatherMap: secondary/fallback provider (Section 3.1). Requires an
// API key, supplied by the caller (stored in AppConfig, never hardcoded).
class OpenWeatherMapProvider : public IWeatherProvider {
public:
    explicit OpenWeatherMapProvider(std::string api_key) : api_key_(std::move(api_key)) {}
    std::string name() const override { return "openweathermap"; }
    std::optional<WeatherSnapshot> fetch(
        IHttpClient& client, double latitude, double longitude) const override;

    static std::optional<WeatherSnapshot> parse_response(const std::string& json_body);

private:
    std::string api_key_;
};

// --- Disk cache (Section 3.1: "Caches the last successful response to
// local disk (JSON) so the app can operate offline using stale-but-valid
// data") -------------------------------------------------------------
[[nodiscard]] bool write_cache(const std::string& cache_file_path, const WeatherSnapshot& snapshot);
[[nodiscard]] std::optional<WeatherSnapshot> read_cache(const std::string& cache_file_path);

// --- Offline seasonal fallback (Section 3.1: "If there is no cached data
// at all ... fall back to a bundled default seasonal wallpaper based on
// system date") ---------------------------------------------------------
//
// ASSUMPTION: the spec asks for a *seasonal* fallback, which is a distinct
// concept from the weather Condition/time_of_day::Bucket tag vocabulary
// used everywhere else - there is no "season" tag defined in Section 3.8's
// standard vocabulary. This module only computes WHICH season applies for
// a given calendar date (pure, meteorological-season definition, Northern
// Hemisphere convention documented in the .cpp - astronomical/hemisphere
// nuance is a `docs/ARCHITECTURE.md` "Known Limitations" item for a future
// contributor to refine with real location-aware hemisphere detection).
// Turning that Season into an actual tag lookup ("winter" as a free-text
// custom tag on bundled assets) is the app-orchestration layer's job
// (src/app), since it needs tag_system access that this module doesn't have.
enum class Season { Spring, Summer, Autumn, Winter };
[[nodiscard]] Season season_for_date(std::chrono::year_month_day date) noexcept;
[[nodiscard]] const char* to_string(Season s) noexcept;

// --- Backoff policy (Section 3.1: "On network failure: do not crash, do
// not retry aggressively. Back off to a longer retry interval") --------
// Pure function: no sleeping/timers here (that's the app orchestration
// layer using OS timers, Section 2.2 "no busy-polling"). This just computes
// what the NEXT interval should be, given how many consecutive failures
// have occurred and the user's configured normal interval.
[[nodiscard]] std::chrono::minutes next_backoff_interval(
    std::chrono::minutes normal_interval,
    int consecutive_failures) noexcept;

} // namespace weatherpaper::weather_fetch
