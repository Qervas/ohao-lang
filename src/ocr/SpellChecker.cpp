#include "SpellChecker.h"
#ifdef HUNSPELL_AVAILABLE
#include <hunspell/hunspell.hxx>
#endif
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>

SpellChecker::SpellChecker(const QString& languageCode)
    : m_languageCode(languageCode)
{
#ifdef HUNSPELL_AVAILABLE
    QString basePath = QCoreApplication::applicationDirPath() +
                       "/resources/dictionaries/" + languageCode + "/" + languageCode;

    QString affPath = basePath + ".aff";
    QString dicPath = basePath + ".dic";

    qDebug() << "Loading Hunspell dictionary:" << affPath << dicPath;

    // Check files exist
    if (!QFile::exists(affPath) || !QFile::exists(dicPath)) {
        qWarning() << "Hunspell dictionary files not found for" << languageCode;
        return;
    }

    try {
        m_hunspell = new Hunspell(
            affPath.toStdString().c_str(),
            dicPath.toStdString().c_str()
        );
        qDebug() << "Hunspell loaded successfully for" << languageCode;
    } catch (...) {
        qWarning() << "Failed to initialize Hunspell for" << languageCode;
        m_hunspell = nullptr;
    }
#else
    qWarning() << "Hunspell not available - spellcheck disabled. Recompile with HUNSPELL_AVAILABLE to enable.";
#endif
}

SpellChecker::~SpellChecker()
{
#ifdef HUNSPELL_AVAILABLE
    delete m_hunspell;
#endif
}

QString SpellChecker::correctText(const QString& text)
{
    // Without Hunspell, return text unchanged
    if (!isLoaded()) {
        return text;
    }

    // Split text into words while preserving delimiters
    QStringList words;
    QStringList delimiters;

    QString currentWord;
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text[i];
        if (c.isLetter() || c == '\'') {
            currentWord += c;
        } else {
            if (!currentWord.isEmpty()) {
                words.append(currentWord);
                currentWord.clear();
            }
            delimiters.append(QString(c));
        }
    }
    if (!currentWord.isEmpty()) {
        words.append(currentWord);
    }

    // Correct each word
    QString result;
    for (int i = 0; i < words.size(); ++i) {
        QString word = words[i];

        // Skip very short words (likely correct or proper nouns)
        if (word.length() <= 2) {
            result += word;
        } else if (!isCorrect(word)) {
            QString corrected = correctWord(word);
            result += corrected;
        } else {
            result += word;
        }

        // Add delimiter if exists
        if (i < delimiters.size()) {
            result += delimiters[i];
        }
    }

    return result;
}

bool SpellChecker::isCorrect(const QString& word)
{
#ifdef HUNSPELL_AVAILABLE
    if (!m_hunspell || word.isEmpty()) {
        return true;
    }

    QByteArray encoded = toHunspellEncoding(word);
    return m_hunspell->spell(encoded.constData()) != 0;
#else
    return true; // Without Hunspell, assume all words are correct
#endif
}

QStringList SpellChecker::suggest(const QString& word)
{
    QStringList result;

#ifdef HUNSPELL_AVAILABLE
    if (!m_hunspell || word.isEmpty()) {
        return result;
    }

    QByteArray encoded = toHunspellEncoding(word);
    std::vector<std::string> suggestions = m_hunspell->suggest(encoded.constData());

    for (const auto& s : suggestions) {
        result << fromHunspellEncoding(s.c_str());
    }
#endif

    return result;
}

QString SpellChecker::correctWord(const QString& word)
{
    QStringList suggestions = suggest(word);

    if (suggestions.isEmpty()) {
        qDebug() << "No suggestions for:" << word;
        return word; // No suggestions, keep original
    }

    // Return first suggestion (highest confidence)
    qDebug() << "Correcting:" << word << "→" << suggestions.first();
    return suggestions.first();
}

QByteArray SpellChecker::toHunspellEncoding(const QString& str)
{
    return str.toUtf8();
}

QString SpellChecker::fromHunspellEncoding(const char* str)
{
    return QString::fromUtf8(str);
}
