#include "AppSettings.h"
#include <QCoreApplication>
#include <QDebug>

AppSettings& AppSettings::instance()
{
    static AppSettings instance;
    return instance;
}

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
{
    m_settings = new QSettings(
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName(),
        this
    );

    qDebug() << "AppSettings initialized with file:" << m_settings->fileName();
}

// === OCR Settings ===

AppSettings::OCRConfig AppSettings::getOCRConfig() const
{
    if (!m_ocrCacheValid) {
        m_cachedOCRConfig.engine = m_settings->value("ocr/engine", "Tesseract").toString();
        m_cachedOCRConfig.language = m_settings->value("ocr/language", "English").toString();
        m_cachedOCRConfig.qualityLevel = m_settings->value("ocr/quality", 3).toInt();
        m_cachedOCRConfig.preprocessing = m_settings->value("ocr/preprocessing", true).toBool();
        m_cachedOCRConfig.autoDetectOrientation = m_settings->value("ocr/autoDetect", true).toBool();
        m_ocrCacheValid = true;
    }
    return m_cachedOCRConfig;
}

void AppSettings::setOCRConfig(const OCRConfig& config)
{
    m_settings->setValue("ocr/engine", config.engine);
    m_settings->setValue("ocr/language", config.language);
    m_settings->setValue("ocr/quality", config.qualityLevel);
    m_settings->setValue("ocr/preprocessing", config.preprocessing);
    m_settings->setValue("ocr/autoDetect", config.autoDetectOrientation);

    m_cachedOCRConfig = config;
    m_ocrCacheValid = true;

    emit ocrSettingsChanged();
    emit settingsChanged();
}

// === Translation Settings ===

AppSettings::TranslationConfig AppSettings::getTranslationConfig() const
{
    if (!m_translationCacheValid) {
        m_cachedTranslationConfig.autoTranslate = m_settings->value("translation/autoTranslate", true).toBool();
        m_cachedTranslationConfig.engine = m_settings->value("translation/engine", "Google Translate (Free)").toString();
        m_cachedTranslationConfig.sourceLanguage = m_settings->value("translation/sourceLanguage", "Auto-Detect").toString();
        m_cachedTranslationConfig.targetLanguage = m_settings->value("translation/targetLanguage", "English").toString();
        m_cachedTranslationConfig.overlayMode = m_settings->value("translation/overlayMode", "Deep Learning Mode").toString();
        m_translationCacheValid = true;
    }
    return m_cachedTranslationConfig;
}

void AppSettings::setTranslationConfig(const TranslationConfig& config)
{
    m_settings->setValue("translation/autoTranslate", config.autoTranslate);
    m_settings->setValue("translation/engine", config.engine);
    m_settings->setValue("translation/sourceLanguage", config.sourceLanguage);
    m_settings->setValue("translation/targetLanguage", config.targetLanguage);
    m_settings->setValue("translation/overlayMode", config.overlayMode);

    m_cachedTranslationConfig = config;
    m_translationCacheValid = true;

    emit translationSettingsChanged();
    emit settingsChanged();
}

// === UI Settings ===

AppSettings::UIConfig AppSettings::getUIConfig() const
{
    if (!m_uiCacheValid) {
        m_cachedUIConfig.floatingWidgetPosition = m_settings->value("ui/floatingWidgetPosition", QPoint(100, 100)).toPoint();
        m_cachedUIConfig.theme = m_settings->value("ui/theme", "Auto").toString();
        m_cachedUIConfig.startWithWindows = m_settings->value("ui/startWithWindows", false).toBool();
        m_cachedUIConfig.minimizeToTray = m_settings->value("ui/minimizeToTray", true).toBool();
        m_uiCacheValid = true;
    }
    return m_cachedUIConfig;
}

void AppSettings::setUIConfig(const UIConfig& config)
{
    m_settings->setValue("ui/floatingWidgetPosition", config.floatingWidgetPosition);
    m_settings->setValue("ui/theme", config.theme);
    m_settings->setValue("ui/startWithWindows", config.startWithWindows);
    m_settings->setValue("ui/minimizeToTray", config.minimizeToTray);

    m_cachedUIConfig = config;
    m_uiCacheValid = true;

    emit uiSettingsChanged();
    emit settingsChanged();
}

