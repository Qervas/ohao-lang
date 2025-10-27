#include "LanguageManager.h"
#include <QCoreApplication>
#include <QDebug>

LanguageManager& LanguageManager::instance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{
    initializeLanguageMap();         // OLD API (keep for compatibility)
    initializeLanguageDatabase();    // NEW: The Single Source of Truth
}

// ========== LanguageInfo Helper Methods ==========

QString LanguageManager::LanguageInfo::getFullWhitelist() const
{
    if (requiresNoWhitelist) {
        return ""; // No whitelist for CJK/Cyrillic - too many characters
    }

    // Base ASCII character set for all Latin-based languages
    static const QString base =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789.,!?;:()[]{}\"'-+=/\\@#$%&*_<> \n\t";

    return base + diacritics;
}

bool LanguageManager::LanguageInfo::isLatinBased() const
{
    return script == QLocale::LatinScript;
}

void LanguageManager::initializeLanguageMap()
{
    // Initialize common language codes to QLocale mappings
    m_languageMap["en"] = QLocale::English;
    m_languageMap["en-US"] = QLocale::English;
    m_languageMap["en-GB"] = QLocale::English;
    m_languageMap["zh"] = QLocale::Chinese;
    m_languageMap["zh-CN"] = QLocale::Chinese;
    m_languageMap["zh-TW"] = QLocale::Chinese;
    m_languageMap["ja"] = QLocale::Japanese;
    m_languageMap["ko"] = QLocale::Korean;
    m_languageMap["fr"] = QLocale::French;
    m_languageMap["de"] = QLocale::German;
    m_languageMap["es"] = QLocale::Spanish;
    m_languageMap["it"] = QLocale::Italian;
    m_languageMap["pt"] = QLocale::Portuguese;
    m_languageMap["ru"] = QLocale::Russian;
    m_languageMap["ar"] = QLocale::Arabic;
    m_languageMap["hi"] = QLocale::Hindi;
    m_languageMap["th"] = QLocale::Thai;
    m_languageMap["vi"] = QLocale::Vietnamese;
    m_languageMap["nl"] = QLocale::Dutch;
    m_languageMap["sv"] = QLocale::Swedish;
    m_languageMap["da"] = QLocale::Danish;
    m_languageMap["no"] = QLocale::NorwegianBokmal;
    m_languageMap["fi"] = QLocale::Finnish;
    m_languageMap["pl"] = QLocale::Polish;
    m_languageMap["tr"] = QLocale::Turkish;
    m_languageMap["he"] = QLocale::Hebrew;

    // Initialize display names
    m_displayNames["en"] = "English";
    m_displayNames["en-US"] = "English (US)";
    m_displayNames["en-GB"] = "English (UK)";
    m_displayNames["zh"] = "中文";
    m_displayNames["zh-CN"] = "中文 (简体)";
    m_displayNames["zh-TW"] = "中文 (繁體)";
    m_displayNames["ja"] = "日本語";
    m_displayNames["ko"] = "한국어";
    m_displayNames["fr"] = "Français";
    m_displayNames["de"] = "Deutsch";
    m_displayNames["es"] = "Español";
    m_displayNames["it"] = "Italiano";
    m_displayNames["pt"] = "Português";
    m_displayNames["ru"] = "Русский";
    m_displayNames["ar"] = "العربية";
    m_displayNames["hi"] = "हिन्दी";
    m_displayNames["th"] = "ไทย";
    m_displayNames["vi"] = "Tiếng Việt";
    m_displayNames["nl"] = "Nederlands";
    m_displayNames["sv"] = "Svenska";
    m_displayNames["sv-SE"] = "Svenska";
    m_displayNames["da"] = "Dansk";
    m_displayNames["no"] = "Norsk";
    m_displayNames["fi"] = "Suomi";
    m_displayNames["pl"] = "Polski";
    m_displayNames["tr"] = "Türkçe";
    m_displayNames["he"] = "עברית";

    // Removed debug log to reduce console noise
}

