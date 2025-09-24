#include "CloudTTSProvider.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDateTime>
#include <QProcess>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>

CloudTTSProvider::CloudTTSProvider(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    connect(&m_nam, &QNetworkAccessManager::finished, this, &CloudTTSProvider::onNetworkFinished);
}

void CloudTTSProvider::setBackend(Backend b) { m_backend = b; }
CloudTTSProvider::Backend CloudTTSProvider::backend() const { return m_backend; }

void CloudTTSProvider::setAzureConfig(const QString& region, const QString& apiKey, const QString& voiceName, const QString& style)
{
    m_azureRegion = region.trimmed();
    m_azureKey = apiKey.trimmed();
    m_azureVoice = voiceName.trimmed();
    m_azureStyle = style.trimmed();
}

void CloudTTSProvider::setGoogleConfig(const QString& apiKey, const QString& voiceName, const QString& languageCode)
{
    m_googleApiKey = apiKey.trimmed();
    m_googleVoice = voiceName.trimmed();
    m_googleLanguageCode = languageCode.trimmed();
}

void CloudTTSProvider::setElevenLabsConfig(const QString& apiKey, const QString& voiceId)
{
    m_elevenApiKey = apiKey.trimmed();
    m_elevenVoiceId = voiceId.trimmed();
}

void CloudTTSProvider::setPollyConfig(const QString& region, const QString& accessKey, const QString& secretKey, const QString& voiceName)
{
    m_pollyRegion = region.trimmed();
    m_pollyAccessKey = accessKey.trimmed();
    m_pollySecretKey = secretKey.trimmed();
    m_pollyVoice = voiceName.trimmed();
}

void CloudTTSProvider::setPiperConfig(const QString& exePath, const QString& modelPath)
{
    m_piperExePath = exePath.trimmed();
    m_piperModelPath = modelPath.trimmed();
}

void CloudTTSProvider::setEdgeConfig(const QString& exePath, const QString& voiceName)
{
    m_edgeExePath = exePath.trimmed();
    m_edgeVoice = voiceName.trimmed();
}

void CloudTTSProvider::setGoogleFreeConfig(const QString& voiceName)
{
    m_googleFreeVoice = voiceName.trimmed();
}

QStringList CloudTTSProvider::azureSuggestedVoicesFor(const QLocale& locale)
{
    // Minimal static mapping of popular neural voices by locale
    // Users can override by typing a specific voice name
    switch (locale.language()) {
    case QLocale::English:
        if (locale.country() == QLocale::UnitedStates) return {"en-US-JennyNeural", "en-US-AriaNeural", "en-US-GuyNeural"};
        if (locale.country() == QLocale::UnitedKingdom) return {"en-GB-LibbyNeural", "en-GB-RyanNeural"};
        return {"en-US-JennyNeural"};
    case QLocale::Chinese:
        if (locale.script() == QLocale::SimplifiedChineseScript || locale.country() == QLocale::China)
            return {"zh-CN-XiaoxiaoNeural", "zh-CN-YunxiNeural"};
        return {"zh-TW-HsiaoChenNeural", "zh-HK-HiuMaanNeural"};
    case QLocale::Japanese:
        return {"ja-JP-NanamiNeural", "ja-JP-KeitaNeural"};
    case QLocale::Korean:
        return {"ko-KR-SunHiNeural", "ko-KR-InJoonNeural"};
    case QLocale::Spanish:
        return {"es-ES-ElviraNeural", "es-ES-AlvaroNeural"};
    case QLocale::French:
        return {"fr-FR-DeniseNeural", "fr-FR-HenriNeural"};
    case QLocale::German:
        return {"de-DE-KatjaNeural", "de-DE-ConradNeural"};
    case QLocale::Swedish:
        return {"sv-SE-SofieNeural", "sv-SE-MattiasNeural", "sv-SE-HilleviNeural"};
    default:
        return {"en-US-JennyNeural"};
    }
}

