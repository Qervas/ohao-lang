#include "ThemeManager.h"
#include "ThemeColors.h"
#include "AppSettings.h"
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QDebug>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    qDebug() << "ThemeManager singleton created";
}

ThemeManager& ThemeManager::instance() {
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return *s_instance;
}

ThemeManager::Theme ThemeManager::fromString(const QString &name) {
    const QString n = name.toLower();
    if (n.contains("cyber")) return Theme::Cyberpunk;
    if (n.contains("dark")) return Theme::Dark;
    if (n.contains("light")) return Theme::Light;
    return Theme::Auto;
}

QString ThemeManager::toString(ThemeManager::Theme theme) {
    switch (theme) {
        case Theme::Cyberpunk: return "Cyberpunk";
        case Theme::Dark: return "Dark";
        case Theme::Light: return "Light";
        case Theme::Auto: default: return "Auto (System)";
    }
}

void ThemeManager::saveToSettings(Theme theme) {
    AppSettings::instance().setTheme(toString(theme));
    qDebug() << "Theme saved to centralized settings:" << toString(theme);
}

void ThemeManager::applyFromSettings() {
    const QString name = AppSettings::instance().theme();
    applyTheme(fromString(name));
    qDebug() << "Applied theme from centralized settings:" << name;
}

void ThemeManager::applyTheme(Theme theme) {
    Theme originalTheme = theme;
    if (theme == Theme::Auto) {
        auto hints = QGuiApplication::styleHints();
        const bool dark = hints ? hints->colorScheme() == Qt::ColorScheme::Dark : false;
        theme = dark ? Theme::Dark : Theme::Light;
    }

    // Store the resolved theme
    m_currentTheme = theme;

    qDebug() << "Applying theme:" << toString(originalTheme) << "-> resolved to:" << toString(theme);

    switch (theme) {
        case Theme::Light: applyLight(); break;
        case Theme::Dark: applyDark(); break;
        case Theme::Cyberpunk: applyCyberpunk(); break;
        case Theme::Auto: default: applyLight(); break;
    }

    // Store current palette and emit signal
    m_currentPalette = QApplication::palette();

    qDebug() << "ThemeManager: Emitting themeChanged signal for theme:" << toString(m_currentTheme);
    emit themeChanged(m_currentTheme);

    qDebug() << "Theme applied successfully. Current palette Window color:" << m_currentPalette.color(QPalette::Window).name();
}

static void setAppPaletteAndStyle(const QPalette &pal, const QString &styleSheet) {
    QApplication::setPalette(pal);
    qApp->setStyleSheet(styleSheet);
}

void ThemeManager::applyLight() {
    using namespace ThemeColors::Light;
    QPalette pal;
    pal.setColor(QPalette::Window, Window);
    pal.setColor(QPalette::WindowText, WindowText);
    pal.setColor(QPalette::Base, Base);
    pal.setColor(QPalette::AlternateBase, AlternateBase);
    pal.setColor(QPalette::ToolTipBase, ToolTipBase);
    pal.setColor(QPalette::ToolTipText, ToolTipText);
    pal.setColor(QPalette::Text, Text);
    pal.setColor(QPalette::Button, Button);
    pal.setColor(QPalette::ButtonText, ButtonText);
    pal.setColor(QPalette::Highlight, Highlight);
    pal.setColor(QPalette::HighlightedText, HighlightedText);

    // Build stylesheet using ThemeColors
    QString style = QString(R"(
        /* Floating widget - clean stylesheet-based design */
        #floatingWidget {
            background-color:rgba(%1,%2,%3,%4);
            border:1px solid rgba(%5,%6,%7,%8);
            border-radius:18px;
        }
        #floatingWidget[highlight="true"] {
            border:2px solid rgba(%9,%10,%11,%12);
        }
        #floatingWidget QPushButton#floatingButton {
            background:transparent;
            border:none;
            font-size:18px;
            font-weight:600;
            color:%13;
        }
        #floatingWidget QPushButton#floatingButton:hover {
            background:rgba(%14,%15,%16,%17);
            border-radius:12px;
        }
        #floatingWidget QPushButton#floatingButton:pressed {
            background:rgba(%18,%19,%20,%21);
        }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid %22; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid %23; }
        QGroupBox { padding-top:18px; margin-top:12px; border:1px solid rgba(%5,%6,%7,38); border-radius:8px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left:12px; top:4px; padding:2px 6px; background:transparent; font-weight:600; }
    )")
    .arg(FloatingWidgetBg.red()).arg(FloatingWidgetBg.green()).arg(FloatingWidgetBg.blue()).arg(FloatingWidgetBg.alpha()) // 1-4
    .arg(FloatingWidgetBorder.red()).arg(FloatingWidgetBorder.green()).arg(FloatingWidgetBorder.blue()).arg(FloatingWidgetBorder.alpha()) // 5-8
    .arg(FloatingWidgetHighlight.red()).arg(FloatingWidgetHighlight.green()).arg(FloatingWidgetHighlight.blue()).arg(FloatingWidgetHighlight.alpha()) // 9-12
    .arg(WindowText.name())                      // 13
    .arg(ButtonHover.red()).arg(ButtonHover.green()).arg(ButtonHover.blue()).arg(ButtonHover.alpha()) // 14-17
    .arg(ButtonPressed.red()).arg(ButtonPressed.green()).arg(ButtonPressed.blue()).arg(ButtonPressed.alpha()) // 18-21
    .arg(Success.name())                         // 22
    .arg(Error.name());                          // 23

    setAppPaletteAndStyle(pal, style);
}