// === TTS Settings ===

AppSettings::TTSConfig AppSettings::getTTSConfig() const
{
    if (!m_ttsCacheValid) {
        m_cachedTTSConfig.engine = m_settings->value("tts/engine", "System").toString();
        m_cachedTTSConfig.voice = m_settings->value("tts/voice", "Default").toString();
        m_cachedTTSConfig.speed = m_settings->value("tts/speed", 1.0f).toFloat();
        m_cachedTTSConfig.volume = m_settings->value("tts/volume", 1.0f).toFloat();
        m_cachedTTSConfig.inputLanguage = m_settings->value("tts/inputLanguage", "Auto-Detect").toString();
        m_cachedTTSConfig.outputLanguage = m_settings->value("tts/outputLanguage", "English").toString();
        m_ttsCacheValid = true;
    }
    return m_cachedTTSConfig;
}

void AppSettings::setTTSConfig(const TTSConfig& config)
{
    m_settings->setValue("tts/engine", config.engine);
    m_settings->setValue("tts/voice", config.voice);
    m_settings->setValue("tts/speed", config.speed);
    m_settings->setValue("tts/volume", config.volume);
    m_settings->setValue("tts/inputLanguage", config.inputLanguage);
    m_settings->setValue("tts/outputLanguage", config.outputLanguage);

    m_cachedTTSConfig = config;
    m_ttsCacheValid = true;

    emit ttsSettingsChanged();
    emit settingsChanged();
}

// === Global Settings ===

AppSettings::GlobalConfig AppSettings::getGlobalConfig() const
{
    if (!m_globalCacheValid) {
        m_cachedGlobalConfig.enableGlobalShortcuts = m_settings->value("global/enableGlobalShortcuts", true).toBool();
        m_cachedGlobalConfig.screenshotShortcut = m_settings->value("global/screenshotShortcut", "Ctrl+Shift+S").toString();
        m_cachedGlobalConfig.enableSounds = m_settings->value("global/enableSounds", true).toBool();
        m_cachedGlobalConfig.enableAnimations = m_settings->value("global/enableAnimations", true).toBool();
        m_globalCacheValid = true;
    }
    return m_cachedGlobalConfig;
}

void AppSettings::setGlobalConfig(const GlobalConfig& config)
{
    m_settings->setValue("global/enableGlobalShortcuts", config.enableGlobalShortcuts);
    m_settings->setValue("global/screenshotShortcut", config.screenshotShortcut);
    m_settings->setValue("global/enableSounds", config.enableSounds);
    m_settings->setValue("global/enableAnimations", config.enableAnimations);

    m_cachedGlobalConfig = config;
    m_globalCacheValid = true;

    emit settingsChanged();
}

// === Direct Accessors ===

QString AppSettings::ocrEngine() const
{
    return getOCRConfig().engine;
}

void AppSettings::setOCREngine(const QString& engine)
{
    auto config = getOCRConfig();
    config.engine = engine;
    setOCRConfig(config);
}

bool AppSettings::autoTranslate() const
{
    return getTranslationConfig().autoTranslate;
}

void AppSettings::setAutoTranslate(bool enabled)
{
    auto config = getTranslationConfig();
    config.autoTranslate = enabled;
    setTranslationConfig(config);
}

QString AppSettings::translationTargetLanguage() const
{
    return getTranslationConfig().targetLanguage;
}

void AppSettings::setTranslationTargetLanguage(const QString& language)
{
    auto config = getTranslationConfig();
    config.targetLanguage = language;
    setTranslationConfig(config);
}

QString AppSettings::theme() const
{
    return getUIConfig().theme;
}

void AppSettings::setTheme(const QString& theme)
{
    auto config = getUIConfig();
    config.theme = theme;
    setUIConfig(config);
}

// === Utility Methods ===

void AppSettings::reset()
{
    qDebug() << "Resetting all settings to defaults";
    m_settings->clear();
    invalidateCache();
    emit settingsChanged();
}

void AppSettings::save()
{
    m_settings->sync();
    qDebug() << "Settings saved to" << m_settings->fileName();
}

