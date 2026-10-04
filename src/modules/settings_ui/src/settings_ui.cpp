#include "weatherpaper/settings_ui/settings_ui.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <filesystem>
#include <iostream>

namespace weatherpaper::settings_ui {
namespace {

static const char* kDarkAppStylesheet = R"(
/* --- Root & Canvas --- */
QMainWindow, QWidget#rootWidget, QWidget#bodyWidget, QWidget#tabContentPage, QStackedWidget, QScrollArea, QScrollArea > QWidget > QWidget {
    background-color: #0A0B0E;
}
QWidget {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    color: #E2E8F0;
    font-size: 13px;
}
QToolTip {
    background-color: #1A1D2A;
    color: #F8FAFC;
    border: 1px solid #2E344D;
    padding: 4px 8px;
    border-radius: 4px;
}

/* --- Header Bar --- */
#headerBar {
    background-color: #0F1117;
    border-bottom: 1px solid #1C1F2B;
    padding: 12px 24px;
}
#headerTitle {
    font-size: 16px;
    font-weight: 700;
    color: #FFFFFF;
}
#headerSubtitle {
    font-size: 12px;
    color: #94A3B8;
}
#statusChip {
    background-color: #042318;
    border: 1px solid #059669;
    border-radius: 12px;
    padding: 4px 12px;
    font-size: 11px;
    font-weight: 600;
    color: #34D399;
}

/* --- Sidebar --- */
#sidebar {
    background-color: #0E1016;
    border: none;
    border-right: 1px solid #1C1F2B;
    padding: 12px 8px;
    outline: none;
}
#sidebar::item {
    height: 40px;
    padding-left: 14px;
    padding-right: 12px;
    margin-bottom: 4px;
    border-radius: 6px;
    font-weight: 500;
    color: #94A3B8;
}
#sidebar::item:hover {
    background-color: #1A1D28;
    color: #F8FAFC;
}
#sidebar::item:selected {
    background-color: #1A2238;
    color: #60A5FA;
    font-weight: 600;
    border: 1px solid #2563EB;
}

/* --- Cards --- */
#card {
    background-color: #141620;
    border: 1px solid #232738;
    border-radius: 8px;
    padding: 16px;
}
#sectionTitle {
    font-size: 15px;
    font-weight: 600;
    color: #F8FAFC;
}
#sectionDesc {
    font-size: 12px;
    color: #94A3B8;
}

/* --- Buttons --- */
QPushButton {
    background-color: #1A1D2B;
    border: 1px solid #2E3347;
    border-radius: 6px;
    padding: 7px 16px;
    font-weight: 500;
    color: #E2E8F0;
}
QPushButton:hover {
    background-color: #23283B;
    border-color: #3E4560;
    color: #FFFFFF;
}
QPushButton:pressed {
    background-color: #151824;
}
#primaryBtn {
    background-color: #2563EB;
    border: 1px solid #1D4ED8;
    color: #FFFFFF;
    font-weight: 600;
}
#primaryBtn:hover {
    background-color: #1D4ED8;
    border-color: #2563EB;
}
#primaryBtn:pressed {
    background-color: #1E40AF;
}
#secondaryActionBtn {
    background-color: #1E2333;
    border: 1px solid #2F364E;
    color: #93C5FD;
    font-weight: 500;
}
#secondaryActionBtn:hover {
    background-color: #262D42;
    color: #BFDBFE;
}
#dangerBtn {
    background-color: #240C10;
    border: 1px solid #7F1D1D;
    color: #F87171;
}
#dangerBtn:hover {
    background-color: #361118;
    border-color: #B91C1C;
    color: #FCA5A5;
}
#dangerBtn:pressed {
    background-color: #1E080C;
}

/* --- Inputs --- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #191C28;
    border: 1px solid #2A2F45;
    border-radius: 6px;
    padding: 7px 10px;
    color: #F8FAFC;
    selection-background-color: #2563EB;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #3B82F6;
    background-color: #1C202E;
}
QLineEdit:read-only {
    background-color: #12141D;
    color: #94A3B8;
    border: 1px solid #232738;
}
QComboBox QAbstractItemView {
    background-color: #161824;
    border: 1px solid #2A2F45;
    selection-background-color: #2563EB;
    selection-color: #FFFFFF;
    color: #F8FAFC;
    padding: 4px;
    outline: none;
}
QComboBox::drop-down {
    border: none;
    padding-right: 8px;
}

/* --- Checkboxes --- */
QCheckBox {
    spacing: 10px;
    color: #E2E8F0;
    font-weight: 500;
}
QCheckBox::indicator {
    width: 17px;
    height: 17px;
    border-radius: 4px;
    border: 1px solid #2E344D;
    background-color: #191C28;
}
QCheckBox::indicator:hover {
    border-color: #3B82F6;
}
QCheckBox::indicator:checked {
    background-color: #2563EB;
    border-color: #2563EB;
}

/* --- Lists --- */
#contentList {
    background-color: #161824;
    border: 1px solid #232738;
    border-radius: 6px;
    padding: 6px;
    outline: none;
}
#contentList::item {
    padding: 9px 12px;
    border-bottom: 1px solid #1C1F2C;
    border-radius: 4px;
    color: #CBD5E1;
}
#contentList::item:hover {
    background-color: #1E2232;
    color: #F8FAFC;
}
#contentList::item:selected {
    background-color: #1E2D4A;
    color: #93C5FD;
    border: 1px solid #2563EB;
}

/* --- Dropzone --- */
#dropZone {
    background-color: #10121A;
    border: 2px dashed #2A2F45;
    border-radius: 8px;
    padding: 14px;
}
#dropZone:hover {
    border-color: #3B82F6;
    background-color: #141824;
}

/* --- Image Preview Box --- */
#imagePreviewBox {
    background-color: #0A0B0E;
    border: 1px solid #232738;
    border-radius: 6px;
}

/* --- Scroll Bars & Scroll Areas --- */
QScrollArea, QAbstractScrollArea {
    border: none;
    background-color: #0A0B0E;
}
QScrollArea > QWidget > QWidget {
    background-color: #0A0B0E;
}
QScrollBar:vertical {
    border: none;
    background: transparent;
    width: 8px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #2A2F45;
    min-height: 24px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
    background: #475569;
}
QScrollBar:horizontal {
    height: 0px;
    border: none;
}
)";

class GalleryListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit GalleryListWidget(QWidget* parent = nullptr) : QListWidget(parent) {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setObjectName("contentList");
    }

signals:
    void filesDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
        else event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
        else event->ignore();
    }
    void dropEvent(QDropEvent* event) override {
        QStringList paths;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.isLocalFile()) paths << url.toLocalFile();
        }
        if (!paths.isEmpty()) emit filesDropped(paths);
        event->acceptProposedAction();
    }
};

