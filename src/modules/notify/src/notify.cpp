#include "weatherpaper/notify/notify.hpp"

#include <sstream>

namespace weatherpaper::notify {

NotificationText format_severe_weather_notification(const SevereWeatherAlert& alert) {
    NotificationText text;
    std::ostringstream body;

    const auto condition_name = wallpaper_engine_core::to_string(alert.condition);
    if (alert.condition == wallpaper_engine_core::Condition::Storm) {
        text.title = "Severe Weather Alert: Storm";
    } else if (alert.temperature_c <= -10.0) {
        text.title = "Severe Weather Alert: Extreme Cold";
    } else if (alert.temperature_c >= 40.0) {
        text.title = "Severe Weather Alert: Extreme Heat";
    } else {
        text.title = "Severe Weather Alert";
    }

    body << "Current conditions";
    if (!alert.location_display_name.empty()) {
        body << " in " << alert.location_display_name;
    }
    body << ": " << condition_name << ", " << static_cast<int>(alert.temperature_c) << "\u00B0C.";
    text.body = body.str();
    return text;
}

std::unique_ptr<INotifier> create_platform_notifier() {
#if defined(_WIN32)
    return std::make_unique<WindowsToastNotifier>();
#elif defined(__linux__)
    return std::make_unique<LinuxNotifySendNotifier>();
#else
    return std::make_unique<NullNotifier>();
#endif
}

} // namespace weatherpaper::notify
