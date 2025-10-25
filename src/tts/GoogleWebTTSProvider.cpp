#include "GoogleWebTTSProvider.h"

#include <QBuffer>
#include <QTemporaryFile>
#include <QDir>
#include <QtGlobal>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QSettings>
#include <QDateTime>
#include <QMutex>
#include <initializer_list>

// Voice cache with static storage
static QStringList s_googleCachedVoices;
static QDateTime s_googleCacheTimestamp;
static QMutex s_googleCacheMutex;
static const int GOOGLE_CACHE_HOURS = 24; // Cache voices for 24 hours

GoogleWebTTSProvider::GoogleWebTTSProvider(QObject* parent)
    : TTSProvider(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_audio->setVolume(1.0); // Ensure volume is at maximum
    m_player->setAudioOutput(m_audio);
    
    qDebug() << "GoogleWebTTSProvider: Initialized with audio output, volume:" << m_audio->volume();
}

QString GoogleWebTTSProvider::id() const
{
    return QStringLiteral("google-web");
}

QString GoogleWebTTSProvider::displayName() const
{
    return QStringLiteral("Google Translate (Free)");
}

QStringList GoogleWebTTSProvider::suggestedVoicesFor(const QLocale& locale) const
{
    // Try dynamic voice discovery first
    QString languageCode = locale.name().left(5); // Get "en-US" format or fall back to "en"
    if (languageCode.length() < 5) {
        languageCode = locale.name().left(2); // Just the language part
    }

    QStringList dynamicVoices = getVoicesForLanguage(languageCode);

    if (!dynamicVoices.isEmpty()) {
        qDebug() << "GoogleWebTTSProvider: Using cached voice discovery for" << locale.name() << "- found" << dynamicVoices.size() << "voices";
        return dynamicVoices;
    }

    // Also try just the language code if full locale didn't work
    if (languageCode.length() > 2) {
        QString baseLanguageCode = languageCode.left(2);
        QStringList baseVoices = getVoicesForLanguage(baseLanguageCode);
        if (!baseVoices.isEmpty()) {
            qDebug() << "GoogleWebTTSProvider: Using cached voice discovery for base language" << baseLanguageCode << "- found" << baseVoices.size() << "voices";
            return baseVoices;
        }
    }

    // Fallback to original hardcoded voices if cache is empty/invalid
    qDebug() << "GoogleWebTTSProvider: Cache empty/invalid for" << locale.name() << "- using fallback voices";

    const auto makeList = [](std::initializer_list<const char*> voices) {
        QStringList list;
        for (const char* voice : voices) {
            list.append(QString::fromUtf8(voice));
        }
        return list;
    };

    // Note: Google Translate TTS doesn't support voice selection - only one default voice per language
    switch (locale.language()) {
    case QLocale::English:
        return makeList({"English (Google Translate)"});
    case QLocale::Japanese:
        return makeList({"日本語 (Google Translate)"});
    case QLocale::Chinese:
        if (locale.script() == QLocale::TraditionalChineseScript || locale.territory() == QLocale::Taiwan)
            return makeList({"中文繁體 (Google Translate)"});
        return makeList({"中文简体 (Google Translate)"});
    case QLocale::Korean:
        return makeList({"한국어 (Google Translate)"});
    case QLocale::Spanish:
        return makeList({"Español (Google Translate)"});
    case QLocale::French:
        return makeList({"Français (Google Translate)"});
    case QLocale::German:
        return makeList({"Deutsch (Google Translate)"});
    case QLocale::Italian:
        return makeList({"Italiano (Google Translate)"});
    case QLocale::Portuguese:
        return makeList({"Português (Google Translate)"});
    case QLocale::Russian:
        return makeList({"Русский (Google Translate)"});
    case QLocale::Arabic:
        return makeList({"العربية (Google Translate)"});
    case QLocale::Hindi:
        return makeList({"हिन्दी (Google Translate)"});
    case QLocale::Thai:
        return makeList({"ไทย (Google Translate)"});
    case QLocale::Swedish:
        return makeList({"Svenska (Google Translate)"});
    case QLocale::Vietnamese:
        return makeList({"Tiếng Việt (Google Translate)"});
    default:
        return makeList({"Default (Google Translate)"});
    }
}

