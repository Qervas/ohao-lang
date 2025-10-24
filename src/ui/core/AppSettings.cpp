#include "AppSettings.h"
#include "ThemeColors.h"
#include <QCoreApplication>
#include <QDebug>
#include <QLocale>

QString AppSettings::getSystemDefaultLanguage()
{
    // Map system locale to supported language names
    QLocale systemLocale = QLocale::system();
    QString langCode = systemLocale.name().left(2).toLower();

    // Map language codes to our supported language display names
    static QMap<QString, QString> langMapping = {
        {"en", "English"},
        {"zh", "Chinese (Simplified)"},
        {"ja", "Japanese"},
        {"ko", "Korean"},
        {"es", "Spanish"},
        {"fr", "French"},
        {"de", "German"},
        {"ru", "Russian"},
        {"pt", "Portuguese"},
        {"it", "Italian"},
        {"nl", "Dutch"},
        {"pl", "Polish"},
        {"sv", "Swedish"},
        {"ar", "Arabic"},
        {"hi", "Hindi"},
        {"th", "Thai"},
        {"vi", "Vietnamese"}
    };

    QString mappedLang = langMapping.value(langCode, "English");
    qDebug() << "System locale:" << systemLocale.name() << "-> Mapped to:" << mappedLang;
    return mappedLang;
}

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
        // Use system default language instead of hardcoded "English"
        m_cachedOCRConfig.language = m_settings->value("ocr/language", getSystemDefaultLanguage()).toString();
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

        // Default source language to OCR language (intelligent default)
        QString ocrLang = getOCRConfig().language;
        m_cachedTranslationConfig.sourceLanguage = m_settings->value("translation/sourceLanguage", ocrLang).toString();

        // Intelligent target language default: ensure source ≠ target for meaningful translation
        // If OCR language matches system language, use English (most universal)
        // Otherwise use system language as target (user likely wants to translate TO their native language)
        QString systemLang = getSystemDefaultLanguage();
        QString defaultTarget = (ocrLang == systemLang) ? "English" : systemLang;
        m_cachedTranslationConfig.targetLanguage = m_settings->value("translation/targetLanguage", defaultTarget).toString();

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
        // Default TTS input language to OCR language
        QString ocrLang = getOCRConfig().language;
        m_cachedTTSConfig.inputLanguage = m_settings->value("tts/inputLanguage", ocrLang).toString();

        // Default TTS output language to translation target language
        QString targetLang = getTranslationConfig().targetLanguage;
        m_cachedTTSConfig.outputLanguage = m_settings->value("tts/outputLanguage", targetLang).toString();
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
    // This method is deprecated. Use ThemeManager and ThemeColors instead.
    // Kept for backward compatibility but returns empty string.
    Q_UNUSED(componentName);
    return "";
}

QColor AppSettings::getThemeColor(const QString& colorName) const
{
    // Use centralized theme colors from ThemeColors.h
    QString currentTheme = theme();
    ThemeColors::ThemeColorSet colors = ThemeColors::getColorSet(currentTheme);

    // Map color names to theme colors
    if (colorName == "background") return colors.window;
    if (colorName == "surface") return colors.button;
    if (colorName == "border") return colors.floatingWidgetBorder;
    if (colorName == "text") return colors.windowText;
    if (colorName == "primary") return colors.highlight;
    if (colorName == "success") return colors.success;
    if (colorName == "error") return colors.error;

    // Default fallback
    return colors.window;
}