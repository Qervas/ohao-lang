#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <memory>

// Make Hunspell optional - compile without it if not available
#ifdef HUNSPELL_AVAILABLE
class Hunspell;
#endif

class SpellChecker
{
public:
    explicit SpellChecker(const QString& languageCode);
    ~SpellChecker();

    // Main correction function
    QString correctText(const QString& text);

    // Check if word is spelled correctly
    bool isCorrect(const QString& word);

    // Get suggestions for misspelled word
    QStringList suggest(const QString& word);

    // Check if spellchecker is loaded
    bool isLoaded() const {
#ifdef HUNSPELL_AVAILABLE
        return m_hunspell != nullptr;
#else
        return false; // Without Hunspell, always return false
#endif
    }

private:
#ifdef HUNSPELL_AVAILABLE
    Hunspell* m_hunspell = nullptr;
#endif
    QString m_languageCode;

    // Helper to correct a single word
    QString correctWord(const QString& word);

    // Convert QString to encoding Hunspell expects
    QByteArray toHunspellEncoding(const QString& str);
    QString fromHunspellEncoding(const char* str);
};