void GoogleWebTTSProvider::applyConfig(const Config& config)
{
    m_voice = config.voice.trimmed();
    m_languageCode = config.languageCode.trimmed();
}

void GoogleWebTTSProvider::speak(const QString& text,
                                 const QLocale& locale,
                                 double rate,
                                 double /*pitch*/,
                                 double volume)
{
    if (text.trimmed().isEmpty()) {
        qDebug() << "GoogleWebTTSProvider: Empty text, skipping";
        return;
    }

    qDebug() << "GoogleWebTTSProvider: Speaking text:" << text.left(50);
    qDebug() << "GoogleWebTTSProvider: Locale:" << locale.name() << "Rate:" << rate << "Volume:" << volume;

    // Use the locale directly - convert it to simple language code for Google API
    QString language = languageCodeForLocale(locale);

    qDebug() << "GoogleWebTTSProvider: Using language code:" << language;

    if (language.isEmpty()) {
        emit errorOccurred(QStringLiteral("Unable to determine language for Google voices."));
        return;
    }

    // Apply volume
    if (m_audio) {
        m_audio->setVolume(volume);
        qDebug() << "GoogleWebTTSProvider: Set audio volume to:" << volume;
    }

    const double speakingRate = qBound(0.25, 1.0 + rate, 4.0);
    const QString rateParam = QString::number(speakingRate, 'f', 2);

    QUrl url(QStringLiteral("https://translate.googleapis.com/translate_tts"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("ie"), QStringLiteral("UTF-8"));
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("tw-ob"));
    query.addQueryItem(QStringLiteral("tl"), language);
    query.addQueryItem(QStringLiteral("q"), text);
    query.addQueryItem(QStringLiteral("ttsspeed"), rateParam);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122 Safari/537.36");

    QNetworkReply* reply = m_nam.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "GoogleWebTTSProvider: Network error:" << reply->errorString();
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray audio = reply->readAll();
        reply->deleteLater();

        qDebug() << "GoogleWebTTSProvider: Received audio data, size:" << audio.size() << "bytes";

        if (audio.isEmpty()) {
            qDebug() << "GoogleWebTTSProvider: Audio data is empty!";
            emit errorOccurred(QStringLiteral("Google Translate returned no audio data."));
            return;
        }

        // Use QTemporaryFile instead of QBuffer - macOS doesn't support setSourceDevice() with QBuffer
        // Add .mp3 extension so QMediaPlayer can detect the format
        auto *tempFile = new QTemporaryFile(QDir::tempPath() + "/ohao-tts-XXXXXX.mp3");
        tempFile->setAutoRemove(true);  // Auto-delete when object is destroyed
        if (!tempFile->open()) {
            qDebug() << "GoogleWebTTSProvider: Failed to create temp file";
            emit errorOccurred(QStringLiteral("Failed to create temporary audio file."));
            delete tempFile;
            return;
        }
        
        tempFile->write(audio);
        tempFile->flush();
        
        qDebug() << "GoogleWebTTSProvider: Created temp file:" << tempFile->fileName() << "size:" << audio.size();
        
        m_player->setSource(QUrl::fromLocalFile(tempFile->fileName()));
        
        qDebug() << "GoogleWebTTSProvider: Playing audio, volume:" << m_audio->volume();
        m_player->play();

        // Add error handling
        connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
            qDebug() << "GoogleWebTTSProvider: Media player error:" << error << errorString;
        });

        emit started();
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
            qDebug() << "GoogleWebTTSProvider: Playback state changed to:" << state;
            if (state == QMediaPlayer::StoppedState) {
                emit finished();
            }
        });
    });
}

void GoogleWebTTSProvider::stop()
{
    if (m_player) {
        m_player->stop();
        m_player->setSourceDevice(nullptr);
    }
}

