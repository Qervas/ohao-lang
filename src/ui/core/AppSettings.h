#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QSize>
#include <QPoint>
#include <QColor>

/**
 * Centralized Settings Manager
 * Singleton pattern - one instance used everywhere
 * Type-safe accessors with sensible defaults
 */
class AppSettings : public QObject
{
    Q_OBJECT

public:
    static AppSettings& instance();
    static QString getSystemDefaultLanguage();

    // === OCR Settings ===
    struct OCRConfig {
        QString engine = "Tesseract";
        QString language; // Dynamically set from system locale
        int qualityLevel = 3;
        bool preprocessing = true;
        bool autoDetectOrientation = true;
    };

    OCRConfig getOCRConfig() const;
    void setOCRConfig(const OCRConfig& config);

    // === Translation Settings ===
    struct TranslationConfig {
        bool autoTranslate = true;
        QString engine = "Google Translate (Free)";
        QString sourceLanguage; // Dynamically set from OCR language
        QString targetLanguage; // Dynamically set from system locale or user preference
        QString overlayMode = "Deep Learning Mode";
    };

    TranslationConfig getTranslationConfig() const;
    void setTranslationConfig(const TranslationConfig& config);

    // === UI Settings ===
    struct UIConfig {
        QPoint floatingWidgetPosition = QPoint(100, 100);
        QString theme = "Auto";
        bool startWithWindows = false;
        bool minimizeToTray = true;
    };

    UIConfig getUIConfig() const;
    void setUIConfig(const UIConfig& config);

    // === TTS Settings ===
    struct TTSConfig {
        QString engine = "System";
        QString voice = "Default";
        float speed = 1.0f;
        float volume = 1.0f;
        QString inputLanguage = "Auto-Detect";
        QString outputLanguage = "English";
        bool speakTranslation = false;  // false = speak original (default), true = speak translation
        bool wordByWordReading = false;  // Add extra space between words for slower reading
    };

    TTSConfig getTTSConfig() const;
    void setTTSConfig(const TTSConfig& config);

    // === Translation Chat Settings ===
    struct ChatConfig {
        bool enabled = true;
        int opacity = 90;  // 0-100
        bool autoClearHistory = false;
        int fontSize = 12;
        bool keepOnTop = true;
    };

    ChatConfig getChatConfig() const;
    void setChatConfig(const ChatConfig& config);

    // === AI Assistant Settings (Beta) ===
    struct AIConfig {
        bool enabled = false;
        QString provider = "GitHub Copilot";
        QString apiUrl = "http://localhost:4141";
        QString apiKey = "";
        QString model = "gpt-4o";  // Default to free model
        float temperature = 0.7f;
        int maxTokens = 2000;
        bool streamResponse = false;
        QString systemPrompt = "You are a helpful translation and language learning assistant.";
        bool trackUsage = true;
        int totalTokensUsed = 0;
        bool showTokenCount = true;
        bool autoFallbackToTranslation = true;
    };

    AIConfig getAIConfig() const;
    void setAIConfig(const AIConfig& config);

    // === Global Settings ===
    struct GlobalConfig {
        bool enableGlobalShortcuts = true;
        QString screenshotShortcut = "Ctrl+Shift+S";
        bool enableSounds = true;
        bool enableAnimations = true;
    };

    GlobalConfig getGlobalConfig() const;
    void setGlobalConfig(const GlobalConfig& config);

    // Direct accessors for commonly used settings
    QString ocrEngine() const;
    void setOCREngine(const QString& engine);

    bool autoTranslate() const;
    void setAutoTranslate(bool enabled);

    QString translationTargetLanguage() const;
    void setTranslationTargetLanguage(const QString& language);

    QString theme() const;
    void setTheme(const QString& theme);

    // Centralized theming
    QString getComponentStyleSheet(const QString& componentName) const;
    QColor getThemeColor(const QString& colorName) const;

    // Utility methods
    void reset(); // Reset to defaults
    void save(); // Force save
    void reload(); // Reload from disk

signals:
    void settingsChanged();
    void ocrSettingsChanged();
    void translationSettingsChanged();
    void uiSettingsChanged();
    void ttsSettingsChanged();
    void chatSettingsChanged();
    void aiSettingsChanged();

private:
    explicit AppSettings(QObject* parent = nullptr);
    ~AppSettings() override = default;

    // Prevent copying
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    QSettings* m_settings;

    // Cached configs for performance
    mutable OCRConfig m_cachedOCRConfig;
    mutable TranslationConfig m_cachedTranslationConfig;
    mutable UIConfig m_cachedUIConfig;
    mutable TTSConfig m_cachedTTSConfig;
    mutable ChatConfig m_cachedChatConfig;
    mutable AIConfig m_cachedAIConfig;
    mutable GlobalConfig m_cachedGlobalConfig;

    // Cache validity flags
    mutable bool m_ocrCacheValid = false;
    mutable bool m_translationCacheValid = false;
    mutable bool m_uiCacheValid = false;
    mutable bool m_ttsCacheValid = false;
    mutable bool m_chatCacheValid = false;
    mutable bool m_aiCacheValid = false;
    mutable bool m_globalCacheValid = false;

    void invalidateCache();
};