void LanguageManager::initializeLanguageDatabase()
{
    // ========== THE SINGLE SOURCE OF TRUTH ==========
    // All language information in one place - the tautology
    // Add new languages here ONLY

    m_languages = {
        // English
        {"English", "English", "en", "eng", "", false, false, QLocale::English, QLocale::LatinScript},

        // European Latin-based languages
        {"Svenska", "Swedish", "sv", "swe", "åäöÅÄÖ", false, false, QLocale::Swedish, QLocale::LatinScript},
        {"Français", "French", "fr", "fra", "éèêëàâùûïîôçÉÈÊËÀÂÙÛÏÎÔÇæœÆŒ", false, false, QLocale::French, QLocale::LatinScript},
        {"Deutsch", "German", "de", "deu", "äöüßÄÖÜ", false, false, QLocale::German, QLocale::LatinScript},
        {"Español", "Spanish", "es", "spa", "áéíóúñÁÉÍÓÚÑ¿¡üÜ", false, false, QLocale::Spanish, QLocale::LatinScript},
        {"Português", "Portuguese", "pt", "por", "áâãàéêíóôõúçÁÂÃÀÉÊÍÓÔÕÚÇ", false, false, QLocale::Portuguese, QLocale::LatinScript},
        {"Italiano", "Italian", "it", "ita", "àèéìíîòóùúÀÈÉÌÍÎÒÓÙÚ", false, false, QLocale::Italian, QLocale::LatinScript},
        {"Nederlands", "Dutch", "nl", "nld", "áéëïóöüÁÉËÏÓÖÜ", false, false, QLocale::Dutch, QLocale::LatinScript},
        {"Polski", "Polish", "pl", "pol", "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ", false, false, QLocale::Polish, QLocale::LatinScript},
        {"Dansk", "Danish", "da", "dan", "æøåÆØÅ", false, false, QLocale::Danish, QLocale::LatinScript},
        {"Norsk", "Norwegian", "no", "nor", "æøåÆØÅ", false, false, QLocale::NorwegianBokmal, QLocale::LatinScript},
        {"Suomi", "Finnish", "fi", "fin", "äöšžÄÖŠŽ", false, false, QLocale::Finnish, QLocale::LatinScript},
        {"Türkçe", "Turkish", "tr", "tur", "çğıöşüÇĞİÖŞÜ", false, false, QLocale::Turkish, QLocale::LatinScript},

        // Cyrillic-based languages (no whitelist - full Cyrillic range)
        {"Русский", "Russian", "ru", "rus", "", false, true, QLocale::Russian, QLocale::CyrillicScript},
        {"Українська", "Ukrainian", "uk", "ukr", "", false, true, QLocale::Ukrainian, QLocale::CyrillicScript},

        // CJK languages (no whitelist - tens of thousands of characters)
        {"中文 (简体)", "Chinese (Simplified)", "zh-CN", "chi_sim", "", true, true, QLocale::Chinese, QLocale::SimplifiedHanScript},
        {"中文 (繁體)", "Chinese (Traditional)", "zh-TW", "chi_tra", "", true, true, QLocale::Chinese, QLocale::TraditionalHanScript},
        {"日本語", "Japanese", "ja", "jpn", "", true, true, QLocale::Japanese, QLocale::JapaneseScript},
        {"한국어", "Korean", "ko", "kor", "", true, true, QLocale::Korean, QLocale::KoreanScript},

        // Other scripts
        {"العربية", "Arabic", "ar", "ara", "", false, true, QLocale::Arabic, QLocale::ArabicScript},
        {"हिन्दी", "Hindi", "hi", "hin", "", false, true, QLocale::Hindi, QLocale::DevanagariScript},
        {"ไทย", "Thai", "th", "tha", "", false, true, QLocale::Thai, QLocale::ThaiScript},
        {"Tiếng Việt", "Vietnamese", "vi", "vie", "ăâđêôơưĂÂĐÊÔƠƯáàảãạéèẻẽẹíìỉĩịóòỏõọúùủũụýỳỷỹỵÁÀẢÃẠÉÈẺẼẸÍÌỈĨỊÓÒỎÕỌÚÙỦŨỤÝỲỶỸỴ", false, false, QLocale::Vietnamese, QLocale::LatinScript},
        {"עברית", "Hebrew", "he", "heb", "", false, true, QLocale::Hebrew, QLocale::HebrewScript},
    };

    // Removed debug log to reduce console noise
}

QLocale LanguageManager::localeFromLanguageCode(const QString& languageCode) const
{
    if (languageCode.isEmpty() || languageCode == "Auto-Detect") {
        return QLocale::system();
    }

    // Check direct mapping first
    if (m_languageMap.contains(languageCode)) {
        return QLocale(m_languageMap[languageCode]);
    }

    // Try base language code (e.g., "en" from "en-US")
    QString baseCode = languageCode.split('-').first();
    if (m_languageMap.contains(baseCode)) {
        return QLocale(m_languageMap[baseCode]);
    }

    // Fallback to system locale
    qDebug() << "Language code not found:" << languageCode << "- using system locale";
    return QLocale::system();
}

QString LanguageManager::displayName(const QString& languageCode) const
{
    if (languageCode.isEmpty() || languageCode == "Auto-Detect") {
        return "Auto-Detect";
    }

    // Check direct mapping first
    if (m_displayNames.contains(languageCode)) {
        return m_displayNames[languageCode];
    }

    // Try base language code
    QString baseCode = languageCode.split('-').first();
    if (m_displayNames.contains(baseCode)) {
        return m_displayNames[baseCode];
    }

    // Fallback to the code itself
    return languageCode;
}

