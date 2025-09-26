#include "LanguageManager.h"
#include <QRegularExpression>
#include <QDebug>

LanguageManager* LanguageManager::s_instance = nullptr;

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{
    initializeLanguages();
}

LanguageManager& LanguageManager::instance()
{
    if (!s_instance) {
        s_instance = new LanguageManager();
    }
    return *s_instance;
}

void LanguageManager::initializeLanguages()
{
    // Initialize supported languages with their locales and aliases
    addLanguage("en", "English", "English",
                QLocale(QLocale::English, QLocale::UnitedStates),
                {"english", "en-us", "en-gb", "en-ca", "en-au", "en-in"});

    addLanguage("zh", "Chinese (Simplified)", "中文（简体）",
                QLocale(QLocale::Chinese, QLocale::China),
                {"chinese", "zh-cn", "zh-hans", "simplified chinese", "mandarin"});

    addLanguage("zh-tw", "Chinese (Traditional)", "中文（繁體）",
                QLocale(QLocale::Chinese, QLocale::Taiwan),
                {"zh-hk", "zh-hant", "traditional chinese", "taiwanese", "cantonese"});

    addLanguage("ja", "Japanese", "日本語",
                QLocale(QLocale::Japanese),
                {"japanese", "jp"});

    addLanguage("ko", "Korean", "한국어",
                QLocale(QLocale::Korean),
                {"korean", "kr"});

    addLanguage("sv", "Swedish", "Svenska",
                QLocale(QLocale::Swedish),
                {"swedish", "se"});

    addLanguage("es", "Spanish", "Español",
                QLocale(QLocale::Spanish),
                {"spanish", "es-es", "es-mx", "es-us", "castellano"});

    addLanguage("fr", "French", "Français",
                QLocale(QLocale::French),
                {"french", "fr-fr", "fr-ca", "français"});

    addLanguage("de", "German", "Deutsch",
                QLocale(QLocale::German),
                {"german", "deutsch"});

    addLanguage("it", "Italian", "Italiano",
                QLocale(QLocale::Italian),
                {"italian"});

    addLanguage("pt", "Portuguese", "Português",
                QLocale(QLocale::Portuguese),
                {"portuguese", "pt-br", "pt-pt", "português", "brasileiro"});

    addLanguage("ru", "Russian", "Русский",
                QLocale(QLocale::Russian),
                {"russian", "русский"});

    addLanguage("ar", "Arabic", "العربية",
                QLocale(QLocale::Arabic),
                {"arabic", "عربي"});

    addLanguage("hi", "Hindi", "हिन्दी",
                QLocale(QLocale::Hindi),
                {"hindi", "हिंदी"});

    addLanguage("th", "Thai", "ไทย",
                QLocale(QLocale::Thai),
                {"thai"});

    addLanguage("vi", "Vietnamese", "Tiếng Việt",
                QLocale(QLocale::Vietnamese),
                {"vietnamese", "tiếng việt"});

    qDebug() << "LanguageManager initialized with" << m_languages.size() << "languages";
}

void LanguageManager::addLanguage(const QString& code, const QString& name, const QString& nativeName,
                                 const QLocale& locale, const QStringList& aliases)
{
    LanguageInfo info;
    info.code = code;
    info.name = name;
    info.nativeName = nativeName;
    info.locale = locale;
    info.aliases = aliases;

    m_languages[code] = info;

    // Add aliases mapping to canonical code
    m_aliasToCode[code.toLower()] = code;  // Self-reference
    for (const QString& alias : aliases) {
        m_aliasToCode[alias.toLower()] = code;
    }
}

QLocale LanguageManager::localeFromLanguageCode(const QString& languageCode) const
{
    if (languageCode.isEmpty()) {
        return QLocale();
    }

    QString normalized = normalizeLanguageCode(languageCode);
    if (m_languages.contains(normalized)) {
        return m_languages[normalized].locale;
    }

    // Fallback: try to construct QLocale directly
    QLocale locale(languageCode);
    if (locale.language() != QLocale::AnyLanguage) {
        return locale;
    }

    // Default to system locale
    qDebug() << "LanguageManager: Unsupported language code:" << languageCode;
    return QLocale();
}

