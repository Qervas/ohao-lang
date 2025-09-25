#include "GoogleWebTTSProvider.h"

#include <QBuffer>
#include <QtGlobal>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <initializer_list>

GoogleWebTTSProvider::GoogleWebTTSProvider(QObject* parent)
    : TTSProvider(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
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
    const auto makeList = [](std::initializer_list<const char*> voices) {
        QStringList list;
        for (const char* voice : voices) {
            list.append(QString::fromUtf8(voice));
        }
        return list;
    };

    switch (locale.language()) {
    case QLocale::English:
        if (locale.territory() == QLocale::India)
            return makeList({"English (IN) - Female", "English (IN) - Male", "English (IN) - News"});
        if (locale.territory() == QLocale::Australia)
            return makeList({"English (AU) - Warm"});
        return makeList({
            "English (US) - Female",
            "English (US) - Male",
            "English (US) - Narrator",
            "English (UK) - Female",
            "English (UK) - Male",
            "English (CA) - Friendly"
        });
    case QLocale::Japanese:
        return makeList({"日本語 - 女性", "日本語 - 男性"});
    case QLocale::Chinese:
        if (locale.script() == QLocale::TraditionalChineseScript || locale.territory() == QLocale::Taiwan)
            return makeList({"中文 (繁體) - 女聲", "中文 (繁體) - 男聲"});
        return makeList({"中文 (简体) - 女声", "中文 (简体) - 男声", "中文 (普通话) - 新闻"});
    case QLocale::Korean:
        return makeList({"한국어 - 여성", "한국어 - 남성"});
    case QLocale::Spanish:
        return makeList({"Español (ES) - Femenino", "Español (ES) - Masculino", "Español (MX) - Femenino"});
    case QLocale::French:
        return makeList({"Français (FR) - Femme", "Français (FR) - Homme", "Français (CA) - Femme"});
    case QLocale::German:
        return makeList({"Deutsch - Weiblich", "Deutsch - Männlich"});
    case QLocale::Italian:
        return makeList({"Italiano - Femmina", "Italiano - Maschio"});
    case QLocale::Portuguese:
        if (locale.territory() == QLocale::Brazil)
            return makeList({"Português (BR) - Feminino", "Português (BR) - Masculino"});
        return makeList({"Português (PT) - Feminino", "Português (PT) - Masculino"});
    case QLocale::Russian:
        return makeList({"Русский - Женский", "Русский - Мужской"});
    case QLocale::Arabic:
        return makeList({"العربية - أنثى", "العربية - ذكر"});
    case QLocale::Hindi:
        return makeList({"हिन्दी - महिला", "हिन्दी - पुरुष"});
    case QLocale::Thai:
        return makeList({"ไทย - หญิง", "ไทย - ชาย"});
    case QLocale::Swedish:
        return makeList({"Svenska - Kvinna", "Svenska - Man", "Svenska - Nyheter"});
    case QLocale::Vietnamese:
        return makeList({"Tiếng Việt - Nữ", "Tiếng Việt - Nam"});
    default:
        return makeList({"English (US) - Female", "English (US) - Male"});
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
                                 double /*volume*/)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    const QString language = !m_languageCode.isEmpty()
        ? m_languageCode
        : languageCodeForVoice(m_voice).isEmpty()
            ? languageCodeForLocale(locale)
            : languageCodeForVoice(m_voice);

    if (language.isEmpty()) {
        emit errorOccurred(QStringLiteral("Unable to determine language for Google voices."));
        return;
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
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray audio = reply->readAll();
        reply->deleteLater();

        if (audio.isEmpty()) {
            emit errorOccurred(QStringLiteral("Google Translate returned no audio data."));
            return;
        }

        auto *buffer = new QBuffer(this);
        buffer->setData(audio);
        buffer->open(QIODevice::ReadOnly);
        m_player->setSourceDevice(buffer);
        m_player->play();

        emit started();
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
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
    default: return QStringLiteral("en-US");
    }
}
