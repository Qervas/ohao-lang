#include "ThemeManager.h"
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {}

ThemeManager::Theme ThemeManager::fromString(const QString &name) {
    const QString n = name.toLower();
    if (n.contains("cyber")) return Theme::Cyberpunk;
    if (n.contains("dark")) return Theme::Dark;
    if (n.contains("high")) return Theme::HighContrast;
    if (n.contains("light")) return Theme::Light;
    return Theme::Auto;
}

QString ThemeManager::toString(ThemeManager::Theme theme) {
    switch (theme) {
        case Theme::Cyberpunk: return "Cyberpunk";
        case Theme::Dark: return "Dark";
        case Theme::HighContrast: return "High Contrast";
        case Theme::Light: return "Light";
        case Theme::Auto: default: return "Auto (System)";
    }
}

void ThemeManager::saveToSettings(Theme theme) {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("appearance/theme", toString(theme));
}

void ThemeManager::applyFromSettings() {
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QString name = settings.value("appearance/theme", "Auto (System)").toString();
    applyTheme(fromString(name));
}

void ThemeManager::applyTheme(Theme theme) {
    if (theme == Theme::Auto) {
        auto hints = QGuiApplication::styleHints();
        const bool dark = hints ? hints->colorScheme() == Qt::ColorScheme::Dark : false;
        theme = dark ? Theme::Dark : Theme::Light;
    }

    switch (theme) {
        case Theme::Light: applyLight(); break;
        case Theme::Dark: applyDark(); break;
        case Theme::HighContrast: applyHighContrast(); break;
        case Theme::Cyberpunk: applyCyberpunk(); break;
        case Theme::Auto: default: applyLight(); break;
    }
}

static void setAppPaletteAndStyle(const QPalette &pal, const QString &styleSheet) {
    QApplication::setPalette(pal);
    qApp->setStyleSheet(styleSheet);
}

void ThemeManager::applyLight() {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(250, 252, 255));
    pal.setColor(QPalette::WindowText, QColor(45, 55, 72));
    pal.setColor(QPalette::Base, QColor(255, 255, 255));
    pal.setColor(QPalette::AlternateBase, QColor(245, 247, 250));
    pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    pal.setColor(QPalette::ToolTipText, QColor(45, 55, 72));
    pal.setColor(QPalette::Text, QColor(45, 55, 72));
    pal.setColor(QPalette::Button, QColor(245, 247, 250));
    pal.setColor(QPalette::ButtonText, QColor(45, 55, 72));
    pal.setColor(QPalette::Highlight, QColor(100, 150, 250));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    const QString style = R"(
        /* Floating widget */
        #floatingWidget { background:rgba(255,255,255,205); border:1px solid rgba(190,195,205,0.65); border-radius:18px; }
        #floatingWidget[highlight="true"] { box-shadow: none; }
        #floatingWidget QPushButton#floatingButton { background:transparent; border:none; font-size:18px; font-weight:600; color:#4A5568; }
        #floatingWidget QPushButton#floatingButton:hover { background:rgba(0,0,0,0.08); border-radius:12px; }
        #floatingWidget QPushButton#floatingButton:pressed { background:rgba(0,0,0,0.15); }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid #28a745; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid #dc3545; }
        QGroupBox { padding-top:18px; margin-top:12px; border:1px solid rgba(0,0,0,0.15); border-radius:8px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left:12px; top:4px; padding:2px 6px; background:transparent; font-weight:600; }
    )";
    setAppPaletteAndStyle(pal, style);
}