QString LanguageManager::languageCodeFromLocale(const QLocale& locale) const
{
    // Find language by matching locale
    for (auto it = m_languages.begin(); it != m_languages.end(); ++it) {
        if (it.value().locale.language() == locale.language()) {
            // For Chinese, check territory/script too
            if (locale.language() == QLocale::Chinese) {
                if (locale.territory() == QLocale::Taiwan ||
                    locale.territory() == QLocale::HongKong ||
                    locale.script() == QLocale::TraditionalChineseScript) {
                    return "zh-tw";
                } else {
                    return "zh";
                }
            }
            return it.key();
        }
    }

    // Fallback to locale name
    return locale.name().section('_', 0, 0);
}

LanguageManager::LanguageInfo LanguageManager::languageInfo(const QString& languageCode) const
{
    QString normalized = normalizeLanguageCode(languageCode);
    return m_languages.value(normalized, LanguageInfo{});
}

bool LanguageManager::isSupported(const QString& languageCode) const
{
    QString normalized = normalizeLanguageCode(languageCode);
    return m_languages.contains(normalized);
}

QStringList LanguageManager::supportedLanguageCodes() const
{
    return m_languages.keys();
}

QStringList LanguageManager::supportedLanguageNames() const
{
    QStringList names;
    for (const auto& info : m_languages) {
        names << info.name;
    }
    return names;
}

QString LanguageManager::normalizeLanguageCode(const QString& input) const
{
    if (input.isEmpty()) {
        return QString();
    }

    QString lower = input.toLower().trimmed();

    // Check direct match or alias
    if (m_aliasToCode.contains(lower)) {
        return m_aliasToCode[lower];
    }

    // Handle common variations
    if (lower.startsWith("zh")) {
        if (lower.contains("tw") || lower.contains("hk") ||
            lower.contains("traditional") || lower.contains("hant")) {
            return "zh-tw";
        } else {
            return "zh";
        }
    }

    // Extract main language part (before hyphen or underscore)
    QString mainLang = lower.section(QRegularExpression("[-_]"), 0, 0);
    if (m_aliasToCode.contains(mainLang)) {
        return m_aliasToCode[mainLang];
    }

    return QString();
}

QString LanguageManager::detectLanguageFromText(const QString& text) const
{
    if (text.isEmpty()) {
        return QString();
    }

    // Basic heuristic detection based on character ranges
    // This is a simple implementation - for production, use proper language detection

    // Check for Chinese characters
    if (text.contains(QRegularExpression("[\\u4e00-\\u9fff]"))) {
        // Simple heuristic: Traditional Chinese often uses specific characters
        if (text.contains(QRegularExpression("[繁體傳統]"))) {
            return "zh-tw";
        }
        return "zh";
    }

    // Check for Japanese (Hiragana/Katakana)
    if (text.contains(QRegularExpression("[\\u3040-\\u309f\\u30a0-\\u30ff]"))) {
        return "ja";
    }

    // Check for Korean (Hangul)
    if (text.contains(QRegularExpression("[\\uac00-\\ud7af]"))) {
        return "ko";
    }

    // Check for Arabic
    if (text.contains(QRegularExpression("[\\u0600-\\u06ff]"))) {
        return "ar";
    }

    // Check for Thai
    if (text.contains(QRegularExpression("[\\u0e00-\\u0e7f]"))) {
        return "th";
    }

    // Check for Russian (Cyrillic)
    if (text.contains(QRegularExpression("[\\u0400-\\u04ff]"))) {
        return "ru";
    }

    // Check for Hindi (Devanagari)
    if (text.contains(QRegularExpression("[\\u0900-\\u097f]"))) {
        return "hi";
    }

    // Default to English for Latin scripts
    return "en";
}

QString LanguageManager::displayName(const QString& languageCode) const
{
    return languageInfo(languageCode).name;
}

QString LanguageManager::nativeName(const QString& languageCode) const
{
    return languageInfo(languageCode).nativeName;
}