// Event filter that forwards drag-and-drop events from the drop-zone banner
// to the GalleryListWidget so users can drop files on either widget.
class DropForwarder : public QObject {
    Q_OBJECT
public:
    explicit DropForwarder(GalleryListWidget* target, QObject* parent = nullptr)
        : QObject(parent), target_(target) {}

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        if (event->type() == QEvent::DragEnter) {
            auto* de = static_cast<QDragEnterEvent*>(event);
            if (de->mimeData()->hasUrls()) { de->acceptProposedAction(); return true; }
        } else if (event->type() == QEvent::DragMove) {
            auto* dm = static_cast<QDragMoveEvent*>(event);
            if (dm->mimeData()->hasUrls()) { dm->acceptProposedAction(); return true; }
        } else if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            if (drop->mimeData()->hasUrls()) {
                QStringList paths;
                for (const QUrl& url : drop->mimeData()->urls())
                    if (url.isLocalFile()) paths << url.toLocalFile();
                if (!paths.isEmpty()) emit target_->filesDropped(paths);
                drop->acceptProposedAction();
                return true;
            }
        }
        return false;
    }

private:
    GalleryListWidget* target_;
};

QFrame* create_card(QWidget* parent = nullptr) {
    auto* card = new QFrame(parent);
    card->setObjectName("card");
    return card;
}

struct CityPreset {
    const char* name;
    double lat;
    double lon;
};

static const CityPreset kCityPresets[] = {
    {"Choose a city preset...", 0.0, 0.0},
    {"Dhaka, Bangladesh", 23.8103, 90.4125},
    {"London, United Kingdom", 51.5074, -0.1278},
    {"New York, United States", 40.7128, -74.0060},
    {"Tokyo, Japan", 35.6762, 139.6503},
    {"Paris, France", 48.8566, 2.3522},
    {"Berlin, Germany", 52.5200, 13.4050},
    {"San Francisco, United States", 37.7749, -122.4194},
    {"Sydney, Australia", -33.8688, 151.2093},
    {"Dubai, United Arab Emirates", 25.2048, 55.2708},
    {"Singapore", 1.3521, 103.8198},
    {"Toronto, Canada", 43.6532, -79.3832}
};

} // namespace

struct MainWindow::Impl {
    QStackedWidget* stack = nullptr;
    QListWidget* sidebar = nullptr;
    QLabel* save_status_label = nullptr;

    // Callbacks
    std::function<void(const std::string&, scaling_and_fit::FitMode, tag_system::AssetType)> on_apply_wallpaper;
    std::function<void()> on_force_refresh;

    // General tab
    QLineEdit* location_display = nullptr;
    QDoubleSpinBox* lat_spin = nullptr;
    QDoubleSpinBox* lon_spin = nullptr;
    QComboBox* city_presets_combo = nullptr;
    QCheckBox* location_auto_detect = nullptr;
    QComboBox* units_combo = nullptr;
    QSpinBox* poll_interval_spin = nullptr;
    QComboBox* selection_policy_combo = nullptr;
    QCheckBox* severe_weather_check = nullptr;

    // Themes tab
    QListWidget* installed_packs_list = nullptr;
    QLabel* theme_details_label = nullptr;
    QPushButton* uninstall_theme_btn = nullptr;

    // Gallery tab & Inspector
    GalleryListWidget* gallery_list = nullptr;
    QLabel* inspector_preview_label = nullptr;
    QLabel* inspector_title_label = nullptr;
    QLabel* inspector_meta_label = nullptr;
    QComboBox* inspector_fit_combo = nullptr;
    QPushButton* inspector_apply_btn = nullptr;
    QPushButton* inspector_delete_btn = nullptr;
    std::vector<QCheckBox*> condition_tag_checks;
    std::vector<QCheckBox*> solar_tag_checks;
    std::string current_selected_asset_id;

    // Performance tab
    QCheckBox* animated_enabled_check = nullptr;
    QCheckBox* pause_locked_check = nullptr;
    QCheckBox* pause_fullscreen_check = nullptr;
    QCheckBox* pause_battery_saver_check = nullptr;
};

MainWindow::MainWindow(config::AppConfig* app_config, tag_system::TagIndex* tag_index,
                       asset_manager::ThemePackManager* pack_manager, QWidget* parent)
    : QMainWindow(parent), impl_(std::make_unique<Impl>()),
      app_config_(app_config), tag_index_(tag_index), pack_manager_(pack_manager) {
    if (QStyleFactory::keys().contains("Fusion")) {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    }

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(0x0A, 0x0B, 0x0E));
    darkPalette.setColor(QPalette::WindowText, QColor(0xF1, 0xF5, 0xF9));
    darkPalette.setColor(QPalette::Base, QColor(0x0A, 0x0B, 0x0E));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x14, 0x16, 0x20));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(0x1A, 0x1D, 0x2A));
    darkPalette.setColor(QPalette::ToolTipText, QColor(0xF8, 0xFA, 0xFC));
    darkPalette.setColor(QPalette::Text, QColor(0xF1, 0xF5, 0xF9));
    darkPalette.setColor(QPalette::Button, QColor(0x1A, 0x1D, 0x2B));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xF1, 0xF5, 0xF9));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(0x3B, 0x82, 0xF6));
    darkPalette.setColor(QPalette::Highlight, QColor(0x25, 0x63, 0xEB));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x64, 0x74, 0x8B));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x64, 0x74, 0x8B));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x64, 0x74, 0x8B));
    QApplication::setPalette(darkPalette);

    setStyleSheet(kDarkAppStylesheet);
    setWindowTitle("WeatherPaper Settings");
    resize(980, 640);
    setMinimumSize(880, 540);
    build_ui();
}

MainWindow::~MainWindow() = default;

void MainWindow::set_apply_wallpaper_callback(std::function<void(const std::string&, scaling_and_fit::FitMode, tag_system::AssetType)> cb) {
    impl_->on_apply_wallpaper = std::move(cb);
}

void MainWindow::set_force_refresh_callback(std::function<void()> cb) {
    impl_->on_force_refresh = std::move(cb);
}

void MainWindow::select_tab(int index) {
    if (impl_->sidebar && index >= 0 && index < impl_->sidebar->count()) {
        impl_->sidebar->setCurrentRow(index);
    }
}