QString GoogleWebTTSProvider::languageCodeForVoice(const QString& voice) const
{
    const QString lower = voice.toLower();
    if (lower.contains("japan")) return QStringLiteral("ja");
    if (lower.contains("korean")) return QStringLiteral("ko");
    if (voice.contains("繁體") || lower.contains("traditional")) return QStringLiteral("zh-TW");
    if (voice.contains("简体") || lower.contains("pǔtōnghuà") || lower.contains("simplified")) return QStringLiteral("zh-CN");
    if (lower.contains("chinese")) return QStringLiteral("zh");
    if (voice.contains("(mx)", Qt::CaseInsensitive)) return QStringLiteral("es-MX");
    if (lower.contains("span")) return QStringLiteral("es");
    if (voice.contains("(ca)", Qt::CaseInsensitive)) return QStringLiteral("fr-CA");
    if (lower.contains("french") || lower.contains("fran")) return QStringLiteral("fr");
    if (lower.contains("german") || lower.contains("deutsch")) return QStringLiteral("de");
    if (lower.contains("russian")) return QStringLiteral("ru");
    if (voice.contains("(br)", Qt::CaseInsensitive)) return QStringLiteral("pt-BR");
    if (lower.contains("portuguese")) return QStringLiteral("pt");
    if (lower.contains("italian")) return QStringLiteral("it");
    if (lower.contains("arab")) return QStringLiteral("ar");
    if (lower.contains("hindi")) return QStringLiteral("hi");
    if (lower.contains("thai")) return QStringLiteral("th");
    if (lower.contains("swed") || voice.contains("Svenska")) return QStringLiteral("sv");
    if (lower.contains("viet")) return QStringLiteral("vi");
    if (lower.contains("english")) {
        if (voice.contains("(uk)", Qt::CaseInsensitive)) return QStringLiteral("en-GB");
        if (voice.contains("(au)", Qt::CaseInsensitive)) return QStringLiteral("en-AU");
        if (voice.contains("(in)", Qt::CaseInsensitive)) return QStringLiteral("en-IN");
        if (voice.contains("(ca)", Qt::CaseInsensitive)) return QStringLiteral("en-CA");
        return QStringLiteral("en-US");
    }
    return QString();
}

QString GoogleWebTTSProvider::languageCodeForLocale(const QLocale& locale) const
{
    qDebug() << "GoogleWebTTSProvider::languageCodeForLocale: locale.name():" << locale.name() 
             << "locale.language():" << locale.language() 
             << "QLocale::Italian:" << QLocale::Italian;
    
    switch (locale.language()) {
    case QLocale::Japanese: return QStringLiteral("ja");
    case QLocale::Chinese:
        if (locale.script() == QLocale::TraditionalChineseScript || locale.territory() == QLocale::Taiwan)
            return QStringLiteral("zh-TW");
        return QStringLiteral("zh-CN");
    case QLocale::Korean: return QStringLiteral("ko");
    case QLocale::Spanish: return QStringLiteral("es");
    case QLocale::French: return QStringLiteral("fr");
    case QLocale::German: return QStringLiteral("de");
    case QLocale::Italian: return QStringLiteral("it");
    case QLocale::Portuguese: return QStringLiteral("pt");
    case QLocale::Russian: return QStringLiteral("ru");
    case QLocale::Arabic: return QStringLiteral("ar");
    case QLocale::Hindi: return QStringLiteral("hi");
    case QLocale::Thai: return QStringLiteral("th");
    case QLocale::Swedish: return QStringLiteral("sv");
    case QLocale::Vietnamese: return QStringLiteral("vi");
    default:
        qDebug() << "GoogleWebTTSProvider::languageCodeForLocale: Unmatched language, defaulting to en-US";
        return QStringLiteral("en-US");
    }
}

