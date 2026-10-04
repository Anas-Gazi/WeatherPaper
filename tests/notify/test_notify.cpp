#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>

#include "weatherpaper/notify/notify.hpp"

using namespace weatherpaper::notify;
using weatherpaper::wallpaper_engine_core::Condition;

TEST_CASE("format_severe_weather_notification: storm produces a storm-specific title") {
    SevereWeatherAlert alert{Condition::Storm, 22.0, "Dhaka, Bangladesh"};
    auto text = format_severe_weather_notification(alert);
    CHECK(text.title == "Severe Weather Alert: Storm");
    CHECK(text.body.find("Dhaka, Bangladesh") != std::string::npos);
    CHECK(text.body.find("storm") != std::string::npos);
}

TEST_CASE("format_severe_weather_notification: extreme cold and heat get distinct titles") {
    SevereWeatherAlert cold{Condition::Snow, -15.0, ""};
    CHECK(format_severe_weather_notification(cold).title == "Severe Weather Alert: Extreme Cold");

    SevereWeatherAlert hot{Condition::Sunny, 42.0, ""};
    CHECK(format_severe_weather_notification(hot).title == "Severe Weather Alert: Extreme Heat");
}

TEST_CASE("format_severe_weather_notification omits location gracefully when unset") {
    SevereWeatherAlert alert{Condition::Storm, 25.0, ""};
    auto text = format_severe_weather_notification(alert);
    CHECK(text.body.find(" in ") == std::string::npos); // no dangling "in " with empty location
}

TEST_CASE("format_severe_weather_notification never includes raw coordinates (Section 5 privacy)") {
    // SevereWeatherAlert only ever carries a display name, never lat/lon -
    // this test documents that guarantee at the type level: there is no
    // coordinate field to accidentally format into the body.
    SevereWeatherAlert alert{Condition::Storm, 30.0, "Some City"};
    auto text = format_severe_weather_notification(alert);
    CHECK(text.body.find(".") != std::string::npos); // has a temperature-terminating period
    // A crude sanity check that no long digit sequence resembling a
    // coordinate string leaked in.
    int consecutive_digits = 0, max_consecutive = 0;
    for (char c : text.body) {
        if (isdigit(static_cast<unsigned char>(c))) {
            consecutive_digits++;
            max_consecutive = std::max(max_consecutive, consecutive_digits);
        } else {
            consecutive_digits = 0;
        }
    }
    CHECK(max_consecutive < 6); // temperature is at most a couple digits
}

TEST_CASE("NullNotifier::show never throws and is safe to call with any input") {
    NullNotifier n;
    CHECK_NOTHROW(n.show(NotificationText{"title", "body"}));
    CHECK_NOTHROW(n.show(NotificationText{}));
}