void AppSettings::reload()
{
    m_settings->sync();
    invalidateCache();
    emit settingsChanged();
    qDebug() << "Settings reloaded from" << m_settings->fileName();
}

void AppSettings::invalidateCache()
{
    m_ocrCacheValid = false;
    m_translationCacheValid = false;
    m_uiCacheValid = false;
    m_ttsCacheValid = false;
    m_globalCacheValid = false;
}

QString AppSettings::getComponentStyleSheet(const QString& componentName) const
{
    QString currentTheme = theme();

    // Define theme color palettes
    QMap<QString, QMap<QString, QString>> themes;

    // Dark theme colors
    QMap<QString, QString> darkColors;
    darkColors["background"] = "#1C2026";
    darkColors["surface"] = "#2A2F37";
    darkColors["surface2"] = "#22262C";
    darkColors["border"] = "#3A404B";
    darkColors["text"] = "#E6E9EF";
    darkColors["textSecondary"] = "#C7CDD7";
    darkColors["primary"] = "#5082E6";
    darkColors["primaryHover"] = "#3C6DD9";
    darkColors["primaryPressed"] = "#2A5BC4";
    darkColors["success"] = "#4CCA6A";
    darkColors["accent"] = "#5082E6";

    // Light theme colors
    QMap<QString, QString> lightColors;
    lightColors["background"] = "#FFFFFF";
    lightColors["surface"] = "#F5F5F5";
    lightColors["surface2"] = "#EEEEEE";
    lightColors["border"] = "#E0E0E0";
    lightColors["text"] = "#212121";
    lightColors["textSecondary"] = "#757575";
    lightColors["primary"] = "#1976D2";
    lightColors["primaryHover"] = "#1565C0";
    lightColors["primaryPressed"] = "#0D47A1";
    lightColors["success"] = "#388E3C";
    lightColors["accent"] = "#1976D2";

    // High contrast theme colors
    QMap<QString, QString> highContrastColors;
    highContrastColors["background"] = "#000000";
    highContrastColors["surface"] = "#000000";
    highContrastColors["surface2"] = "#000000";
    highContrastColors["border"] = "#FFFF00";
    highContrastColors["text"] = "#FFFF00";
    highContrastColors["textSecondary"] = "#FFFF00";
    highContrastColors["primary"] = "#FFFF00";
    highContrastColors["primaryHover"] = "#000000";
    highContrastColors["primaryPressed"] = "#000000";
    highContrastColors["success"] = "#FFFF00";
    highContrastColors["accent"] = "#FFFF00";

    // Cyberpunk theme colors
    QMap<QString, QString> cyberpunkColors;
    cyberpunkColors["background"] = "#0a0e1a";
    cyberpunkColors["surface"] = "#1a1f2e";
    cyberpunkColors["surface2"] = "#0f1419";
    cyberpunkColors["border"] = "#00ff88";
    cyberpunkColors["text"] = "#00ff88";
    cyberpunkColors["textSecondary"] = "#66ffaa";
    cyberpunkColors["primary"] = "#ff0066";
    cyberpunkColors["primaryHover"] = "#ff3385";
    cyberpunkColors["primaryPressed"] = "#cc0052";
    cyberpunkColors["success"] = "#00ff88";
    cyberpunkColors["accent"] = "#00ff88";

    themes["Auto"] = darkColors; // Default to dark for auto
    themes["Light"] = lightColors;
    themes["Dark"] = darkColors;
    themes["HighContrast"] = highContrastColors;
    themes["Cyberpunk"] = cyberpunkColors;

    QMap<QString, QString> colors = themes.value(currentTheme, darkColors);

    // Component-specific stylesheets
    if (componentName == "LanguageLearningOverlay") {
        return QString(R"(
            LanguageLearningOverlay {
                background-color: %1;
                border-radius: 12px;
                border: 1px solid %2;
            }

            QFrame#headerFrame {
                background-color: %3;
                border-radius: 8px;
                border: none;
                padding: 5px;
            }

            QFrame#section {
                background-color: %4;
                border-radius: 8px;
                border: 1px solid %2;
                padding: 10px;
            }

            QLabel#titleLabel {
                font-size: 16px;
                font-weight: bold;
                color: %5;
            }

            QLabel#languageLabel {
                font-size: 11px;
                color: %6;
            }

            QLabel#sectionLabel {
                font-size: 13px;
                font-weight: bold;
                color: %5;
                margin-bottom: 5px;
            }

            QPushButton {
                background-color: %7;
                color: %5;
                border: 1px solid %8;
                border-radius: 6px;
                padding: 6px 12px;
                font-weight: 500;
            }

            QPushButton:hover {
                background-color: %8;
                border-color: %7;
            }

            QPushButton:pressed {
                background-color: %9;
            }

            QPushButton#wordButton {
                background-color: %3;
                color: %5;
                border: 1px solid %2;
                font-size: 11px;
                padding: 4px 8px;
            }

            QPushButton#wordButton:hover {
                background-color: %4;
                border-color: %7;
            }

            QTextEdit {
                background-color: %4;
                border: 1px solid %2;
                border-radius: 4px;
                padding: 8px;
                font-size: 12px;
                color: %5;
            }

            QProgressBar {
                border: 1px solid %2;
                border-radius: 4px;
                background-color: %3;
            }

            QProgressBar::chunk {
                background-color: %10;
                border-radius: 3px;
            }

            QSlider::groove:horizontal {
                height: 4px;
                background-color: %3;
                border-radius: 2px;
            }

            QSlider::handle:horizontal {
                background-color: %7;
                width: 16px;
                height: 16px;
                border-radius: 8px;
                margin: -6px 0;
            }

            QCheckBox {
                color: %5;
            }

            QScrollArea {
                background-color: %1;
                border: none;
            }
        )")
        .arg(colors["background"])     // %1
        .arg(colors["border"])         // %2
        .arg(colors["surface"])        // %3
        .arg(colors["surface2"])       // %4
        .arg(colors["text"])           // %5
        .arg(colors["textSecondary"])  // %6
        .arg(colors["primary"])        // %7
        .arg(colors["primaryHover"])   // %8
        .arg(colors["primaryPressed"]) // %9
        .arg(colors["success"]);       // %10
    }

    // Add more component stylesheets as needed
    if (componentName == "QuickTranslationOverlay") {
        // For now, QuickTranslationOverlay uses custom painting, not stylesheets
        return "";
    }

    return "";
}