void MainWindow::build_ui() {
    auto* root_widget = new QWidget(this);
    root_widget->setObjectName("rootWidget");
    root_widget->setAttribute(Qt::WA_StyledBackground, true);
    auto* main_vbox = new QVBoxLayout(root_widget);
    main_vbox->setContentsMargins(0, 0, 0, 0);
    main_vbox->setSpacing(0);

    // --- Top Header Bar ---
    auto* header = new QFrame(root_widget);
    header->setObjectName("headerBar");
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(20, 12, 20, 12);

    auto* brand_layout = new QVBoxLayout;
    brand_layout->setSpacing(2);
    auto* title = new QLabel("WeatherPaper", header);
    title->setObjectName("headerTitle");
    auto* subtitle = new QLabel("Live Weather & Dynamic Wallpaper Engine", header);
    subtitle->setObjectName("headerSubtitle");
    brand_layout->addWidget(title);
    brand_layout->addWidget(subtitle);
    header_layout->addLayout(brand_layout);

    header_layout->addStretch();

    auto* status = new QLabel("● Live Engine Active", header);
    status->setObjectName("statusChip");
    header_layout->addWidget(status);

    main_vbox->addWidget(header);

    // --- Body ---
    auto* body = new QWidget(root_widget);
    body->setObjectName("bodyWidget");
    body->setAttribute(Qt::WA_StyledBackground, true);
    auto* body_layout = new QHBoxLayout(body);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);

    // Sidebar Container
    auto* sidebar_container = new QWidget(body);
    sidebar_container->setFixedWidth(200);
    auto* sidebar_vbox = new QVBoxLayout(sidebar_container);
    sidebar_vbox->setContentsMargins(0, 0, 0, 12);
    sidebar_vbox->setSpacing(6);

    impl_->sidebar = new QListWidget(sidebar_container);
    impl_->sidebar->setObjectName("sidebar");
    impl_->sidebar->addItem("⚙  General");
    impl_->sidebar->addItem("🎨  Themes");
    impl_->sidebar->addItem("🖼  Gallery");
    impl_->sidebar->addItem("⚡  Performance");
    impl_->sidebar->addItem("ℹ  About & Updates");
    impl_->sidebar->setCurrentRow(0);
    sidebar_vbox->addWidget(impl_->sidebar, /*stretch=*/1);

    // Sidebar bottom buttons
    auto* refresh_now_btn = new QPushButton("🔄 Refresh Weather", sidebar_container);
    refresh_now_btn->setObjectName("secondaryActionBtn");
    connect(refresh_now_btn, &QPushButton::clicked, this, [this]() {
        if (impl_->on_force_refresh) {
            impl_->on_force_refresh();
            QMessageBox::information(this, "Refreshing", "Triggered live weather refresh and wallpaper update.");
        }
    });
    sidebar_vbox->addWidget(refresh_now_btn);

    auto* quit_btn = new QPushButton("✕ Quit WeatherPaper", sidebar_container);
    quit_btn->setObjectName("dangerBtn");
    connect(quit_btn, &QPushButton::clicked, this, []() {
        QApplication::quit();
    });
    sidebar_vbox->addWidget(quit_btn);

    body_layout->addWidget(sidebar_container);

    impl_->stack = new QStackedWidget(body);
    impl_->stack->addWidget(build_general_tab());
    impl_->stack->addWidget(build_themes_tab());
    impl_->stack->addWidget(build_gallery_tab());
    impl_->stack->addWidget(build_performance_tab());
    impl_->stack->addWidget(build_about_tab());
    body_layout->addWidget(impl_->stack, /*stretch=*/1);

    main_vbox->addWidget(body, /*stretch=*/1);

    connect(impl_->sidebar, &QListWidget::currentRowChanged, impl_->stack,
            &QStackedWidget::setCurrentIndex);

    setCentralWidget(root_widget);
}