void ThemeManager::applyDark() {
    using namespace ThemeColors::Dark;
    QPalette pal;
    pal.setColor(QPalette::Window, Window);
    pal.setColor(QPalette::WindowText, WindowText);
    pal.setColor(QPalette::Base, Base);
    pal.setColor(QPalette::AlternateBase, AlternateBase);
    pal.setColor(QPalette::ToolTipBase, ToolTipBase);
    pal.setColor(QPalette::ToolTipText, ToolTipText);
    pal.setColor(QPalette::Text, Text);
    pal.setColor(QPalette::Button, Button);
    pal.setColor(QPalette::ButtonText, ButtonText);
    pal.setColor(QPalette::Highlight, Highlight);
    pal.setColor(QPalette::HighlightedText, HighlightedText);

    // Build stylesheet using ThemeColors
    QString style = QString(R"(
        QWidget { color: %1; background-color: %2; }
        QGroupBox { border: 1px solid %3; border-radius: 6px; margin-top: 10px; }
        QGroupBox { padding-top:18px; margin-top:12px; }
        QGroupBox::title { color: %1; subcontrol-origin: margin; subcontrol-position: top left; left:12px; top:4px; padding:2px 6px; background:%2; }
        QPushButton { background-color: %3; border: 1px solid %4; border-radius: 6px; padding: 6px 12px; }
        QPushButton:hover { border-color: %5; }
        QPushButton#applyBtn { background-color: %5; color: white; border: 1px solid %5; }
        QTabWidget::pane { border: 1px solid %3; }
        QTabBar::tab { background-color: %3; padding: 8px 16px; }
        QTabBar::tab:selected { background-color: %6; }
        QLineEdit, QTextEdit, QComboBox, QSpinBox { background-color: %7; border: 1px solid %4; border-radius: 6px; }
        QSlider::groove:horizontal { height: 6px; background: %3; border-radius: 3px; }
        QSlider::handle:horizontal { background: %5; width: 18px; border-radius: 9px; margin: -6px 0; }
        /* Selection toolbar */
        #selectionToolbar { background: rgba(%8,%9,%10,%11); border:1px solid %4; border-radius:24px; }
        #selectionToolbar QPushButton#toolbarButton { background: transparent; min-width:38px; min-height:38px; font-size:16px; font-weight:600; }
        #selectionToolbar QPushButton#toolbarButton:hover { background:%6; }
        #selectionToolbar QPushButton#toolbarButton:pressed { background:%7; }
        /* OCR Result window placeholders */
        #ocrImagePlaceholder[placeholder="true"] { border:2px dashed %4; padding:20px; color:%1; }
        QLabel[placeholder="true"] { color:%1; }
        QTextEdit[hint="true"] { color:%1; font-style:italic; }
        QTextEdit[error="true"] { color:%13; font-style:italic; }
        QLabel[status="success"], QLabel[status="error"], QLabel[status="progress"] { font-weight:600; }
        QLabel[status="success"] { color:%12; }
        QLabel[status="error"] { color:%13; }
        QLabel[status="progress"] { color:%5; }
        /* Floating widget - clean stylesheet-based design */
        #floatingWidget {
            background-color:rgba(%14,%15,%16,%17);
            border:1px solid rgba(%18,%19,%20,%21);
            border-radius:18px;
        }
        #floatingWidget[highlight="true"] {
            border-color:%5;
        }
        #floatingWidget QPushButton#floatingButton {
            background:transparent;
            border:none;
            font-size:18px;
            font-weight:600;
            color:%1;
        }
        #floatingWidget QPushButton#floatingButton:hover {
            background:rgba(%22,%23,%24,%25);
            border-radius:12px;
        }
        #floatingWidget QPushButton#floatingButton:pressed {
            background:rgba(%26,%27,%28,%29);
        }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid %12; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid %13; }
    )")
    .arg(WindowText.name())                      // 1
    .arg(Window.name())                          // 2
    .arg(Button.name())                          // 3
    .arg(FloatingWidgetBorder.name())            // 4
    .arg(Highlight.name())                       // 5
    .arg(AlternateBase.name())                   // 6
    .arg(Base.name())                            // 7
    .arg(ScreenshotToolbarBg.red()).arg(ScreenshotToolbarBg.green()).arg(ScreenshotToolbarBg.blue()).arg(ScreenshotToolbarBg.alpha()) // 8-11
    .arg(Success.name())                         // 12
    .arg(Error.name())                           // 13
    .arg(FloatingWidgetBg.red()).arg(FloatingWidgetBg.green()).arg(FloatingWidgetBg.blue()).arg(FloatingWidgetBg.alpha()) // 14-17
    .arg(FloatingWidgetBorder.red()).arg(FloatingWidgetBorder.green()).arg(FloatingWidgetBorder.blue()).arg(FloatingWidgetBorder.alpha()) // 18-21
    .arg(ButtonHover.red()).arg(ButtonHover.green()).arg(ButtonHover.blue()).arg(ButtonHover.alpha()) // 22-25
    .arg(ButtonPressed.red()).arg(ButtonPressed.green()).arg(ButtonPressed.blue()).arg(ButtonPressed.alpha()); // 26-29

    setAppPaletteAndStyle(pal, style);
}