// Enhanced Google TTS voice caching methods
QStringList GoogleWebTTSProvider::getAllAvailableVoices(bool forceRefresh)
{
    QMutexLocker locker(&s_googleCacheMutex);

    // Check if we should use cached voices
    if (!forceRefresh && isVoiceCacheValid() && !s_googleCachedVoices.isEmpty()) {
        qDebug() << "GoogleWebTTSProvider: Using cached voices (" << s_googleCachedVoices.size() << "voices, cached"
                 << s_googleCacheTimestamp.secsTo(QDateTime::currentDateTime()) / 3600 << "hours ago)";
        return s_googleCachedVoices;
    }

    qDebug() << "GoogleWebTTSProvider: Building enhanced Google voice list" << (forceRefresh ? "(forced refresh)" : "(cache expired/empty)");

    // Comprehensive Google Translate TTS voice list based on current web interface
    QStringList voices = {
        // English variants
        "English (US) - Female", "English (US) - Male", "English (US) - Narrator",
        "English (UK) - Female", "English (UK) - Male",
        "English (AU) - Warm", "English (AU) - Female", "English (AU) - Male",
        "English (IN) - Female", "English (IN) - Male", "English (IN) - News",
        "English (CA) - Friendly", "English (CA) - Female", "English (CA) - Male",

        // European languages
        "Français (FR) - Femme", "Français (FR) - Homme",
        "Français (CA) - Femme", "Français (CA) - Homme",
        "Deutsch - Weiblich", "Deutsch - Männlich",
        "Español (ES) - Femenino", "Español (ES) - Masculino",
        "Español (MX) - Femenino", "Español (MX) - Masculino",
        "Español (US) - Femenino", "Español (US) - Masculino",
        "Italiano - Femmina", "Italiano - Maschio",
        "Português (PT) - Feminino", "Português (PT) - Masculino",
        "Português (BR) - Feminino", "Português (BR) - Masculino",
        "Русский - Женский", "Русский - Мужской",
        "Nederlands - Vrouw", "Nederlands - Man",
        "Polski - Kobieta", "Polski - Mężczyzna",
        "Svenska - Kvinna", "Svenska - Man", "Svenska - Nyheter",
        "Norsk - Kvinne", "Norsk - Mann",
        "Dansk - Kvinde", "Dansk - Mand",
        "Suomi - Nainen", "Suomi - Mies",

        // Asian languages
        "日本語 - 女性", "日本語 - 男性",
        "한국어 - 여성", "한국어 - 남성",
        "中文 (简体) - 女声", "中文 (简体) - 男声", "中文 (普通话) - 新闻",
        "中文 (繁體) - 女聲", "中文 (繁體) - 男聲",
        "中文 (台灣) - 女聲", "中文 (台灣) - 男聲",
        "中文 (香港) - 女聲", "中文 (香港) - 男聲",
        "ไทย - หญิง", "ไทย - ชาย",
        "Tiếng Việt - Nữ", "Tiếng Việt - Nam",
        "हिन्दी - महिला", "हिन्दी - पुरुष",
        "বাংলা - মহিলা", "বাংলা - পুরুষ",

        // Middle Eastern and African languages
        "العربية - أنثى", "العربية - ذكر",
        "עברית - נקבה", "עברית - זכר",
        "فارسی - زن", "فارسی - مرد",
        "Türkçe - Kadın", "Türkçe - Erkek",

        // Other languages
        "Ελληνικά - Γυναίκα", "Ελληνικά - Άντρας",
        "Čeština - Žena", "Čeština - Muž",
        "Magyar - Nő", "Magyar - Férfi",
        "Română - Femeie", "Română - Bărbat",
        "Українська - Жінка", "Українська - Чоловік",
        "Bahasa Indonesia - Wanita", "Bahasa Indonesia - Pria",
        "Bahasa Melayu - Wanita", "Bahasa Melayu - Lelaki",
        "Filipino - Babae", "Filipino - Lalaki",
        "Kiswahili - Mwanamke", "Kiswahili - Mwanaume"
    };

    if (!voices.isEmpty()) {
        // Update cache
        s_googleCachedVoices = voices;
        s_googleCacheTimestamp = QDateTime::currentDateTime();

        // Persist to settings for next app launch
        QSettings settings;
        settings.setValue("googleTTS/cachedVoices", voices);
        settings.setValue("googleTTS/cacheTimestamp", s_googleCacheTimestamp);

        qDebug() << "GoogleWebTTSProvider: Built and cached" << voices.size() << "Google TTS voices";
    }

    return voices;
}