void ThemeManager::applyDark() {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(28, 32, 38));
    pal.setColor(QPalette::WindowText, QColor(230, 233, 239));
    pal.setColor(QPalette::Base, QColor(22, 26, 31));
    pal.setColor(QPalette::AlternateBase, QColor(32, 36, 43));
    pal.setColor(QPalette::ToolTipBase, QColor(60, 60, 70));
    pal.setColor(QPalette::ToolTipText, QColor(240, 240, 240));
    pal.setColor(QPalette::Text, QColor(230, 233, 239));
    pal.setColor(QPalette::Button, QColor(38, 43, 50));
    pal.setColor(QPalette::ButtonText, QColor(230, 233, 239));
    pal.setColor(QPalette::Highlight, QColor(80, 130, 230));
    pal.setColor(QPalette::HighlightedText, Qt::white);

    const QString style = R"( 
        QWidget { color: #E6E9EF; background-color: #1C2026; }
        QGroupBox { border: 1px solid #2A2F37; border-radius: 6px; margin-top: 10px; }
        QGroupBox { padding-top:18px; margin-top:12px; }
        QGroupBox::title { color: #C7CDD7; subcontrol-origin: margin; subcontrol-position: top left; left:12px; top:4px; padding:2px 6px; background:#1C2026; }
        QPushButton { background-color: #2A2F37; border: 1px solid #3A404B; border-radius: 6px; padding: 6px 12px; }
        QPushButton:hover { border-color: #5082E6; }
        QPushButton#applyBtn { background-color: #5082E6; color: white; border: 1px solid #3C6DD9; }
        QTabWidget::pane { border: 1px solid #2A2F37; }
        QTabBar::tab { background-color: #2A2F37; padding: 8px 16px; }
        QTabBar::tab:selected { background-color: #303642; }
        QLineEdit, QTextEdit, QComboBox, QSpinBox { background-color: #22262C; border: 1px solid #3A404B; border-radius: 6px; }
        QSlider::groove:horizontal { height: 6px; background: #2A2F37; border-radius: 3px; }
        QSlider::handle:horizontal { background: #5082E6; width: 18px; border-radius: 9px; margin: -6px 0; }
        /* Selection toolbar */
        #selectionToolbar { background: rgba(40,42,52,230); border:1px solid #3A404B; border-radius:24px; }
        #selectionToolbar QPushButton#toolbarButton { background: transparent; min-width:38px; min-height:38px; font-size:16px; font-weight:600; }
        #selectionToolbar QPushButton#toolbarButton:hover { background:#303642; }
        #selectionToolbar QPushButton#toolbarButton:pressed { background:#252a31; }
        /* OCR Result window placeholders */
        #ocrImagePlaceholder[placeholder="true"] { border:2px dashed #4a505a; padding:20px; color:#6c737d; }
        QLabel[placeholder="true"] { color:#6c737d; }
        QTextEdit[hint="true"] { color:#8a93a1; font-style:italic; }
        QTextEdit[error="true"] { color:#ff6b6b; font-style:italic; }
        QLabel[status="success"], QLabel[status="error"], QLabel[status="progress"] { font-weight:600; }
        QLabel[status="success"] { color:#4CCA6A; }
        QLabel[status="error"] { color:#FF5A5F; }
        QLabel[status="progress"] { color:#5082E6; }
        /* Floating widget */
    #floatingWidget { background:rgba(44,49,58,212); border:1px solid #3A404B; border-radius:18px; }
    #floatingWidget[highlight="true"] { border-color:#5082E6; }
        #floatingWidget QPushButton#floatingButton { background:transparent; border:none; font-size:18px; font-weight:600; color:#E6E9EF; }
        #floatingWidget QPushButton#floatingButton:hover { background:#303642; border-radius:12px; }
        #floatingWidget QPushButton#floatingButton:pressed { background:#252a31; }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid #4CCA6A; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid #FF5A5F; }
    )";

    setAppPaletteAndStyle(pal, style);
}

void ThemeManager::applyHighContrast() {
    QPalette pal;
    pal.setColor(QPalette::Window, Qt::black);
    pal.setColor(QPalette::WindowText, Qt::yellow);
    pal.setColor(QPalette::Base, Qt::black);
    pal.setColor(QPalette::Text, Qt::yellow);
    pal.setColor(QPalette::Button, Qt::black);
    pal.setColor(QPalette::ButtonText, Qt::yellow);
    pal.setColor(QPalette::Highlight, Qt::yellow);
    pal.setColor(QPalette::HighlightedText, Qt::black);
    const QString style = R"(
        QWidget { background: black; color: yellow; }
        QPushButton { background: black; color: yellow; border: 2px solid yellow; }
        QLineEdit, QTextEdit, QComboBox { background: black; color: yellow; border: 2px solid yellow; }
        /* Floating widget */
        #floatingWidget { background:rgba(0,0,0,210); border:2px solid yellow; border-radius:18px; }
        #floatingWidget[highlight="true"] { border-color:white; }
        QGroupBox { padding-top:18px; margin-top:14px; border:2px solid yellow; border-radius:8px; }
        QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left:12px; top:4px; padding:2px 6px; background:black; font-weight:700; }
        #floatingWidget QPushButton#floatingButton { background:transparent; border:none; font-size:18px; font-weight:700; color:yellow; }
        #floatingWidget QPushButton#floatingButton:hover { background:rgba(255,255,0,0.2); border-radius:12px; }
        #floatingWidget QPushButton#floatingButton:pressed { background:rgba(255,255,0,0.35); }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid #00ff00; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid #ff0000; }
    )";
    setAppPaletteAndStyle(pal, style);
}

void ThemeManager::applyCyberpunk() {
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(10, 12, 20));
    pal.setColor(QPalette::WindowText, QColor(255, 255, 255));
    pal.setColor(QPalette::Base, QColor(8, 10, 18));
    pal.setColor(QPalette::AlternateBase, QColor(14, 16, 26));
    pal.setColor(QPalette::ToolTipBase, QColor(20, 24, 40));
    pal.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    pal.setColor(QPalette::Text, QColor(230, 255, 252));
    pal.setColor(QPalette::Button, QColor(14, 16, 26));
    pal.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    pal.setColor(QPalette::Highlight, QColor(0, 255, 208));
    pal.setColor(QPalette::HighlightedText, QColor(10, 12, 20));

    const QString style = R"(
        /* Neon cyberpunk: teal + magenta accents */
        QWidget { background-color: #0A0C14; color: #E6FFFC; }
        QDialog, QMainWindow { background-color: #0A0C14; }
        QGroupBox { border: 1px solid rgba(0,255,208,0.35); border-radius: 8px; margin-top: 10px; }
        QGroupBox { padding-top:20px; margin-top:14px; }
        QGroupBox::title { padding:2px 8px; color: #00FFD0; left:12px; top:4px; subcontrol-origin: margin; subcontrol-position: top left; background:#0A0C14; font-weight:600; }
        QPushButton { background: #0E101A; color: #FFFFFF; border: 1px solid rgba(255,0,128,0.4); border-radius: 10px; padding: 8px 14px; }
        QPushButton:hover { border-color: #00FFD0; }
        QPushButton#applyBtn { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #00FFD0, stop:1 #FF0080); color: #0A0C14; border: none; }
        QTabWidget::pane { border: 1px solid rgba(255,0,128,0.35); border-radius: 8px; }
        QTabBar::tab { background: #0E101A; padding: 10px 18px; color: #B3FFF3; border: 1px solid rgba(0,255,208,0.25); border-bottom: none; }
        QTabBar::tab:selected { background: #121624; color: #FFFFFF; border-color: rgba(255,0,128,0.6); }
        QLineEdit, QTextEdit, QComboBox, QSpinBox { background: #0E101A; color: #E6FFFC; border: 1px solid rgba(0,255,208,0.35); border-radius: 8px; }
        QCheckBox { color: #CCFCEF; }
        QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #FF0080; border-radius: 4px; background: #0E101A; }
        QCheckBox::indicator:checked { background: #00FFD0; border-color: #00FFD0; }
        QSlider::groove:horizontal { height: 6px; background: #121624; border-radius: 3px; }
        QSlider::handle:horizontal { background: #FF0080; width: 18px; border-radius: 9px; margin: -6px 0; border: 1px solid #00FFD0; }
        QScrollBar::handle:vertical { background: #1A1E30; border: 1px solid rgba(0,255,208,0.25); border-radius: 5px; }
        QLabel { color: #B3FFF3; }
        #selectionToolbar { background: rgba(14,16,26,235); border:1px solid rgba(255,0,128,0.4); border-radius:24px; }
        #selectionToolbar QPushButton#toolbarButton { background: transparent; min-width:38px; min-height:38px; font-size:16px; font-weight:600; }
        #selectionToolbar QPushButton#toolbarButton:hover { background: rgba(255,0,128,0.15); }
        #selectionToolbar QPushButton#toolbarButton:pressed { background: rgba(0,255,208,0.18); }
        #ocrImagePlaceholder[placeholder="true"] { border:2px dashed rgba(0,255,208,0.4); padding:20px; color:#00FFD0; }
        QTextEdit[hint="true"] { color:#7dd6c8; font-style:italic; }
        QTextEdit[error="true"] { color:#FF479C; font-style:italic; }
        QLabel[status="success"], QLabel[status="error"], QLabel[status="progress"] { font-weight:600; }
        QLabel[status="success"] { color:#00FFD0; }
        QLabel[status="error"] { color:#FF479C; }
        QLabel[status="progress"] { color:#FF0080; }
        /* Floating widget */
    #floatingWidget { background:rgba(14,16,26,225); border:1px solid rgba(255,0,128,0.45); border-radius:18px; }
    #floatingWidget[highlight="true"] { border-color:#00FFD0; }
        #floatingWidget QPushButton#floatingButton { background:transparent; border:none; font-size:20px; font-weight:600; color:#00FFD0; }
        #floatingWidget QPushButton#floatingButton:hover { background:rgba(255,0,128,0.18); border-radius:12px; }
        #floatingWidget QPushButton#floatingButton:pressed { background:rgba(0,255,208,0.25); }
        /* Validation */
        QComboBox[valid="true"], QLineEdit[valid="true"] { border:1px solid #00FFD0; }
        QComboBox[valid="false"], QLineEdit[valid="false"] { border:1px solid #FF479C; }
    )";

    setAppPaletteAndStyle(pal, style);
}
