#pragma once

#include <QObject>
#include <QString>
#include <QLocale>
#include <QMap>
#include <QList>

/**
 * Language Manager - Single Source of Truth for all language information
 * Centralized database containing ISO codes, OCR codes, diacritics, scripts, etc.
 * Singleton pattern for consistent language handling across the application
 */
class LanguageManager : public QObject
{
    Q_OBJECT

public:
    /**
     * Language Information - Complete definition for each supported language
     * This is the tautology - one language, one place, one truth
     */
    struct LanguageInfo {
        // Identity
        QString displayName;        // Native name: "Svenska", "Français", "中文"
        QString englishName;        // English name: "Swedish", "French", "Chinese"
        QString isoCode;            // ISO 639-1 code: "sv", "fr", "zh"

        // OCR-specific properties
        QString tesseractCode;      // Tesseract lang code: "swe", "fra", "chi_sim"
        QString diacritics;         // Special characters: "åäöÅÄÖ", "éèêëàâ..."
        bool hasCJKScript;          // true for Chinese/Japanese/Korean
        bool requiresNoWhitelist;   // true for CJK/Russian (too many chars)

        // System properties
        QLocale::Language qtLocale; // Qt locale enum
        QLocale::Script script;     // LatinScript, CyrillicScript, etc.

        // Helper methods
        QString getFullWhitelist() const;  // Base ASCII + diacritics
        bool isLatinBased() const;         // true if uses Latin script
    };

    static LanguageManager& instance();

    // Convert language code to QLocale
    QLocale localeFromLanguageCode(const QString& languageCode) const;

    // Get human-readable display name for language
    QString displayName(const QString& languageCode) const;

    // Get language code from QLocale
    QString languageCodeFromLocale(const QLocale& locale) const;

    // Check if language code is supported
    bool isSupported(const QString& languageCode) const;

    // Get list of supported languages
    QStringList supportedLanguages() const;

    // Convert display name back to language code
    QString languageCodeFromDisplayName(const QString& displayName) const;

    // ========== NEW APIs: Single Source of Truth ==========

    // Get complete language information
    LanguageInfo getInfo(const QString& languageCode) const;
    LanguageInfo getInfoByDisplayName(const QString& displayName) const;
    LanguageInfo getInfoByEnglishName(const QString& englishName) const;

    // Get all supported languages
    QList<LanguageInfo> allLanguages() const;

    // OCR-specific helpers (convenience methods)
    QString getTesseractCode(const QString& displayName) const;
    QString getMultiLanguageTesseractCode(const QString& displayName) const;  // NEW: Returns "swe+eng" for Swedish, etc.
    QString getCharacterWhitelist(const QString& displayName) const;

private:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override = default;

    // Prevent copying
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;

    void initializeLanguageMap();
    void initializeLanguageDatabase();  // NEW: Initialize the tautology

    // OLD APIs data (keep for compatibility during transition)
    QMap<QString, QLocale::Language> m_languageMap;
    QMap<QString, QString> m_displayNames;

    // NEW: The Single Source of Truth
    QList<LanguageInfo> m_languages;
};