QByteArray CloudTTSProvider::buildAzureSSML(const QString& text, const QString& voiceName, const QString& style, double rate, double pitch, double volume) const
{
    // Convert normalized controls to Azure values
    // rate, pitch in [-1,1], volume in [0,1]
    const int ratePct = static_cast<int>(rate * 100);
    const int pitchPct = static_cast<int>(pitch * 100);
    const int volPct = static_cast<int>(volume * 100);

    QString prosody = QString("rate:%1%; pitch:%2%; volume:%3%")
        .arg(ratePct >= 0 ? "+" + QString::number(ratePct) : QString::number(ratePct))
        .arg(pitchPct >= 0 ? "+" + QString::number(pitchPct) : QString::number(pitchPct))
        .arg(qBound(0, volPct, 100));

    QString ssml = QString(
        "<speak version=\"1.0\" xml:lang=\"en-US\">"
        "<voice name=\"%1\">"
        "%2%3%4"
        "</voice>"
        "</speak>")
        .arg(voiceName)
        .arg(style.isEmpty() ? "" : QString("<mstts:express-as style=\"%1\">\n").arg(style))
        .arg(QString("<prosody rate=\"%1\" pitch=\"%2\" volume=\"%3\">%4</prosody>\n").arg(
                 prosody.split(';').at(0).trimmed().mid(QString("rate:").length()),
                 prosody.split(';').at(1).trimmed().mid(QString(" pitch:").length()),
                 prosody.split(';').at(2).trimmed().mid(QString(" volume:").length()),
                 text.toHtmlEscaped()))
        .arg(style.isEmpty() ? "" : "</mstts:express-as>\n");

    // Add namespaces
    ssml.prepend("<speak xmlns=\"http://www.w3.org/2001/10/synthesis\" xmlns:mstts=\"https://www.w3.org/2001/mstts\" version=\"1.0\">\n");
    return ssml.toUtf8();
}

void CloudTTSProvider::speak(const QString& text, const QLocale& /*locale*/, double rate, double pitch, double volume)
{
    if (m_backend == Backend::Azure) {
        if (m_azureRegion.isEmpty() || m_azureKey.isEmpty() || m_azureVoice.isEmpty()) {
            emit errorOccurred("Azure TTS not configured. Set region, key, and voice.");
            return;
        }

        const QUrl url(QString("https://%1.tts.speech.microsoft.com/cognitiveservices/v1").arg(m_azureRegion));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/ssml+xml");
        req.setRawHeader("Ocp-Apim-Subscription-Key", m_azureKey.toUtf8());
        req.setRawHeader("X-Microsoft-OutputFormat", "audio-16khz-32kbitrate-mono-mp3");
        req.setRawHeader("User-Agent", "ohao-lang/1.0");

        QByteArray ssml = buildAzureSSML(text, m_azureVoice, m_azureStyle, rate, pitch, volume);
        auto* reply = m_nam.post(req, ssml);
        Q_UNUSED(reply);
        emit started();
        return;
    }

    if (m_backend == Backend::Google) { postGoogle(text, QLocale(), rate, pitch, volume); emit started(); return; }
    if (m_backend == Backend::GoogleFree) { postGoogleFree(text, QLocale()); emit started(); return; }
    if (m_backend == Backend::ElevenLabs) { postElevenLabs(text, rate, pitch, volume); emit started(); return; }
    if (m_backend == Backend::Polly) { postPolly(text, QLocale(), rate, pitch, volume); emit started(); return; }
    if (m_backend == Backend::Piper) { postPiper(text); emit started(); return; }
    if (m_backend == Backend::Edge) { postEdge(text, QLocale()); emit started(); return; }

    emit errorOccurred("No cloud TTS backend selected");
}