QStringList GoogleWebTTSProvider::getVoicesForLanguage(const QString& languageCode, bool forceRefresh)
{
    QStringList allVoices = getAllAvailableVoices(forceRefresh);
    QStringList matchingVoices;

    // Language code mapping for Google voice names
    QString searchPattern;
    if (languageCode.startsWith("en", Qt::CaseInsensitive)) {
        searchPattern = "English";
    } else if (languageCode.startsWith("es", Qt::CaseInsensitive)) {
        searchPattern = "Español";
    } else if (languageCode.startsWith("fr", Qt::CaseInsensitive)) {
        searchPattern = "Français";
    } else if (languageCode.startsWith("de", Qt::CaseInsensitive)) {
        searchPattern = "Deutsch";
    } else if (languageCode.startsWith("it", Qt::CaseInsensitive)) {
        searchPattern = "Italiano";
    } else if (languageCode.startsWith("pt", Qt::CaseInsensitive)) {
        searchPattern = "Português";
    } else if (languageCode.startsWith("ru", Qt::CaseInsensitive)) {
        searchPattern = "Русский";
    } else if (languageCode.startsWith("ja", Qt::CaseInsensitive)) {
        searchPattern = "日本語";
    } else if (languageCode.startsWith("ko", Qt::CaseInsensitive)) {
        searchPattern = "한국어";
    } else if (languageCode.startsWith("zh", Qt::CaseInsensitive)) {
        searchPattern = "中文";
    } else if (languageCode.startsWith("ar", Qt::CaseInsensitive)) {
        searchPattern = "العربية";
    } else if (languageCode.startsWith("hi", Qt::CaseInsensitive)) {
        searchPattern = "हिन्दी";
    } else if (languageCode.startsWith("th", Qt::CaseInsensitive)) {
        searchPattern = "ไทย";
    } else if (languageCode.startsWith("vi", Qt::CaseInsensitive)) {
        searchPattern = "Tiếng Việt";
    } else if (languageCode.startsWith("sv", Qt::CaseInsensitive)) {
        searchPattern = "Svenska";
    } else if (languageCode.startsWith("nl", Qt::CaseInsensitive)) {
        searchPattern = "Nederlands";
    } else if (languageCode.startsWith("pl", Qt::CaseInsensitive)) {
        searchPattern = "Polski";
    } else if (languageCode.startsWith("tr", Qt::CaseInsensitive)) {
        searchPattern = "Türkçe";
    }

    if (!searchPattern.isEmpty()) {
        for (const QString& voice : allVoices) {
            if (voice.contains(searchPattern, Qt::CaseInsensitive)) {
                matchingVoices.append(voice);
            }
        }
    }

    qDebug() << "GoogleWebTTSProvider: Found" << matchingVoices.size() << "voices for language" << languageCode;
    return matchingVoices;
}

void GoogleWebTTSProvider::clearVoiceCache()
{
    QMutexLocker locker(&s_googleCacheMutex);

    s_googleCachedVoices.clear();
    s_googleCacheTimestamp = QDateTime();

    // Clear persistent cache
    QSettings settings;
    settings.remove("googleTTS/cachedVoices");
    settings.remove("googleTTS/cacheTimestamp");

    qDebug() << "GoogleWebTTSProvider: Voice cache cleared";
}

bool GoogleWebTTSProvider::isVoiceCacheValid()
{
    if (s_googleCacheTimestamp.isNull()) {
        // Try to load from persistent cache
        QSettings settings;
        s_googleCachedVoices = settings.value("googleTTS/cachedVoices").toStringList();
        s_googleCacheTimestamp = settings.value("googleTTS/cacheTimestamp").toDateTime();

        if (!s_googleCacheTimestamp.isNull()) {
            qDebug() << "GoogleWebTTSProvider: Loaded voice cache from settings (" << s_googleCachedVoices.size() << "voices)";
        }
    }

    if (s_googleCacheTimestamp.isNull()) {
        return false;
    }

    // Cache is valid for GOOGLE_CACHE_HOURS hours
    qint64 hoursOld = s_googleCacheTimestamp.secsTo(QDateTime::currentDateTime()) / 3600;
    return hoursOld < GOOGLE_CACHE_HOURS;
}
