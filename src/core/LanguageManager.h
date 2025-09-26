#pragma once

#include <QObject>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QHash>

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    struct LanguageInfo {
        QString code;              // ISO 639-1 code (e.g., "en", "zh")
        QString name;              // Display name (e.g., "English", "Chinese")
        QString nativeName;        // Native name (e.g., "English", "中文")
        QLocale locale;            // Qt locale
        QStringList aliases;       // Alternative codes/names
    };

    static LanguageManager& instance();

    // Core language conversion methods
    QLocale localeFromLanguageCode(const QString& languageCode) const;
    QString languageCodeFromLocale(const QLocale& locale) const;
    LanguageInfo languageInfo(const QString& languageCode) const;

    // Utility methods
    bool isSupported(const QString& languageCode) const;
    QStringList supportedLanguageCodes() const;
    QStringList supportedLanguageNames() const;

    // Language detection and normalization
    QString normalizeLanguageCode(const QString& input) const;
    QString detectLanguageFromText(const QString& text) const; // Basic heuristic detection

    // Display helpers
    QString displayName(const QString& languageCode) const;
    QString nativeName(const QString& languageCode) const;

private:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() = default;

    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;

    void initializeLanguages();
    void addLanguage(const QString& code, const QString& name, const QString& nativeName,
                    const QLocale& locale, const QStringList& aliases = {});

    QHash<QString, LanguageInfo> m_languages;
    QHash<QString, QString> m_aliasToCode; // Maps aliases to canonical codes
    static LanguageManager* s_instance;
};