void CloudTTSProvider::postPiper(const QString& text)
{
    if (m_piperExePath.isEmpty() || m_piperModelPath.isEmpty()) {
        emit errorOccurred("Piper not configured. Set executable and model.");
        return;
    }
    // Write text to temp file
    QTemporaryFile txtFile(QDir::tempPath() + "/piper-XXXXXX.txt");
    if (!txtFile.open()) { emit errorOccurred("Failed to create temp file for Piper."); return; }
    txtFile.write(text.toUtf8());
    txtFile.flush();
    txtFile.close();

    // Output wav
    QTemporaryFile wavFile(QDir::tempPath() + "/piper-XXXXXX.wav");
    if (!wavFile.open()) { emit errorOccurred("Failed to create temp wav for Piper."); return; }
    const QString outPath = wavFile.fileName();
    wavFile.close();

    QStringList args;
    args << "--model" << m_piperModelPath << "--output_file" << outPath << "--text_file" << txtFile.fileName();
    QProcess proc;
    proc.start(m_piperExePath, args);
    if (!proc.waitForStarted(30000)) { emit errorOccurred("Failed to start Piper process."); return; }
    proc.waitForFinished(60000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        emit errorOccurred(QString("Piper TTS failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }

    QFile f(outPath);
    if (!f.open(QIODevice::ReadOnly)) { emit errorOccurred("Piper output missing."); return; }
    QByteArray audio = f.readAll();
    f.close();

    QBuffer* buf = new QBuffer(this);
    buf->setData(audio);
    buf->open(QIODevice::ReadOnly);
    m_player->setSourceDevice(buf);
    m_player->play();
}

void CloudTTSProvider::postGoogleFree(const QString& text, const QLocale& locale)
{
    // Unofficial: Google Translate TTS
    QString tl = locale.name().left(2);
    if (tl.isEmpty()) tl = "en";
    
    // Use the selected voice to determine language code if available
    if (!m_googleFreeVoice.isEmpty()) {
        if (m_googleFreeVoice.contains("English", Qt::CaseInsensitive)) tl = "en";
        else if (m_googleFreeVoice.contains("Chinese", Qt::CaseInsensitive)) {
            if (m_googleFreeVoice.contains("Simplified") || m_googleFreeVoice.contains("Mandarin")) tl = "zh-cn";
            else if (m_googleFreeVoice.contains("Traditional") || m_googleFreeVoice.contains("Cantonese")) tl = "zh-tw";
            else tl = "zh";
        }
        else if (m_googleFreeVoice.contains("Japanese", Qt::CaseInsensitive)) tl = "ja";
        else if (m_googleFreeVoice.contains("Korean", Qt::CaseInsensitive)) tl = "ko";
        else if (m_googleFreeVoice.contains("Spanish", Qt::CaseInsensitive)) tl = "es";
        else if (m_googleFreeVoice.contains("French", Qt::CaseInsensitive)) tl = "fr";
        else if (m_googleFreeVoice.contains("German", Qt::CaseInsensitive)) tl = "de";
        else if (m_googleFreeVoice.contains("Russian", Qt::CaseInsensitive)) tl = "ru";
        else if (m_googleFreeVoice.contains("Portuguese", Qt::CaseInsensitive)) tl = "pt";
        else if (m_googleFreeVoice.contains("Italian", Qt::CaseInsensitive)) tl = "it";
        else if (m_googleFreeVoice.contains("Dutch", Qt::CaseInsensitive)) tl = "nl";
        else if (m_googleFreeVoice.contains("Polish", Qt::CaseInsensitive)) tl = "pl";
        else if (m_googleFreeVoice.contains("Swedish", Qt::CaseInsensitive)) tl = "sv";
        else if (m_googleFreeVoice.contains("Arabic", Qt::CaseInsensitive)) tl = "ar";
    }
    
    QUrl url("https://translate.googleapis.com/translate_tts");
    QUrlQuery q;
    q.addQueryItem("ie", "UTF-8");
    q.addQueryItem("client", "tw-ob");
    q.addQueryItem("tl", tl);
    q.addQueryItem("q", text);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    auto* reply = m_nam.get(req);
    Q_UNUSED(reply);
}

void CloudTTSProvider::postEdge(const QString& text, const QLocale& locale)
{
    if (m_edgeExePath.isEmpty()) { emit errorOccurred("Edge TTS not configured"); return; }
    QString lang = locale.name().left(2);
    if (lang.isEmpty()) lang = "en";
    QTemporaryFile wavFile(QDir::tempPath() + "/edge-XXXXXX.mp3");
    if (!wavFile.open()) { emit errorOccurred("Failed to create temp output for Edge TTS."); return; }
    const QString outPath = wavFile.fileName();
    wavFile.close();
    QStringList args;
    if (!m_edgeVoice.isEmpty()) { args << "--voice" << m_edgeVoice; }
    args << "--text" << text << "--write-media" << outPath;
    QProcess proc;
    proc.start(m_edgeExePath, args);
    if (!proc.waitForStarted(30000)) { emit errorOccurred("Failed to start Edge TTS."); return; }
    proc.waitForFinished(60000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        emit errorOccurred(QString("Edge TTS failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }

    QFile f(outPath);
    if (!f.open(QIODevice::ReadOnly)) { emit errorOccurred("Edge TTS output missing."); return; }
    QByteArray audio = f.readAll();
    f.close();

    QBuffer* buf = new QBuffer(this);
    buf->setData(audio);
    buf->open(QIODevice::ReadOnly);
    m_player->setSourceDevice(buf);
    m_player->play();
}

void CloudTTSProvider::onNetworkFinished(QNetworkReply* reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(QString("Cloud TTS request failed: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    m_lastAudio = reply->readAll();
    reply->deleteLater();

    // Play from buffer
    QBuffer* buf = new QBuffer(this);
    buf->setData(m_lastAudio);
    buf->open(QIODevice::ReadOnly);

    m_player->setSourceDevice(buf);
    m_player->play();

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s){
        if (s == QMediaPlayer::StoppedState) emit finished();
    });
}

void CloudTTSProvider::stop()
{
    if (m_player) {
        m_player->stop();
    }
}

// ---------- Suggestions ----------
QStringList CloudTTSProvider::googleSuggestedVoicesFor(const QLocale& locale)
{
    switch (locale.language()) {
    case QLocale::English:
        return {"en-US-Neural2-J", "en-US-Neural2-F", "en-GB-Neural2-A"};
    case QLocale::Japanese:
        return {"ja-JP-Neural2-B", "ja-JP-Neural2-C"};
    case QLocale::Chinese:
        return {"cmn-CN-Neural2-A", "cmn-TW-Neural2-A"};
    case QLocale::Korean:
        return {"ko-KR-Neural2-A"};
    case QLocale::Spanish:
        return {"es-ES-Neural2-B", "es-US-Neural2-A"};
    default:
        return {"en-US-Neural2-J"};
    }
}

QStringList CloudTTSProvider::googleFreeSuggestedVoicesFor(const QLocale& locale)
{
    // Google Translate TTS uses simple language codes but has limited voice variety
    switch (locale.language()) {
    case QLocale::English:
        return {"English (US)", "English (UK)", "English (AU)", "English (IN)"};
    case QLocale::Japanese:
        return {"日本語 (Japanese)"};
    case QLocale::Chinese:
        return {"中文 (Chinese - Simplified)", "中文 (Chinese - Traditional)"};
    case QLocale::Korean:
        return {"한국어 (Korean)"};
    case QLocale::Spanish:
        return {"Español (Spanish - Spain)", "Español (Spanish - Mexico)"};
    case QLocale::French:
        return {"Français (French)"};
    case QLocale::German:
        return {"Deutsch (German)"};
    case QLocale::Italian:
        return {"Italiano (Italian)"};
    case QLocale::Portuguese:
        return {"Português (Portuguese)"};
    case QLocale::Russian:
        return {"Русский (Russian)"};
    case QLocale::Arabic:
        return {"العربية (Arabic)"};
    case QLocale::Hindi:
        return {"हिन्दी (Hindi)"};
    case QLocale::Swedish:
        return {"Svenska (Swedish)"};
    default:
        return {"English (US)"};
    }
}

QStringList CloudTTSProvider::edgeSuggestedVoicesFor(const QLocale& locale)
{
    // Microsoft Edge TTS has many high-quality neural voices
    switch (locale.language()) {
    case QLocale::English:
        if (locale.country() == QLocale::UnitedStates) 
            return {"en-US-AriaNeural", "en-US-JennyNeural", "en-US-GuyNeural", "en-US-DavisNeural", "en-US-AmberNeural"};
        if (locale.country() == QLocale::UnitedKingdom)
            return {"en-GB-SoniaNeural", "en-GB-RyanNeural", "en-GB-LibbyNeural"};
        return {"en-US-AriaNeural", "en-US-JennyNeural", "en-GB-SoniaNeural"};
    case QLocale::Chinese:
        if (locale.script() == QLocale::SimplifiedChineseScript || locale.country() == QLocale::China)
            return {"zh-CN-XiaoxiaoNeural", "zh-CN-YunxiNeural", "zh-CN-YunjianNeural", "zh-CN-XiaoyiNeural"};
        return {"zh-TW-HsiaoChenNeural", "zh-TW-YunJheNeural", "zh-HK-HiuMaanNeural", "zh-HK-WanLungNeural"};
    case QLocale::Japanese:
        return {"ja-JP-NanamiNeural", "ja-JP-KeitaNeural", "ja-JP-AoiNeural", "ja-JP-DaichiNeural"};
    case QLocale::Korean:
        return {"ko-KR-SunHiNeural", "ko-KR-InJoonNeural", "ko-KR-BongJinNeural", "ko-KR-GookMinNeural"};
    case QLocale::Spanish:
        return {"es-ES-ElviraNeural", "es-ES-AlvaroNeural", "es-MX-DaliaNeural", "es-MX-JorgeNeural"};
    case QLocale::French:
        return {"fr-FR-DeniseNeural", "fr-FR-HenriNeural", "fr-CA-SylvieNeural", "fr-CA-AntoineNeural"};
    case QLocale::German:
        return {"de-DE-KatjaNeural", "de-DE-ConradNeural", "de-AT-IngridNeural", "de-CH-LeniNeural"};
    case QLocale::Italian:
        return {"it-IT-ElsaNeural", "it-IT-IsabellaNeural", "it-IT-DiegoNeural"};
    case QLocale::Portuguese:
        return {"pt-BR-FranciscaNeural", "pt-BR-AntonioNeural", "pt-PT-RaquelNeural"};
    case QLocale::Russian:
        return {"ru-RU-SvetlanaNeural", "ru-RU-DmitryNeural"};
    case QLocale::Arabic:
        return {"ar-SA-ZariyahNeural", "ar-SA-HamedNeural"};
    case QLocale::Hindi:
        return {"hi-IN-SwaraNeural", "hi-IN-MadhurNeural"};
    case QLocale::Thai:
        return {"th-TH-AcharaNeural", "th-TH-NiwatNeural"};
    case QLocale::Swedish:
        return {"sv-SE-SofieNeural", "sv-SE-MattiasNeural", "sv-SE-HilleviNeural"};
    default:
        return {"en-US-AriaNeural", "en-US-JennyNeural"};
    }
}

QStringList CloudTTSProvider::pollySuggestedVoicesFor(const QLocale& locale)
{
    switch (locale.language()) {
    case QLocale::English:
        return {"Joanna", "Matthew", "Amy"};
    case QLocale::Japanese:
        return {"Mizuki", "Takumi"};
    case QLocale::Korean:
        return {"Seoyeon"};
    case QLocale::Spanish:
        return {"Lucia", "Miguel"};
    case QLocale::Chinese:
        return {"Zhiyu"};
    case QLocale::Swedish:
        return {"Astrid"};
    default:
        return {"Joanna"};
    }
}

// ---------- Posts ----------
void CloudTTSProvider::postGoogle(const QString& text, const QLocale& locale, double rate, double pitch, double volume)
{
    if (m_googleApiKey.isEmpty() || (m_googleVoice.isEmpty() && locale.name().isEmpty())) {
        emit errorOccurred("Google TTS not configured. Set API key and voice.");
        return;
    }
    const QString voiceName = m_googleVoice;
    const QString languageCode = !m_googleLanguageCode.isEmpty() ? m_googleLanguageCode : QString("%1-%2").arg(QLocale::languageToString(locale.language())).arg(QLocale::countryToString(locale.country()));
    const QUrl url(QString("https://texttospeech.googleapis.com/v1/text:synthesize?key=%1").arg(m_googleApiKey));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const double speakingRate = qBound(0.25, 1.0 + rate, 4.0);
    const double pitchSemitones = qBound(-20.0, pitch * 20.0, 20.0);
    const double volGainDb = qBound(-96.0, (volume - 1.0) * 20.0, 16.0);

    QJsonObject input{{"text", text}};
    QJsonObject voice{{"name", voiceName}, {"languageCode", languageCode}};
    QJsonObject audioConfig{{"audioEncoding", "MP3"}, {"speakingRate", speakingRate}, {"pitch", pitchSemitones}, {"volumeGainDb", volGainDb}};
    QJsonObject body{{"input", input}, {"voice", voice}, {"audioConfig", audioConfig}};

    auto* reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) { emit errorOccurred(reply->errorString()); reply->deleteLater(); return; }
        const auto json = QJsonDocument::fromJson(reply->readAll()).object();
        const QByteArray audio = QByteArray::fromBase64(json.value("audioContent").toString().toUtf8());
        QBuffer* buf = new QBuffer(this);
        buf->setData(audio);
        buf->open(QIODevice::ReadOnly);
        m_player->setSourceDevice(buf);
        m_player->play();
        reply->deleteLater();
    });
}

void CloudTTSProvider::postElevenLabs(const QString& text, double rate, double pitch, double volume)
{
    Q_UNUSED(rate); Q_UNUSED(pitch); Q_UNUSED(volume);
    if (m_elevenApiKey.isEmpty() || m_elevenVoiceId.isEmpty()) { emit errorOccurred("ElevenLabs not configured."); return; }
    const QUrl url(QString("https://api.elevenlabs.io/v1/text-to-speech/%1").arg(m_elevenVoiceId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("xi-api-key", m_elevenApiKey.toUtf8());
    QJsonObject body{{"text", text}, {"voice_settings", QJsonObject{{"stability", 0.5}, {"similarity_boost", 0.75}}}};
    auto* reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) { emit errorOccurred(reply->errorString()); reply->deleteLater(); return; }
        QByteArray audio = reply->readAll();
        QBuffer* buf = new QBuffer(this);
        buf->setData(audio);
        buf->open(QIODevice::ReadOnly);
        m_player->setSourceDevice(buf);
        m_player->play();
        reply->deleteLater();
    });
}

// Minimal SigV4 for Polly REST (synthesize)
static QString amzDateNowUTC()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss'Z'");
}
static QString amzDateYMD()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
}

