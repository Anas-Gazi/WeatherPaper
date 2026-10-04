// weatherpaper/settings_ui/settings_ui.hpp
//
// Module: settings_ui (Section 3.12)
// Layer:  Qt Widgets GUI. Section 3.12 explicitly calls for "a lightweight
// native or near-native GUI toolkit ... Qt Widgets or Dear ImGui ... avoid
// a full embedded browser/Chromium-based UI stack" and a CLASSIC look:
// "traditional menu bar, tabbed or sidebar-navigated settings panels,
// standard buttons and form controls, muted/neutral color palette, no
// glassmorphism, no heavy animation". Qt Widgets was chosen over Dear
// ImGui specifically because Section 3.12 also requires standard native-
// feeling form controls (checkboxes, dropdowns, file pickers, drag-and-
// drop) which Qt Widgets provides out of the box and immediate-mode ImGui
// does not (ImGui would need every one of those rebuilt by hand).
//
// This header exposes exactly one entry point - MainWindow, a QMainWindow
// subclass - so the rest of the codebase (src/app/main.cpp) never needs to
// know Qt exists; everything else in this module is private (.cpp-only).
// MainWindow talks to the engine exclusively through the interfaces
// already defined elsewhere (config::AppConfig, tag_system::TagIndex,
// asset_manager::ThemePackManager) - it holds no wallpaper-selection logic
// of its own, keeping this a pure presentation layer.
#pragma once

#include <QMainWindow>

#include <functional>
#include <memory>

#include "weatherpaper/asset_manager/asset_manager.hpp"
#include "weatherpaper/config/config.hpp"
#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"
#include "weatherpaper/tag_system/tag_system.hpp"

namespace weatherpaper::settings_ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(config::AppConfig* app_config,
                tag_system::TagIndex* tag_index,
                asset_manager::ThemePackManager* pack_manager,
                QWidget* parent = nullptr);
    ~MainWindow() override;

    void set_apply_wallpaper_callback(std::function<void(const std::string&, scaling_and_fit::FitMode, tag_system::AssetType)> cb);
    void set_force_refresh_callback(std::function<void()> cb);
    void select_tab(int index);

private:
    void build_ui();
    QWidget* build_general_tab();
    QWidget* build_themes_tab();
    QWidget* build_gallery_tab();
    QWidget* build_performance_tab();
    QWidget* build_about_tab();

    void refresh_gallery_list();
    void on_add_gallery_file_clicked();
    void on_save_settings_clicked();

    struct Impl; // private widget members (Section 2.1: hidden implementation)
    std::unique_ptr<Impl> impl_;

    config::AppConfig* app_config_;
    tag_system::TagIndex* tag_index_;
    asset_manager::ThemePackManager* pack_manager_;
};

} // namespace weatherpaper::settings_ui