QString LanguageManager::languageCodeFromLocale(const QLocale& locale) const
{
    QLocale::Language language = locale.language();

    // Find the first matching language code
    for (auto it = m_languageMap.begin(); it != m_languageMap.end(); ++it) {
        if (it.value() == language) {
            return it.key();
        }
    }

    // Fallback to locale name
    return locale.name();
}

bool LanguageManager::isSupported(const QString& languageCode) const
{
    if (languageCode.isEmpty() || languageCode == "Auto-Detect") {
        return true;
    }

    // Check direct mapping
    if (m_languageMap.contains(languageCode)) {
        return true;
    }

    // Check base language code
    QString baseCode = languageCode.split('-').first();
    return m_languageMap.contains(baseCode);
}

QStringList LanguageManager::supportedLanguages() const
{
    QStringList languages;
    languages << "Auto-Detect";
    languages << m_languageMap.keys();
    languages.sort();
    return languages;
}

QString LanguageManager::languageCodeFromDisplayName(const QString& displayName) const
{
    if (displayName.isEmpty() || displayName == "Auto-Detect") {
        return "Auto-Detect";
    }

    // Search through display names to find matching language code
    for (auto it = m_displayNames.begin(); it != m_displayNames.end(); ++it) {
        if (it.value().compare(displayName, Qt::CaseInsensitive) == 0) {
            return it.key();
        }
    }

    // Handle common English language names that might come from OCR
    QMap<QString, QString> englishNames;
    englishNames["English"] = "en";
    englishNames["Chinese"] = "zh";
    englishNames["Japanese"] = "ja";
    englishNames["Korean"] = "ko";
    englishNames["French"] = "fr";
    englishNames["German"] = "de";
    englishNames["Spanish"] = "es";
    englishNames["Italian"] = "it";
    englishNames["Portuguese"] = "pt";
    englishNames["Russian"] = "ru";
    englishNames["Arabic"] = "ar";
    englishNames["Hindi"] = "hi";
    englishNames["Thai"] = "th";
    englishNames["Vietnamese"] = "vi";
    englishNames["Dutch"] = "nl";
    englishNames["Swedish"] = "sv";
    englishNames["Danish"] = "da";
    englishNames["Norwegian"] = "no";
    englishNames["Finnish"] = "fi";
    englishNames["Polish"] = "pl";
    englishNames["Turkish"] = "tr";
    englishNames["Hebrew"] = "he";

    if (englishNames.contains(displayName)) {
        return englishNames[displayName];
    }

    // If not found, assume it's already a language code
    return displayName;
}

// ========== NEW APIs: Single Source of Truth ==========

LanguageManager::LanguageInfo LanguageManager::getInfo(const QString& languageCode) const
{
    // Search by ISO code or Tesseract code
    for (const auto& lang : m_languages) {
        if (lang.isoCode == languageCode || lang.tesseractCode == languageCode) {
            return lang;
        }
    }

    // Fallback: return English
    return m_languages.first();
}

LanguageManager::LanguageInfo LanguageManager::getInfoByDisplayName(const QString& displayName) const
{
    // Try exact match with display name (native name like "Svenska")
    for (const auto& lang : m_languages) {
        if (lang.displayName == displayName) {
            return lang;
        }
    }

    // Try match with English name (like "Swedish")
    for (const auto& lang : m_languages) {
        if (lang.englishName == displayName) {
            return lang;
        }
    }

    // Try case-insensitive match
    for (const auto& lang : m_languages) {
        if (lang.displayName.compare(displayName, Qt::CaseInsensitive) == 0 ||
            lang.englishName.compare(displayName, Qt::CaseInsensitive) == 0) {
            return lang;
        }
    }

    qDebug() << "WARNING: Language not found:" << displayName << "- falling back to English";
    // Fallback: return English
    return m_languages.first();
}

LanguageManager::LanguageInfo LanguageManager::getInfoByEnglishName(const QString& englishName) const
{
    for (const auto& lang : m_languages) {
        if (lang.englishName == englishName) {
            return lang;
        }
    }

    // Fallback: return English
    return m_languages.first();
}

QList<LanguageManager::LanguageInfo> LanguageManager::allLanguages() const
{
    return m_languages;
}

QString LanguageManager::getTesseractCode(const QString& displayName) const
{
    return getInfoByDisplayName(displayName).tesseractCode;
}

QString LanguageManager::getMultiLanguageTesseractCode(const QString& displayName) const
{
    LanguageInfo info = getInfoByDisplayName(displayName);

    // Use single language only - multi-language mode (+eng) causes issues
    // where Tesseract defaults to English characters instead of native diacritics
    return info.tesseractCode;  // e.g., "swe", "fra", "deu", "eng", "chi_sim", "rus"
}

QString LanguageManager::getCharacterWhitelist(const QString& displayName) const
{
    return getInfoByDisplayName(displayName).getFullWhitelist();
}