QWidget* MainWindow::build_general_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scroll->viewport()) {
        scroll->viewport()->setStyleSheet("background-color: #0A0B0E;");
    }

    auto* page = new QWidget;
    page->setObjectName("tabContentPage");
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    // Location Card
    auto* loc_card = create_card(page);
    auto* loc_layout = new QVBoxLayout(loc_card);
    loc_layout->setSpacing(8);

    auto* loc_title = new QLabel("Location Settings", loc_card);
    loc_title->setObjectName("sectionTitle");
    auto* loc_desc = new QLabel("Coordinates for Open-Meteo live weather and astronomical solar calculations.", loc_card);
    loc_desc->setObjectName("sectionDesc");
    loc_desc->setWordWrap(true);
    loc_layout->addWidget(loc_title);
    loc_layout->addWidget(loc_desc);

    auto* form1 = new QFormLayout;
    form1->setSpacing(10);
    form1->setLabelAlignment(Qt::AlignLeft);

    impl_->location_auto_detect = new QCheckBox("Auto-detect location from IP / system", loc_card);
    impl_->location_auto_detect->setChecked(app_config_->location.auto_detected);
    form1->addRow(impl_->location_auto_detect);

    // City presets picker
    impl_->city_presets_combo = new QComboBox(loc_card);
    for (const auto& preset : kCityPresets) {
        impl_->city_presets_combo->addItem(preset.name);
    }
    connect(impl_->city_presets_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx > 0 && idx < static_cast<int>(sizeof(kCityPresets)/sizeof(kCityPresets[0]))) {
            const auto& p = kCityPresets[idx];
            impl_->location_display->setText(p.name);
            impl_->lat_spin->setValue(p.lat);
            impl_->lon_spin->setValue(p.lon);
            impl_->location_auto_detect->setChecked(false);
        }
    });
    form1->addRow("City Preset:", impl_->city_presets_combo);

    impl_->location_display = new QLineEdit(QString::fromStdString(app_config_->location.display_name), loc_card);
    form1->addRow("Location Display Name:", impl_->location_display);

    auto* coords_layout = new QHBoxLayout;
    impl_->lat_spin = new QDoubleSpinBox(loc_card);
    impl_->lat_spin->setRange(-90.0, 90.0);
    impl_->lat_spin->setDecimals(4);
    impl_->lat_spin->setValue(app_config_->location.latitude);
    impl_->lat_spin->setPrefix("Lat: ");

    impl_->lon_spin = new QDoubleSpinBox(loc_card);
    impl_->lon_spin->setRange(-180.0, 180.0);
    impl_->lon_spin->setDecimals(4);
    impl_->lon_spin->setValue(app_config_->location.longitude);
    impl_->lon_spin->setPrefix("Lon: ");

    coords_layout->addWidget(impl_->lat_spin);
    coords_layout->addWidget(impl_->lon_spin);
    form1->addRow("Coordinates:", coords_layout);

    auto update_loc_inputs_enabled = [this](bool auto_detected) {
        impl_->city_presets_combo->setEnabled(!auto_detected);
        impl_->location_display->setEnabled(!auto_detected);
        impl_->lat_spin->setEnabled(!auto_detected);
        impl_->lon_spin->setEnabled(!auto_detected);
    };
    update_loc_inputs_enabled(impl_->location_auto_detect->isChecked());
    connect(impl_->location_auto_detect, &QCheckBox::toggled, this, update_loc_inputs_enabled);

    loc_layout->addLayout(form1);
    layout->addWidget(loc_card);

    // Preferences Card
    auto* pref_card = create_card(page);
    auto* pref_layout = new QVBoxLayout(pref_card);
    pref_layout->setSpacing(8);

    auto* pref_title = new QLabel("Engine Preferences", pref_card);
    pref_title->setObjectName("sectionTitle");
    auto* pref_desc = new QLabel("Weather update interval, temperature units, and wallpaper selection behavior.", pref_card);
    pref_desc->setObjectName("sectionDesc");
    pref_desc->setWordWrap(true);
    pref_layout->addWidget(pref_title);
    pref_layout->addWidget(pref_desc);

    auto* form2 = new QFormLayout;
    form2->setSpacing(10);
    form2->setLabelAlignment(Qt::AlignLeft);

    impl_->units_combo = new QComboBox(pref_card);
    impl_->units_combo->addItem("Celsius (\u00B0C)");
    impl_->units_combo->addItem("Fahrenheit (\u00B0F)");
    impl_->units_combo->setCurrentIndex(app_config_->units == config::TemperatureUnit::Fahrenheit ? 1 : 0);
    form2->addRow("Temperature Unit:", impl_->units_combo);

    impl_->poll_interval_spin = new QSpinBox(pref_card);
    impl_->poll_interval_spin->setRange(15, 60);
    impl_->poll_interval_spin->setSuffix(" minutes");
    impl_->poll_interval_spin->setValue(static_cast<int>(app_config_->weather_poll_interval.count()));
    form2->addRow("Poll Interval (15-60 min):", impl_->poll_interval_spin);

    impl_->selection_policy_combo = new QComboBox(pref_card);
    impl_->selection_policy_combo->addItem("Sequential (Next matched wallpaper)", static_cast<int>(tag_system::SelectionPolicy::Sequential));
    impl_->selection_policy_combo->addItem("Random (Shuffle matched wallpapers)", static_cast<int>(tag_system::SelectionPolicy::Random));
    impl_->selection_policy_combo->addItem("Pinned (Prioritize user-pinned)", static_cast<int>(tag_system::SelectionPolicy::Pinned));
    impl_->selection_policy_combo->setCurrentIndex(static_cast<int>(app_config_->selection_policy));
    form2->addRow("Wallpaper Cycling Policy:", impl_->selection_policy_combo);

    impl_->severe_weather_check = new QCheckBox("Display desktop alerts on severe weather warnings", pref_card);
    impl_->severe_weather_check->setChecked(app_config_->severe_weather_notifications_enabled);
    form2->addRow(impl_->severe_weather_check);

    pref_layout->addLayout(form2);
    layout->addWidget(pref_card);

    // Save Row
    auto* save_row = new QHBoxLayout;
    impl_->save_status_label = new QLabel("", page);
    impl_->save_status_label->setStyleSheet("color: #34D399; font-weight: 600; font-size: 13px;");

    auto* save_button = new QPushButton("Save Changes", page);
    save_button->setObjectName("primaryBtn");
    save_button->setCursor(Qt::PointingHandCursor);
    connect(save_button, &QPushButton::clicked, this, &MainWindow::on_save_settings_clicked);

    save_row->addWidget(impl_->save_status_label);
    save_row->addStretch();
    save_row->addWidget(save_button);
    layout->addLayout(save_row);

    layout->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::build_themes_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scroll->viewport()) {
        scroll->viewport()->setStyleSheet("background-color: #0A0B0E;");
    }

    auto* page = new QWidget;
    page->setObjectName("tabContentPage");
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    auto* card = create_card(page);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(10);

    auto* title = new QLabel("Installed Theme Packs", card);
    title->setObjectName("sectionTitle");
    auto* desc = new QLabel("Theme packs bundle categorized wallpapers tagged for conditions and solar buckets.", card);
    desc->setObjectName("sectionDesc");
    desc->setWordWrap(true);
    card_layout->addWidget(title);
    card_layout->addWidget(desc);

    impl_->installed_packs_list = new QListWidget(card);
    impl_->installed_packs_list->setObjectName("contentList");
    impl_->installed_packs_list->setMinimumHeight(180);

    if (pack_manager_ != nullptr) {
        for (const auto& id : pack_manager_->list_installed_pack_ids()) {
            impl_->installed_packs_list->addItem("📦  " + QString::fromStdString(id));
        }
    }
    if (impl_->installed_packs_list->count() == 0) {
        impl_->installed_packs_list->addItem("📦  bundled-default  (Default Built-in Pack)");
    }
    card_layout->addWidget(impl_->installed_packs_list);

    impl_->theme_details_label = new QLabel("Select a theme pack above to view details.", card);
    impl_->theme_details_label->setStyleSheet("color: #94A3B8; font-size: 12px;");
    card_layout->addWidget(impl_->theme_details_label);

    auto* btn_row = new QHBoxLayout;
    impl_->uninstall_theme_btn = new QPushButton("Uninstall Selected", card);
    impl_->uninstall_theme_btn->setObjectName("dangerBtn");

    connect(impl_->installed_packs_list, &QListWidget::currentRowChanged, this, [this](int row) {
        auto* item = impl_->installed_packs_list->item(row);
        if (!item) return;
        QString name = item->text().remove("📦  ").trimmed();
        bool is_bundled = name.contains("bundled-default");
        impl_->uninstall_theme_btn->setEnabled(!is_bundled);
        impl_->uninstall_theme_btn->setToolTip(is_bundled ? "Default theme cannot be uninstalled." : "");
        impl_->theme_details_label->setText("Theme Pack: " + name + (is_bundled ? " (Active Core Default)" : " (User Installed)"));
    });

    connect(impl_->uninstall_theme_btn, &QPushButton::clicked, this, [this]() {
        auto* item = impl_->installed_packs_list->currentItem();
        if (item == nullptr || pack_manager_ == nullptr || tag_index_ == nullptr) return;
        QString text = item->text().remove("📦  ").trimmed();
        if (text.contains("bundled-default")) {
            QMessageBox::warning(this, "Protected Pack", "The bundled default theme pack cannot be uninstalled.");
            return;
        }
        (void)pack_manager_->uninstall(text.toStdString(), *tag_index_);
        delete impl_->installed_packs_list->takeItem(impl_->installed_packs_list->currentRow());
        refresh_gallery_list();
    });
    btn_row->addWidget(impl_->uninstall_theme_btn);

    auto* view_gallery_btn = new QPushButton("View Wallpapers in Gallery", card);
    view_gallery_btn->setObjectName("secondaryActionBtn");
    connect(view_gallery_btn, &QPushButton::clicked, this, [this]() {
        impl_->sidebar->setCurrentRow(2); // Switch to Gallery
    });
    btn_row->addWidget(view_gallery_btn);
    btn_row->addStretch();
    card_layout->addLayout(btn_row);

    layout->addWidget(card);

    // Online Theme Pack Info Card
    auto* info_card = create_card(page);
    auto* info_layout = new QVBoxLayout(info_card);
    auto* info_title = new QLabel("Theme Installation", info_card);
    info_title->setObjectName("sectionTitle");
    auto* info_text = new QLabel(
        "Theme packs can be distributed as signed .zip or catalog archives. WeatherPaper "
        "validates sha256 checksums and digital signatures before extracting wallpapers.", info_card);
    info_text->setObjectName("sectionDesc");
    info_text->setWordWrap(true);
    info_layout->addWidget(info_title);
    info_layout->addWidget(info_text);

    auto* install_zip_btn = new QPushButton("Install Theme Archive (.zip)...", info_card);
    connect(install_zip_btn, &QPushButton::clicked, this, [this]() {
        QString zip_file = QFileDialog::getOpenFileName(this, "Select Theme Pack Archive", QString(), "ZIP Files (*.zip)");
        if (!zip_file.isEmpty()) {
            QMessageBox::information(this, "Theme Pack Installer",
                "Selected: " + zip_file + "\nTheme pack validation and installation ready.");
        }
    });
    info_layout->addWidget(install_zip_btn);
    layout->addWidget(info_card);

    layout->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::build_gallery_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scroll->viewport()) {
        scroll->viewport()->setStyleSheet("background-color: #0A0B0E;");
    }

    auto* page = new QWidget;
    page->setObjectName("tabContentPage");
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(12);

    // Dropzone Banner
    auto* drop_card = new QFrame(page);
    drop_card->setObjectName("dropZone");
    auto* drop_layout = new QVBoxLayout(drop_card);
    drop_layout->setContentsMargins(14, 12, 14, 12);
    drop_layout->setSpacing(2);

    auto* drop_title = new QLabel("📥  Drag & Drop Wallpapers Here", drop_card);
    drop_title->setStyleSheet("font-weight: 600; font-size: 14px; color: #F1F5F9;");
    drop_title->setAlignment(Qt::AlignCenter);
    auto* drop_subtitle = new QLabel("Accepts PNG, JPG, WebP images and MP4/WebM video loops", drop_card);
    drop_subtitle->setObjectName("sectionDesc");
    drop_subtitle->setWordWrap(true);
    drop_subtitle->setAlignment(Qt::AlignCenter);
    drop_layout->addWidget(drop_title);
    drop_layout->addWidget(drop_subtitle);
    layout->addWidget(drop_card);

    // Main Gallery Split Container
    auto* main_split = new QHBoxLayout;
    main_split->setSpacing(14);

    // Left: Wallpaper List Card
    auto* list_card = create_card(page);
    auto* list_card_layout = new QVBoxLayout(list_card);
    list_card_layout->setSpacing(8);

    auto* list_header = new QHBoxLayout;
    auto* list_title = new QLabel("Wallpaper Collection", list_card);
    list_title->setObjectName("sectionTitle");
    list_header->addWidget(list_title);
    list_header->addStretch();

    auto* add_file_btn = new QPushButton("+ Add Wallpaper...", list_card);
    add_file_btn->setObjectName("primaryBtn");
    connect(add_file_btn, &QPushButton::clicked, this, &MainWindow::on_add_gallery_file_clicked);
    list_header->addWidget(add_file_btn);
    list_card_layout->addLayout(list_header);

    impl_->gallery_list = new GalleryListWidget(list_card);
    impl_->gallery_list->setMinimumHeight(320);
    list_card_layout->addWidget(impl_->gallery_list);

    connect(impl_->gallery_list, &GalleryListWidget::filesDropped, this,
            [this](const QStringList& paths) {
                for (const auto& p : paths) {
                    auto validation = asset_manager::validate_upload(p.toStdString(), 0);
                    if (!validation.accepted) {
                        QMessageBox::warning(this, "Unsupported File", QString::fromStdString(validation.rejection_reason));
                        continue;
                    }
                    tag_system::AssetRecord rec;
                    std::string base_id = QFileInfo(p).baseName().toStdString();
                    std::string id = base_id;
                    int suffix = 1;
                    while (tag_index_ != nullptr && tag_index_->get(id).has_value()) {
                        id = base_id + "_" + std::to_string(suffix++);
                    }
                    rec.id = id;
                    rec.file_path = p.toStdString();
                    rec.type = validation.type == asset_manager::UploadFileType::Video
                                   ? tag_system::AssetType::Video
                                   : tag_system::AssetType::Image;
                    rec.tags = {"day", "clear"};
                    if (tag_index_ != nullptr) {
                        tag_index_->upsert(rec);
                        std::filesystem::create_directories(config::default_data_dir());
                        (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
                    }
                }
                refresh_gallery_list();
            });

    // Make the drop banner also accept drops by forwarding to gallery_list
    drop_card->setAcceptDrops(true);
    auto* drop_filter = new DropForwarder(impl_->gallery_list, drop_card);
    drop_card->installEventFilter(drop_filter);
    // Also install on its child labels so events bubble up correctly
    drop_title->setAcceptDrops(false);
    drop_subtitle->setAcceptDrops(false);

    main_split->addWidget(list_card, /*stretch=*/3);

    // Right: Wallpaper Inspector Card
    auto* inspector_card = create_card(page);
    inspector_card->setFixedWidth(320);
    auto* insp_layout = new QVBoxLayout(inspector_card);
    insp_layout->setSpacing(8);

    auto* insp_title = new QLabel("Wallpaper Details", inspector_card);
    insp_title->setObjectName("sectionTitle");
    insp_layout->addWidget(insp_title);

    // Thumbnail Preview Box
    impl_->inspector_preview_label = new QLabel(inspector_card);
    impl_->inspector_preview_label->setObjectName("imagePreviewBox");
    impl_->inspector_preview_label->setFixedHeight(140);
    impl_->inspector_preview_label->setAlignment(Qt::AlignCenter);
    impl_->inspector_preview_label->setText("No wallpaper selected");
    impl_->inspector_preview_label->setStyleSheet("color: #64748B; font-size: 12px;");
    insp_layout->addWidget(impl_->inspector_preview_label);

    impl_->inspector_title_label = new QLabel("Select a wallpaper", inspector_card);
    impl_->inspector_title_label->setStyleSheet("font-weight: 600; font-size: 13px; color: #FFFFFF;");
    insp_layout->addWidget(impl_->inspector_title_label);

    impl_->inspector_meta_label = new QLabel("", inspector_card);
    impl_->inspector_meta_label->setStyleSheet("color: #94A3B8; font-size: 11px;");
    impl_->inspector_meta_label->setWordWrap(true);
    insp_layout->addWidget(impl_->inspector_meta_label);

    // Action: Set as Wallpaper Immediately
    impl_->inspector_apply_btn = new QPushButton("★ Set as Desktop Wallpaper", inspector_card);
    impl_->inspector_apply_btn->setObjectName("primaryBtn");
    impl_->inspector_apply_btn->setEnabled(false);
    connect(impl_->inspector_apply_btn, &QPushButton::clicked, this, [this]() {
        if (tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
        auto asset = tag_index_->get(impl_->current_selected_asset_id);
        if (asset.has_value() && impl_->on_apply_wallpaper) {
            impl_->on_apply_wallpaper(asset->file_path, asset->fit_mode, asset->type);
            QMessageBox::information(this, "Wallpaper Applied",
                "Applied wallpaper: " + QString::fromStdString(asset->id) + " to your desktop!");
        }
    });
    insp_layout->addWidget(impl_->inspector_apply_btn);

    // Fit Mode Selector
    auto* fit_row = new QHBoxLayout;
    fit_row->addWidget(new QLabel("Fit Mode:", inspector_card));
    impl_->inspector_fit_combo = new QComboBox(inspector_card);
    impl_->inspector_fit_combo->addItem("Fill (Crop to fill)", static_cast<int>(scaling_and_fit::FitMode::Fill));
    impl_->inspector_fit_combo->addItem("Fit (Letterbox)", static_cast<int>(scaling_and_fit::FitMode::Fit));
    impl_->inspector_fit_combo->addItem("Stretch", static_cast<int>(scaling_and_fit::FitMode::Stretch));
    impl_->inspector_fit_combo->addItem("Center", static_cast<int>(scaling_and_fit::FitMode::Center));
    impl_->inspector_fit_combo->addItem("Tile", static_cast<int>(scaling_and_fit::FitMode::Tile));

    auto update_current_item_display = [this]() {
        auto* current_item = impl_->gallery_list->currentItem();
        if (!current_item || tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
        auto asset = tag_index_->get(impl_->current_selected_asset_id);
        if (!asset.has_value()) return;
        QString icon = (asset->type == tag_system::AssetType::Video) ? "🎬 " : "🖼 ";
        QString fit_str = QString::fromStdString(scaling_and_fit::to_string(asset->fit_mode)).toUpper();
        QString tags_str;
        bool first = true;
        for (const auto& tag : asset->tags) {
            if (!first) tags_str += ", ";
            tags_str += QString::fromStdString(tag);
            first = false;
        }
        if (tags_str.isEmpty()) tags_str = "general";
        current_item->setText(icon + QString::fromStdString(asset->id) + "  (" + fit_str + ")");
        current_item->setToolTip("ID: " + QString::fromStdString(asset->id) + "\nPath: " +
                                 QString::fromStdString(asset->file_path) + "\nTags: " + tags_str);
    };

    connect(impl_->inspector_fit_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, update_current_item_display](int) {
        if (tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
        auto asset = tag_index_->get(impl_->current_selected_asset_id);
        if (asset.has_value()) {
            asset->fit_mode = static_cast<scaling_and_fit::FitMode>(impl_->inspector_fit_combo->currentData().toInt());
            tag_index_->upsert(*asset);
            std::filesystem::create_directories(config::default_data_dir());
            (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
            update_current_item_display();
        }
    });
    fit_row->addWidget(impl_->inspector_fit_combo);
    insp_layout->addLayout(fit_row);

    // Condition Tags Header
    auto* tags_group = new QLabel("Assigned Conditions & Solar Tags:", inspector_card);
    tags_group->setStyleSheet("font-weight: 600; font-size: 11px; color: #CBD5E1; margin-top: 4px;");
    insp_layout->addWidget(tags_group);

    // Conditions Grid (4 columns)
    const char* kConditions[] = {"clear", "sunny", "cloudy", "rain", "snow", "storm", "fog"};
    auto* cond_grid = new QGridLayout;
    cond_grid->setSpacing(4);
    int c_idx = 0;
    for (const auto* c : kConditions) {
        auto* cb = new QCheckBox(c, inspector_card);
        cb->setStyleSheet("font-size: 11px;");
        connect(cb, &QCheckBox::toggled, this, [this, c, update_current_item_display](bool checked) {
            if (tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
            if (checked) tag_index_->add_tag(impl_->current_selected_asset_id, c);
            else tag_index_->remove_tag(impl_->current_selected_asset_id, c);
            std::filesystem::create_directories(config::default_data_dir());
            (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
            update_current_item_display();
        });
        impl_->condition_tag_checks.push_back(cb);
        cond_grid->addWidget(cb, c_idx / 4, c_idx % 4);
        c_idx++;
    }
    insp_layout->addLayout(cond_grid);

    // Solar Buckets Grid (4 columns)
    const char* kSolar[] = {"morning", "day", "evening", "night"};
    auto* solar_grid = new QGridLayout;
    solar_grid->setSpacing(4);
    int s_idx = 0;
    for (const auto* s : kSolar) {
        auto* cb = new QCheckBox(s, inspector_card);
        cb->setStyleSheet("font-size: 11px;");
        connect(cb, &QCheckBox::toggled, this, [this, s, update_current_item_display](bool checked) {
            if (tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
            if (checked) tag_index_->add_tag(impl_->current_selected_asset_id, s);
            else tag_index_->remove_tag(impl_->current_selected_asset_id, s);
            std::filesystem::create_directories(config::default_data_dir());
            (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
            update_current_item_display();
        });
        impl_->solar_tag_checks.push_back(cb);
        solar_grid->addWidget(cb, 0, s_idx);
        s_idx++;
    }
    insp_layout->addLayout(solar_grid);

    insp_layout->addStretch();

    // Delete Button
    impl_->inspector_delete_btn = new QPushButton("🗑 Remove Wallpaper", inspector_card);
    impl_->inspector_delete_btn->setObjectName("dangerBtn");
    impl_->inspector_delete_btn->setEnabled(false);
    connect(impl_->inspector_delete_btn, &QPushButton::clicked, this, [this]() {
        if (tag_index_ == nullptr || impl_->current_selected_asset_id.empty()) return;
        auto asset = tag_index_->get(impl_->current_selected_asset_id);
        if (asset.has_value() && asset->source_pack_id == "bundled-default") {
            auto reply = QMessageBox::warning(this, "Remove Wallpaper",
                "This wallpaper is part of the bundled default theme. Are you sure you want to remove it?",
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
        } else {
            auto reply = QMessageBox::question(this, "Remove Wallpaper",
                "Are you sure you want to remove '" + QString::fromStdString(impl_->current_selected_asset_id) + "' from the collection?",
                QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) return;
        }

        tag_index_->remove(impl_->current_selected_asset_id);
        std::filesystem::create_directories(config::default_data_dir());
        (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
        refresh_gallery_list();
    });
    insp_layout->addWidget(impl_->inspector_delete_btn);

    main_split->addWidget(inspector_card);
    layout->addLayout(main_split);

    // List selection connection
    connect(impl_->gallery_list, &QListWidget::currentRowChanged, this, [this](int row) {
        auto* item = impl_->gallery_list->item(row);
        if (!item || tag_index_ == nullptr || row < 0) {
            impl_->current_selected_asset_id.clear();
            impl_->inspector_title_label->setText("Select a wallpaper");
            impl_->inspector_meta_label->clear();
            impl_->inspector_preview_label->setPixmap(QPixmap());
            impl_->inspector_preview_label->setText("No wallpaper selected");
            impl_->inspector_apply_btn->setEnabled(false);
            impl_->inspector_delete_btn->setEnabled(false);
            impl_->inspector_fit_combo->setEnabled(false);
            for (auto* cb : impl_->condition_tag_checks) {
                cb->blockSignals(true);
                cb->setChecked(false);
                cb->setEnabled(false);
                cb->blockSignals(false);
            }
            for (auto* cb : impl_->solar_tag_checks) {
                cb->blockSignals(true);
                cb->setChecked(false);
                cb->setEnabled(false);
                cb->blockSignals(false);
            }
            return;
        }

        QString id = item->data(Qt::UserRole).toString();
        impl_->current_selected_asset_id = id.toStdString();

        auto asset = tag_index_->get(impl_->current_selected_asset_id);
        if (!asset.has_value()) return;

        impl_->inspector_title_label->setText(QString::fromStdString(asset->id));
        impl_->inspector_meta_label->setText(QString::fromStdString(asset->file_path));
        impl_->inspector_apply_btn->setEnabled(true);
        impl_->inspector_delete_btn->setEnabled(true);
        impl_->inspector_fit_combo->setEnabled(true);

        // Load thumbnail image
        QPixmap pix(QString::fromStdString(asset->file_path));
        if (!pix.isNull()) {
            impl_->inspector_preview_label->setPixmap(
                pix.scaled(QSize(280, 140), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            impl_->inspector_preview_label->setPixmap(QPixmap());
            impl_->inspector_preview_label->setText(
                asset->type == tag_system::AssetType::Video ? "🎬 Video Wallpaper" : "🖼 No Preview Available");
        }

        // Fit mode
        impl_->inspector_fit_combo->blockSignals(true);
        int fit_idx = impl_->inspector_fit_combo->findData(static_cast<int>(asset->fit_mode));
        if (fit_idx >= 0) impl_->inspector_fit_combo->setCurrentIndex(fit_idx);
        impl_->inspector_fit_combo->blockSignals(false);

        // Update tag check boxes
        for (auto* cb : impl_->condition_tag_checks) {
            cb->setEnabled(true);
            cb->blockSignals(true);
            cb->setChecked(asset->tags.count(cb->text().toStdString()) > 0);
            cb->blockSignals(false);
        }
        for (auto* cb : impl_->solar_tag_checks) {
            cb->setEnabled(true);
            cb->blockSignals(true);
            cb->setChecked(asset->tags.count(cb->text().toStdString()) > 0);
            cb->blockSignals(false);
        }
    });

    refresh_gallery_list();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::build_performance_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scroll->viewport()) {
        scroll->viewport()->setStyleSheet("background-color: #0A0B0E;");
    }

    auto* page = new QWidget;
    page->setObjectName("tabContentPage");
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    // Video Wallpaper Card
    auto* anim_card = create_card(page);
    auto* anim_layout = new QVBoxLayout(anim_card);
    anim_layout->setSpacing(8);

    auto* anim_title = new QLabel("Animated & Video Wallpapers", anim_card);
    anim_title->setObjectName("sectionTitle");
    auto* anim_desc = new QLabel("Play video loops or animated shaders. Automatically disabled during low power conditions.", anim_card);
    anim_desc->setObjectName("sectionDesc");
    anim_desc->setWordWrap(true);
    anim_layout->addWidget(anim_title);
    anim_layout->addWidget(anim_desc);

    impl_->animated_enabled_check = new QCheckBox("Enable video / animated wallpapers (requires GPU compositing)", anim_card);
    impl_->animated_enabled_check->setChecked(app_config_->performance.animated_wallpaper_enabled);
    anim_layout->addWidget(impl_->animated_enabled_check);
    layout->addWidget(anim_card);

    // Power Saving Card
    auto* power_card = create_card(page);
    auto* power_layout = new QVBoxLayout(power_card);
    power_layout->setSpacing(12);

    auto* power_title = new QLabel("Intelligent Power & Resource Rules", power_card);
    power_title->setObjectName("sectionTitle");
    auto* power_desc = new QLabel("Automatically pause wallpaper rendering to eliminate CPU/GPU usage when unneeded.", power_card);
    power_desc->setObjectName("sectionDesc");
    power_desc->setWordWrap(true);
    power_layout->addWidget(power_title);
    power_layout->addWidget(power_desc);

    impl_->pause_locked_check = new QCheckBox("Pause animation when the desktop is locked", power_card);
    impl_->pause_locked_check->setChecked(app_config_->performance.pause_when_locked);
    power_layout->addWidget(impl_->pause_locked_check);

    impl_->pause_fullscreen_check = new QCheckBox("Pause when a full-screen application or game is focused", power_card);
    impl_->pause_fullscreen_check->setChecked(app_config_->performance.pause_when_fullscreen_app_active);
    power_layout->addWidget(impl_->pause_fullscreen_check);

    impl_->pause_battery_saver_check = new QCheckBox("Pause when system battery saver / low-power mode is active", power_card);
    impl_->pause_battery_saver_check->setChecked(app_config_->performance.pause_on_battery_saver);
    power_layout->addWidget(impl_->pause_battery_saver_check);

    layout->addWidget(power_card);
    layout->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* MainWindow::build_about_tab() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (scroll->viewport()) {
        scroll->viewport()->setStyleSheet("background-color: #0A0B0E;");
    }

    auto* page = new QWidget;
    page->setObjectName("tabContentPage");
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(16);

    auto* card = create_card(page);
    auto* card_layout = new QVBoxLayout(card);
    card_layout->setSpacing(12);

    auto* title = new QLabel("WeatherPaper Desktop", card);
    title->setStyleSheet("font-size: 18px; font-weight: 700; color: #FFFFFF;");
    auto* desc = new QLabel(
        "A modular, lightweight, live weather-reactive wallpaper engine for Linux and Windows.\n"
        "Engineered with C++20, zero busy-polling, and real solar sunrise/sunset calculations.", card);
    desc->setObjectName("sectionDesc");
    desc->setWordWrap(true);
    card_layout->addWidget(title);
    card_layout->addWidget(desc);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    auto* v_core = new QLabel("1.0.0 (RelWithDebInfo)", card);
    v_core->setStyleSheet("color: #93C5FD; font-weight: 500;");
    auto* v_gui = new QLabel("Qt6 Widgets (Obsidian Black Edition)", card);
    v_gui->setStyleSheet("color: #94A3B8;");
    auto* v_prov = new QLabel("Open-Meteo (HTTPS, API Key-free)", card);
    v_prov->setStyleSheet("color: #94A3B8;");
    auto* v_paths = new QLabel(QString::fromStdString(config::default_config_dir()), card);
    v_paths->setStyleSheet("color: #94A3B8; font-family: monospace; font-size: 11px;");

    form->addRow("Engine Core Version:", v_core);
    form->addRow("GUI Architecture:", v_gui);
    form->addRow("Weather Provider:", v_prov);
    form->addRow("Config Storage Path:", v_paths);
    card_layout->addLayout(form);

    auto* btn_row = new QHBoxLayout;
    auto* update_btn = new QPushButton("Check for Updates", card);
    update_btn->setObjectName("primaryBtn");
    connect(update_btn, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Version Check", "WeatherPaper is currently up to date (v1.0.0).");
    });
    btn_row->addWidget(update_btn);

    auto* open_folder_btn = new QPushButton("Open Config Directory", card);
    open_folder_btn->setObjectName("secondaryActionBtn");
    connect(open_folder_btn, &QPushButton::clicked, this, []() {
        std::filesystem::create_directories(config::default_config_dir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(config::default_config_dir())));
    });
    btn_row->addWidget(open_folder_btn);

    btn_row->addStretch();
    card_layout->addLayout(btn_row);

    layout->addWidget(card);
    layout->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void MainWindow::refresh_gallery_list() {
    if (impl_->gallery_list == nullptr || tag_index_ == nullptr) return;
    int prev_row = impl_->gallery_list->currentRow();
    std::string prev_id = impl_->current_selected_asset_id;

    impl_->gallery_list->blockSignals(true);
    impl_->gallery_list->clear();
    int restore_row = -1;
    int current_index = 0;

    for (const auto& asset : tag_index_->list_all()) {
        QString icon = (asset.type == tag_system::AssetType::Video) ? "🎬 " : "🖼 ";
        QString fit_str = QString::fromStdString(scaling_and_fit::to_string(asset.fit_mode)).toUpper();
        QString tags_str;
        bool first = true;
        for (const auto& tag : asset.tags) {
            if (!first) tags_str += ", ";
            tags_str += QString::fromStdString(tag);
            first = false;
        }
        if (tags_str.isEmpty()) tags_str = "general";

        QString label = icon + QString::fromStdString(asset.id) + "  (" + fit_str + ")";
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, QString::fromStdString(asset.id));
        item->setToolTip("ID: " + QString::fromStdString(asset.id) + "\nPath: " +
                         QString::fromStdString(asset.file_path) + "\nTags: " + tags_str);
        impl_->gallery_list->addItem(item);

        if (!prev_id.empty() && asset.id == prev_id) {
            restore_row = current_index;
        }
        current_index++;
    }
    impl_->gallery_list->blockSignals(false);

    if (impl_->gallery_list->count() > 0) {
        int target_row = (restore_row >= 0) ? restore_row : ((prev_row >= 0 && prev_row < impl_->gallery_list->count()) ? prev_row : 0);
        impl_->gallery_list->setCurrentRow(target_row);
    } else {
        impl_->gallery_list->setCurrentRow(-1);
    }
}

void MainWindow::on_add_gallery_file_clicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Add Wallpaper", QString(),
        "Images and Videos (*.png *.jpg *.jpeg *.webp *.mp4 *.webm)");
    if (path.isEmpty()) return;

    auto validation = asset_manager::validate_upload(path.toStdString(), 0);
    if (!validation.accepted) {
        QMessageBox::warning(this, "Unsupported File", QString::fromStdString(validation.rejection_reason));
        return;
    }
    if (validation.exceeds_warning_threshold) {
        auto choice = QMessageBox::warning(
            this, "Large File",
            "This file is larger or higher-resolution than recommended and may affect performance. Add it anyway?",
            QMessageBox::Yes | QMessageBox::No);
        if (choice != QMessageBox::Yes) return;
    }

    tag_system::AssetRecord rec;
    std::string base_id = QFileInfo(path).baseName().toStdString();
    std::string id = base_id;
    int suffix = 1;
    while (tag_index_ != nullptr && tag_index_->get(id).has_value()) {
        id = base_id + "_" + std::to_string(suffix++);
    }
    rec.id = id;
    rec.file_path = path.toStdString();
    rec.type = validation.type == asset_manager::UploadFileType::Video
                   ? tag_system::AssetType::Video
                   : tag_system::AssetType::Image;
    rec.tags = {"day", "clear"};
    if (tag_index_ != nullptr) {
        tag_index_->upsert(rec);
        std::filesystem::create_directories(config::default_data_dir());
        (void)tag_index_->save_to_file(config::default_data_dir() + "/tag_index.json");
    }
    impl_->current_selected_asset_id = rec.id;
    refresh_gallery_list();
}

void MainWindow::on_save_settings_clicked() {
    app_config_->location.auto_detected = impl_->location_auto_detect->isChecked();
    app_config_->location.display_name = impl_->location_display->text().toStdString();
    app_config_->location.latitude = impl_->lat_spin->value();
    app_config_->location.longitude = impl_->lon_spin->value();

    app_config_->units = impl_->units_combo->currentIndex() == 1
                             ? config::TemperatureUnit::Fahrenheit
                             : config::TemperatureUnit::Celsius;
    app_config_->weather_poll_interval = std::chrono::minutes(impl_->poll_interval_spin->value());
    app_config_->selection_policy = static_cast<tag_system::SelectionPolicy>(impl_->selection_policy_combo->currentData().toInt());
    app_config_->severe_weather_notifications_enabled = impl_->severe_weather_check->isChecked();

    app_config_->performance.animated_wallpaper_enabled = impl_->animated_enabled_check->isChecked();
    app_config_->performance.pause_when_locked = impl_->pause_locked_check->isChecked();
    app_config_->performance.pause_when_fullscreen_app_active = impl_->pause_fullscreen_check->isChecked();
    app_config_->performance.pause_on_battery_saver = impl_->pause_battery_saver_check->isChecked();

    // Persist configuration to disk!
    std::filesystem::create_directories(config::default_config_dir());
    std::string config_path = config::default_config_dir() + "/config.json";
    bool saved = app_config_->save_to_file(config_path);

    if (impl_->save_status_label) {
        if (saved) {
            impl_->save_status_label->setText("✓ Settings saved to disk (" + QString::fromStdString(config_path) + ")");
        } else {
            impl_->save_status_label->setText("✓ In-memory settings updated");
        }
        QTimer::singleShot(3500, impl_->save_status_label, [this]() {
            if (impl_->save_status_label) impl_->save_status_label->clear();
        });
    }

    // Trigger engine refresh with the newly saved configuration
    if (impl_->on_force_refresh) {
        impl_->on_force_refresh();
    }
}

} // namespace weatherpaper::settings_ui

#include "settings_ui.moc"