QColor AppSettings::getThemeColor(const QString& colorName) const
{
    QString currentTheme = theme();

    // Same color definitions as above
    QMap<QString, QMap<QString, QString>> themes;

    QMap<QString, QString> darkColors;
    darkColors["background"] = "#1C2026";
    darkColors["surface"] = "#2A2F37";
    darkColors["border"] = "#3A404B";
    darkColors["text"] = "#E6E9EF";
    darkColors["primary"] = "#5082E6";

    QMap<QString, QString> lightColors;
    lightColors["background"] = "#FFFFFF";
    lightColors["surface"] = "#F5F5F5";
    lightColors["border"] = "#E0E0E0";
    lightColors["text"] = "#212121";
    lightColors["primary"] = "#1976D2";

    QMap<QString, QString> highContrastColors;
    highContrastColors["background"] = "#000000";
    highContrastColors["surface"] = "#000000";
    highContrastColors["border"] = "#FFFF00";
    highContrastColors["text"] = "#FFFF00";
    highContrastColors["primary"] = "#FFFF00";

    QMap<QString, QString> cyberpunkColors;
    cyberpunkColors["background"] = "#0a0e1a";
    cyberpunkColors["surface"] = "#1a1f2e";
    cyberpunkColors["border"] = "#00ff88";
    cyberpunkColors["text"] = "#00ff88";
    cyberpunkColors["primary"] = "#ff0066";

    themes["Auto"] = darkColors;
    themes["Light"] = lightColors;
    themes["Dark"] = darkColors;
    themes["HighContrast"] = highContrastColors;
    themes["Cyberpunk"] = cyberpunkColors;

    QMap<QString, QString> colors = themes.value(currentTheme, darkColors);
    QString colorHex = colors.value(colorName, "#000000");

    return QColor(colorHex);
}