QByteArray CloudTTSProvider::hmacSha256(const QByteArray& key, const QByteArray& data)
{ return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256); }
QByteArray CloudTTSProvider::sha256(const QByteArray& data)
{ return QCryptographicHash::hash(data, QCryptographicHash::Sha256); }
QString CloudTTSProvider::sha256Hex(const QByteArray& data)
{ return sha256(data).toHex(); }

void CloudTTSProvider::postPolly(const QString& text, const QLocale& locale, double rate, double pitch, double volume)
{
    Q_UNUSED(rate); Q_UNUSED(pitch); Q_UNUSED(volume); Q_UNUSED(locale);
    if (m_pollyRegion.isEmpty() || m_pollyAccessKey.isEmpty() || m_pollySecretKey.isEmpty() || m_pollyVoice.isEmpty()) {
        emit errorOccurred("Polly not configured."); return; }

    const QString service = "polly";
    const QString host = QString("polly.%1.amazonaws.com").arg(m_pollyRegion);
    const QUrl url(QString("https://%1/v1/speech").arg(host));

    QJsonObject body{{"OutputFormat", "mp3"}, {"Text", text}, {"VoiceId", m_pollyVoice}};
    QByteArray bodyBytes = QJsonDocument(body).toJson(QJsonDocument::Compact);

    const QString amzDate = amzDateNowUTC();
    const QString date = amzDateYMD();
    const QString credentialScope = QString("%1/%2/%3/aws4_request").arg(date, m_pollyRegion, service);

    // Canonical request
    QString canonicalHeaders = QString("content-type:application/json\nhost:%1\nx-amz-date:%2\n").arg(host, amzDate);
    QString signedHeaders = "content-type;host;x-amz-date";
    QString canonicalRequest = QString("POST\n/v1/speech\n\n%1\n%2\n%3").arg(canonicalHeaders, signedHeaders, sha256Hex(bodyBytes));

    // String to sign
    QString sts = QString("AWS4-HMAC-SHA256\n%1\n%2\n%3").arg(amzDate, credentialScope, sha256Hex(canonicalRequest.toUtf8()));

    // Signing key
    QByteArray kDate = hmacSha256("AWS4" + m_pollySecretKey.toUtf8(), date.toUtf8());
    QByteArray kRegion = hmacSha256(kDate, m_pollyRegion.toUtf8());
    QByteArray kService = hmacSha256(kRegion, service.toUtf8());
    QByteArray kSigning = hmacSha256(kService, "aws4_request");

    QString signature = hmacSha256(kSigning, sts.toUtf8()).toHex();
    QString authHeader = QString("AWS4-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
                             .arg(m_pollyAccessKey, credentialScope, signedHeaders, signature);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", authHeader.toUtf8());
    req.setRawHeader("x-amz-date", amzDate.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = m_nam.post(req, bodyBytes);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) { emit errorOccurred(reply->errorString()); reply->deleteLater(); return; }
        QByteArray audio = reply->readAll();
        QBuffer* buf = new QBuffer(this);
        buf->setData(audio);
        buf->open(QIODevice::ReadOnly);
        m_player->setSourceDevice(buf);
        m_player->play();
        reply->deleteLater();
    });
}

