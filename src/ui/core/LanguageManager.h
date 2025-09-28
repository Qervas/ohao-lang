#pragma once

#include <QObject>
#include <QString>
#include <QLocale>
#include <QMap>

/**
 * Language Manager - handles language code conversions and display names
 * Singleton pattern for consistent language handling across the application
 */
class LanguageManager : public QObject
{
    Q_OBJECT

public:
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

private:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override = default;

    // Prevent copying
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;

    void initializeLanguageMap();

    QMap<QString, QLocale::Language> m_languageMap;
    QMap<QString, QString> m_displayNames;
};