void ThemeManager::applyCyberpunk() {
    using namespace ThemeColors::Cyberpunk;
    QPalette pal;
    pal.setColor(QPalette::Window, Window);
    pal.setColor(QPalette::WindowText, WindowText);
    pal.setColor(QPalette::Base, Base);
    pal.setColor(QPalette::AlternateBase, AlternateBase);
    pal.setColor(QPalette::ToolTipBase, ToolTipBase);
    pal.setColor(QPalette::ToolTipText, ToolTipText);
    pal.setColor(QPalette::Text, Text);
    pal.setColor(QPalette::Button, Button);
    pal.setColor(QPalette::ButtonText, ButtonText);
    pal.setColor(QPalette::Highlight, Highlight);
    pal.setColor(QPalette::HighlightedText, HighlightedText);

    // Build stylesheet using ThemeColors
    QString style = QString(R"(
        /* Neon cyberpunk: teal + magenta accents */
        QWidget { background-color: %1; color: %2; }
        QDialog, QMainWindow { background-color: %1; }
        QGroupBox { border: 1px solid %3; border-radius: 8px; margin-top: 10px; }
        QGroupBox { padding-top:20px; margin-top:14px; }
        QGroupBox::title { padding:2px 8px; color: %4; left:12px; top:4px; subcontrol-origin: margin; subcontrol-position: top left; background:%1; font-weight:600; }
        QPushButton { background: %5; color: %6; border: 1px solid %7; border-radius: 10px; padding: 8px 14px; }
        QPushButton:hover { border-color: %4; }
        QPushButton#applyBtn { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %4, stop:1 %8); color: %1; border: none; }
        QTabWidget::pane { border: 1px solid %7; border-radius: 8px; }
        QTabBar::tab { background: %5; padding: 10px 18px; color: %2; border: 1px solid %3; border-bottom: none; }
        QTabBar::tab:selected { background: %9; color: %6; border-color: %7; }
        QLineEdit, QTextEdit, QComboBox, QSpinBox { background: %5; color: %2; border: 1px solid %3; border-radius: 8px; }
        QCheckBox { color: %2; }
        QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid %8; border-radius: 4px; background: %5; }
        QCheckBox::indicator:checked { background: %4; border-color: %4; }
        QSlider::groove:horizontal { height: 6px; background: %9; border-radius: 3px; }
        QSlider::handle:horizontal { background: %8; width: 18px; border-radius: 9px; margin: -6px 0; border: 1px solid %4; }
        QScrollBar::handle:vertical { background: %9; border: 1px solid %3; border-radius: 5px; }
        QLabel { color: %2; }
        #selectionToolbar { background: rgba(%10,%11,%12,%13); border:1px solid %7; border-radius:24px; }
        #selectionToolbar QPushButton#toolbarButton { background: transparent; min-width:38px; min-height:38px; font-size:16px; font-weight:600; }
        #selectionToolbar QPushButton#toolbarButton:hover { background: rgba(%14,%15,%16,%17); }
        #selectionToolbar QPushButton#toolbarButton:pressed { background: rgba(%18,%19,%20,%21); }
        #ocrImagePlaceholder[placeholder="true"] { border:2px dashed %3; padding:20px; color:%4; }
        QTextEdit[hint="true"] { color:%2; font-style:italic; }
        QTextEdit[error="true"] { color:%22; font-style:italic; }
        QLabel[status="success"], QLabel[status="error"], QLabel[status="progress"] { font-weight:600; }
        QLabel[status="success"] { color:%23; }
        QLabel[status="error"] { color:%22; }
        QLabel[status="progress"] { color:%8; }
        /* Floating widget - clean stylesheet-based design */
        #floatingWidget {
            background-color:rgba(%24,%25,%26,%27);
            border:1px solid rgba(%28,%29,%30,%31);
            border-radius:18px;
        }
        #floatingWidget[highlight="true"] {
            border-color:%4;
        }
        #floatingWidget QPushButton#floatingButton {
            background:transparent;
            border:none;
            font-size:20px;
            font-weight:600;
            color:%4;
        }
        #floatingWidget QPushButton#floatingButton:hover {
            background:rgba(%32,%33,%34,%35);
            border-radius:12px;
        }
        #floatingWidget QPushButton#floatingButton:pressed {
            background:rgba(%36,%37,%38,%39);
        }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid %23; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid %22; }
    )")
    .arg(Window.name())                          // 1
    .arg(Text.name())                            // 2
    .arg(FloatingWidgetBorder.name())            // 3
    .arg(Teal.name())                            // 4 - Cyberpunk teal accent
    .arg(Button.name())                          // 5
    .arg(WindowText.name())                      // 6
    .arg(Blue.name())                            // 7 - Cyberpunk blue accent
    .arg(Blue.name())                            // 8 - Cyberpunk blue
    .arg(AlternateBase.name())                   // 9
    .arg(ScreenshotToolbarBg.red()).arg(ScreenshotToolbarBg.green()).arg(ScreenshotToolbarBg.blue()).arg(ScreenshotToolbarBg.alpha()) // 10-13
    .arg(ButtonHover.red()).arg(ButtonHover.green()).arg(ButtonHover.blue()).arg(ButtonHover.alpha()) // 14-17
    .arg(ButtonPressed.red()).arg(ButtonPressed.green()).arg(ButtonPressed.blue()).arg(ButtonPressed.alpha()) // 18-21
    .arg(Error.name())                           // 22
    .arg(Success.name())                         // 23
    .arg(FloatingWidgetBg.red()).arg(FloatingWidgetBg.green()).arg(FloatingWidgetBg.blue()).arg(FloatingWidgetBg.alpha()) // 24-27
    .arg(FloatingWidgetBorder.red()).arg(FloatingWidgetBorder.green()).arg(FloatingWidgetBorder.blue()).arg(FloatingWidgetBorder.alpha()) // 28-31
    .arg(ButtonHover.red()).arg(ButtonHover.green()).arg(ButtonHover.blue()).arg(ButtonHover.alpha()) // 32-35
    .arg(ButtonPressed.red()).arg(ButtonPressed.green()).arg(ButtonPressed.blue()).arg(ButtonPressed.alpha()); // 36-39

    setAppPaletteAndStyle(pal, style);
}

QPalette ThemeManager::getCurrentPalette() const {
    return m_currentPalette;
}

ThemeManager::Theme ThemeManager::getCurrentTheme() const {
    return m_currentTheme;
}