void CloudTTSProvider::fetchEdgeVoices()
{
    if (m_edgeExePath.isEmpty()) {
        emit voiceListReady(edgeSuggestedVoicesFor(QLocale()));
        return;
    }

    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        QStringList voices;
        
        if (exitCode == 0) {
            QByteArray output = proc->readAllStandardOutput();
            QString outputStr = QString::fromUtf8(output);
            
            // Parse edge-tts --list-voices output
            QStringList lines = outputStr.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.contains("Name:")) {
                    // Extract voice name from "Name: en-US-AriaNeural"
                    QString voiceName = line.section("Name:", 1, 1).trimmed();
                    if (!voiceName.isEmpty()) {
                        voices.append(voiceName);
                    }
                }
            }
        }
        
        if (voices.isEmpty()) {
            // Fallback to hardcoded list
            voices = edgeSuggestedVoicesFor(QLocale());
        }
        
        emit voiceListReady(voices);
        proc->deleteLater();
    });
    
    proc->start(m_edgeExePath, QStringList() << "--list-voices");
}

void CloudTTSProvider::fetchGoogleFreeVoices()
{
    // Google Translate TTS doesn't have multiple voices per language
    // It's just one voice per language, so we return language options
    emit voiceListReady(googleFreeSuggestedVoicesFor(QLocale()));
}
