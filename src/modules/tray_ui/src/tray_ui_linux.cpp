#if defined(__linux__)
#include "weatherpaper/tray_ui/tray_ui.hpp"

#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

namespace weatherpaper::tray_ui {
namespace {

// Section 3.11: native StatusNotifierItem via AppIndicator3, which is what
// GNOME (with the AppIndicator extension), KDE Plasma, XFCE, Cinnamon, and
// MATE all natively render as a tray icon - the single most broadly
// compatible native tray API across the Linux DEs this project targets.
class LinuxTrayIcon : public ITrayIcon {
public:
    ~LinuxTrayIcon() override { destroy(); }

    bool create(const TrayCallbacks& callbacks, const std::string& icon_path) override {
        callbacks_ = callbacks;

        // gtk_init is idempotent/safe to call more than once per process
        // in practice, but the app orchestration layer should only ever
        // construct one ITrayIcon - documented in tray_ui.hpp.
        if (!gtk_init_check(nullptr, nullptr)) return false;

        indicator_ = app_indicator_new("weatherpaper", icon_path.c_str(),
                                         APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
        if (indicator_ == nullptr) return false;
        app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);

        menu_ = gtk_menu_new();

        pause_item_ = gtk_menu_item_new_with_label("Pause auto-updates");
        g_signal_connect(pause_item_, "activate", G_CALLBACK(&LinuxTrayIcon::on_pause_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), pause_item_);

        GtkWidget* refresh_item = gtk_menu_item_new_with_label("Refresh Now");
        g_signal_connect(refresh_item, "activate", G_CALLBACK(&LinuxTrayIcon::on_refresh_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), refresh_item);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

        GtkWidget* settings_item = gtk_menu_item_new_with_label("Open Settings...");
        g_signal_connect(settings_item, "activate", G_CALLBACK(&LinuxTrayIcon::on_settings_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), settings_item);

        GtkWidget* gallery_item = gtk_menu_item_new_with_label("Open Gallery...");
        g_signal_connect(gallery_item, "activate", G_CALLBACK(&LinuxTrayIcon::on_gallery_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gallery_item);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

        GtkWidget* quit_item = gtk_menu_item_new_with_label("Quit WeatherPaper");
        g_signal_connect(quit_item, "activate", G_CALLBACK(&LinuxTrayIcon::on_quit_clicked), this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_), quit_item);

        gtk_widget_show_all(menu_);
        app_indicator_set_menu(indicator_, GTK_MENU(menu_));
        return true;
    }

    void set_paused_label(bool is_paused) override {
        if (pause_item_ == nullptr) return;
        gtk_menu_item_set_label(GTK_MENU_ITEM(pause_item_),
                                  is_paused ? "Resume auto-updates" : "Pause auto-updates");
    }

    void pump_events() override {
        // Drain pending GTK/GLib main-loop events (menu clicks, redraws)
        // without blocking - called from the app's existing timer tick,
        // same "no dedicated spin thread" contract as
        // platform_common::IPlatformHooks::pump_events.
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
    }

    void destroy() override {
        if (indicator_ != nullptr) {
            app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_PASSIVE);
            g_object_unref(indicator_);
            indicator_ = nullptr;
        }
        // menu_'s widgets are owned by GTK's ref-counting and were already
        // handed to app_indicator_set_menu(); no separate destroy needed.
    }

private:
    static void on_pause_clicked(GtkWidget*, gpointer data) {
        auto* self = static_cast<LinuxTrayIcon*>(data);
        if (self->callbacks_.on_toggle_pause_resume) self->callbacks_.on_toggle_pause_resume();
    }
    static void on_refresh_clicked(GtkWidget*, gpointer data) {
        auto* self = static_cast<LinuxTrayIcon*>(data);
        if (self->callbacks_.on_force_refresh_now) self->callbacks_.on_force_refresh_now();
    }
    static void on_settings_clicked(GtkWidget*, gpointer data) {
        auto* self = static_cast<LinuxTrayIcon*>(data);
        if (self->callbacks_.on_open_settings) self->callbacks_.on_open_settings();
    }
    static void on_gallery_clicked(GtkWidget*, gpointer data) {
        auto* self = static_cast<LinuxTrayIcon*>(data);
        if (self->callbacks_.on_open_gallery) self->callbacks_.on_open_gallery();
    }
    static void on_quit_clicked(GtkWidget*, gpointer data) {
        auto* self = static_cast<LinuxTrayIcon*>(data);
        if (self->callbacks_.on_quit) self->callbacks_.on_quit();
    }

    TrayCallbacks callbacks_;
    AppIndicator* indicator_ = nullptr;
    GtkWidget* menu_ = nullptr;
    GtkWidget* pause_item_ = nullptr;
};

} // namespace

std::unique_ptr<ITrayIcon> create_platform_tray_icon() {
    return std::make_unique<LinuxTrayIcon>();
}

} // namespace weatherpaper::tray_ui
#endif // __linux__
