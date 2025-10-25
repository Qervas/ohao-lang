#include "EdgeTTSProvider.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QDateTime>
#include <QMutex>
#include <QtGlobal>
#include <initializer_list>

// Voice cache with static storage
static QStringList s_cachedVoices;
static QDateTime s_cacheTimestamp;
static QMutex s_cacheMutex;
static const int CACHE_HOURS = 24; // Cache voices for 24 hours

// Static availability check - persists across instances
static bool s_edgeTtsChecked = false;
static bool s_edgeTtsAvailable = false;
static QString s_edgeTtsExecutable;  // Cache the executable path too

EdgeTTSProvider::EdgeTTSProvider(QObject* parent)
    : TTSProvider(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);

    // Try to auto-detect edge-tts command (check only once, cache result)
    if (!s_edgeTtsChecked) {
        QProcess testProcess;
        testProcess.start("edge-tts", QStringList() << "--list-voices");
        if (testProcess.waitForFinished(5000)) {
            QString output = testProcess.readAllStandardOutput();
            // If we get voice list output, edge-tts is installed
            if (!output.isEmpty() && output.contains("Name")) {
                s_edgeTtsAvailable = true;
                s_edgeTtsExecutable = "edge-tts";
                qDebug() << "EdgeTTSProvider: Auto-detected edge-tts command";
            } else {
                s_edgeTtsAvailable = false;
                s_edgeTtsExecutable = "";
                qDebug() << "EdgeTTSProvider: edge-tts command not found in PATH";
            }
        } else {
            s_edgeTtsAvailable = false;
            s_edgeTtsExecutable = "";
            qDebug() << "EdgeTTSProvider: edge-tts command not responding";
        }
        s_edgeTtsChecked = true;
    }

    // Use cached results
    m_edgeTtsAvailable = s_edgeTtsAvailable;
    m_executable = s_edgeTtsExecutable;
    if (m_edgeTtsAvailable) {
        qDebug() << "EdgeTTSProvider: Using cached availability, executable set to:" << m_executable;
    } else {
        qDebug() << "EdgeTTSProvider: Using cached availability, edge-tts NOT available";
    }
}

EdgeTTSProvider::~EdgeTTSProvider()
{
    stop();
}

QString EdgeTTSProvider::id() const
{
    return QStringLiteral("edge-free");
}

QString EdgeTTSProvider::displayName() const
{
    return QStringLiteral("Microsoft Edge (Free)");
}

QStringList EdgeTTSProvider::suggestedVoicesFor(const QLocale& locale) const
{
    // Try dynamic voice discovery first
    QString languageCode = locale.name().left(5); // Get "en-US" format or fall back to "en"
    if (languageCode.length() < 5) {
        languageCode = locale.name().left(2); // Just the language part
    }

    QStringList dynamicVoices = getVoicesForLanguage(languageCode);

    if (!dynamicVoices.isEmpty()) {
        qDebug() << "EdgeTTSProvider: Using dynamic voice discovery for" << locale.name() << "- found" << dynamicVoices.size() << "voices";
        return dynamicVoices;
    }

    // Also try just the language code if full locale didn't work
    if (languageCode.length() > 2) {
        QString baseLanguageCode = languageCode.left(2);
        QStringList baseVoices = getVoicesForLanguage(baseLanguageCode);
        if (!baseVoices.isEmpty()) {
            qDebug() << "EdgeTTSProvider: Using dynamic voice discovery for base language" << baseLanguageCode << "- found" << baseVoices.size() << "voices";
            return baseVoices;
        }
    }

    // Fallback to hardcoded voices if dynamic discovery fails
    qDebug() << "EdgeTTSProvider: Dynamic discovery failed for" << locale.name() << "- using fallback voices";

    const auto makeList = [](std::initializer_list<const char*> voices) {
        QStringList list;
        for (const char* voice : voices) {
            list.append(QString::fromUtf8(voice));
        }
        return list;
    };

    switch (locale.language()) {
    case QLocale::English:
        if (locale.territory() == QLocale::UnitedKingdom)
            return makeList({
                "en-GB-SoniaNeural",
                "en-GB-RyanNeural",
                "en-GB-MaisieNeural",
                "en-GB-LibbyNeural"
            });
        if (locale.territory() == QLocale::India)
            return makeList({
                "en-IN-NeerjaNeural",
                "en-IN-PrabhatNeural",
                "en-IN-AnanyaNeural"
            });
        return makeList({
            "en-US-AriaNeural",
            "en-US-JennyNeural",
            "en-US-GuyNeural",
            "en-US-DavisNeural",
            "en-US-AnaNeural",
            "en-US-ChristopherNeural"
        });
    case QLocale::Chinese:
        if (locale.script() == QLocale::SimplifiedChineseScript || locale.territory() == QLocale::China)
            return makeList({
                "zh-CN-XiaoxiaoNeural",
                "zh-CN-YunxiNeural",
                "zh-CN-XiaoyiNeural",
                "zh-CN-YunjianNeural",
                "zh-CN-YunjieNeural"
            });
        return makeList({
            "zh-TW-HsiaoChenNeural",
            "zh-TW-HsiaoYuNeural",
            "zh-HK-HiuMaanNeural",
            "zh-HK-WanLungNeural"
        });
    case QLocale::Japanese:
        return makeList({
            "ja-JP-NanamiNeural",
            "ja-JP-AoiNeural",
            "ja-JP-KeitaNeural",
            "ja-JP-MayuNeural"
        });
    case QLocale::Korean:
        return makeList({
            "ko-KR-SunHiNeural",
            "ko-KR-InJoonNeural",
            "ko-KR-JiMinNeural"
        });
    case QLocale::Spanish:
        return makeList({
            "es-ES-ElviraNeural",
            "es-ES-AlvaroNeural",
            "es-MX-DaliaNeural",
            "es-MX-JorgeNeural",
            "es-US-PalomaNeural"
        });
    case QLocale::French:
        return makeList({
            "fr-FR-DeniseNeural",
            "fr-FR-HenriNeural",
            "fr-CA-SylvieNeural",
            "fr-CA-AntoineNeural"
        });
    case QLocale::German:
        return makeList({
            "de-DE-KatjaNeural",
            "de-DE-ConradNeural",
            "de-DE-LouisaNeural"
        });
    case QLocale::Italian:
        return makeList({
            "it-IT-ElsaNeural",
            "it-IT-IsabellaNeural",
            "it-IT-DiegoNeural"
        });
    case QLocale::Portuguese:
        return makeList({
            "pt-BR-FranciscaNeural",
            "pt-BR-AntonioNeural",
            "pt-PT-FernandaNeural"
        });
    case QLocale::Russian:
        return makeList({
            "ru-RU-SvetlanaNeural",
            "ru-RU-DmitryNeural",
            "ru-RU-AlenaNeural"
        });
    case QLocale::Arabic:
        return makeList({
            "ar-SA-ZariyahNeural",
            "ar-SA-HamedNeural",
            "ar-EG-SalmaNeural"
        });
    case QLocale::Hindi:
        return makeList({
            "hi-IN-SwaraNeural",
            "hi-IN-MadhurNeural",
            "hi-IN-KalpanaNeural"
        });
    case QLocale::Thai:
        return makeList({
            "th-TH-AcharaNeural",
            "th-TH-NiwatNeural",
            "th-TH-PremwadeeNeural"
        });
    case QLocale::Swedish:
        return makeList({
            "sv-SE-SofieNeural",
            "sv-SE-MattiasNeural",
            "sv-SE-HilleviNeural"
        });
    default:
        return makeList({
            "en-US-AriaNeural",
            "en-US-JennyNeural"
        });
    }
}

void EdgeTTSProvider::applyConfig(const Config& config)
{
    m_voice = config.voice.trimmed();
    m_executable = config.extra.value(QStringLiteral("exePath")).toString().trimmed();
}

void EdgeTTSProvider::speak(const QString& text,
                            const QLocale& /*locale*/,
                            double rate,
                            double pitch,
                            double volume)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    qDebug() << "EdgeTTSProvider::speak() - m_executable:" << m_executable << "m_edgeTtsAvailable:" << m_edgeTtsAvailable;

    if (m_executable.isEmpty() || !m_edgeTtsAvailable) {
        qDebug() << "EdgeTTSProvider::speak() - FAILING availability check!";
        emit errorOccurred(tr("Edge TTS not installed. Install with: pip install edge-tts"));
        return;
    }

    if (m_voice.isEmpty()) {
        emit errorOccurred(tr("Choose a Microsoft Edge voice."));
        return;
    }

    stop();
    launchEdgeTTS(text, rate, pitch, volume);
}

void EdgeTTSProvider::stop()
{
    if (m_process) {
        disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_player) {
        m_player->stop();
        m_player->setSourceDevice(nullptr);
    }

    if (!m_tmpMediaPath.isEmpty()) {
        QFile::remove(m_tmpMediaPath);
        m_tmpMediaPath.clear();
    }
}

void EdgeTTSProvider::launchEdgeTTS(const QString& text,
                                    double rate,
                                    double pitch,
                                    double volume)
{
    QTemporaryFile mediaTemplate(QDir::tempPath() + "/edge-tts-XXXXXX.mp3");
    if (!mediaTemplate.open()) {
        emit errorOccurred(tr("Could not create temporary output file."));
        return;
    }
    m_tmpMediaPath = mediaTemplate.fileName();
    mediaTemplate.close();

    QStringList arguments;
    arguments << QStringLiteral("--voice") << m_voice
              << QStringLiteral("--text") << text
              << QStringLiteral("--write-media") << m_tmpMediaPath;

    // Map normalized pitch/rate settings to edge-tts arguments (percentage string).
    const int ratePercent = static_cast<int>(rate * 100.0);
    const int pitchPercent = static_cast<int>(pitch * 100.0);

    if (!qFuzzyIsNull(rate)) {
        arguments << QStringLiteral("--rate") << QString::number(ratePercent) + QStringLiteral("%");
    }
    if (!qFuzzyIsNull(pitch)) {
        arguments << QStringLiteral("--pitch") << QString::number(pitchPercent) + QStringLiteral("%");
    }
    if (!qFuzzyCompare(volume, 1.0)) {
        const int gain = static_cast<int>((volume - 1.0) * 20.0);
        arguments << QStringLiteral("--volume") << (gain >= 0 ? QStringLiteral("+") + QString::number(gain) : QString::number(gain)) + QStringLiteral("dB");
    }

    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &EdgeTTSProvider::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit errorOccurred(tr("Failed to launch edge-tts executable."));
    });

    m_process->start(m_executable, arguments);
    if (!m_process->waitForStarted(5000)) {
        emit errorOccurred(tr("edge-tts did not start. Check the executable path."));
        stop();
    }
}

void EdgeTTSProvider::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    QProcess* finishedProcess = m_process;
    m_process = nullptr;

    if (status != QProcess::NormalExit || exitCode != 0) {
        const QString error = finishedProcess ? QString::fromLocal8Bit(finishedProcess->readAllStandardError()) : QString();
        if (finishedProcess) {
            finishedProcess->deleteLater();
        }
        emit errorOccurred(error.isEmpty() ? tr("edge-tts failed to generate audio.") : error);
        stop();
        return;
    }

    if (finishedProcess) {
        finishedProcess->deleteLater();
    }

    QFile audioFile(m_tmpMediaPath);
    if (!audioFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("edge-tts output is missing."));
        stop();
        return;
    }

    QByteArray audioData = audioFile.readAll();
    audioFile.close();

    auto *buffer = new QBuffer(this);
    buffer->setData(audioData);
    buffer->open(QIODevice::ReadOnly);
    m_player->setSourceDevice(buffer);
    m_player->play();

    emit started();
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::StoppedState) {
            if (!m_tmpMediaPath.isEmpty()) {
                QFile::remove(m_tmpMediaPath);
                m_tmpMediaPath.clear();
            }
            emit finished();
        }
    });
}

// Dynamic voice discovery methods with smart caching
QStringList EdgeTTSProvider::getAllAvailableVoices(bool forceRefresh)
{
    QMutexLocker locker(&s_cacheMutex);

    // Check if we should use cached voices
    if (!forceRefresh && isVoiceCacheValid() && !s_cachedVoices.isEmpty()) {
        qDebug() << "EdgeTTSProvider: Using cached voices (" << s_cachedVoices.size() << "voices, cached"
                 << s_cacheTimestamp.secsTo(QDateTime::currentDateTime()) / 3600 << "hours ago)";
        return s_cachedVoices;
    }

    qDebug() << "EdgeTTSProvider: Discovering voices from edge-tts" << (forceRefresh ? "(forced refresh)" : "(cache expired/empty)");

    QStringList voices;
    QProcess process;
    process.start("edge-tts", QStringList() << "--list-voices");

    if (!process.waitForFinished(10000)) { // 10 second timeout
        qDebug() << "EdgeTTSProvider: Failed to get voice list from edge-tts";
        // Return cached voices if available, even if expired
        if (!s_cachedVoices.isEmpty()) {
            qDebug() << "EdgeTTSProvider: Returning stale cached voices as fallback";
            return s_cachedVoices;
        }
        return voices;
    }

    if (process.exitCode() != 0) {
        qDebug() << "EdgeTTSProvider: edge-tts --list-voices failed:" << process.readAllStandardError();
        // Return cached voices if available
        if (!s_cachedVoices.isEmpty()) {
            qDebug() << "EdgeTTSProvider: Returning cached voices due to edge-tts error";
            return s_cachedVoices;
        }
        return voices;
    }

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // Skip header line and parse voice names
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;

        // Voice name is the first column
        QStringList columns = line.split(QRegularExpression("\\s+"));
        if (!columns.isEmpty()) {
            QString voiceName = columns[0];
            if (!voiceName.isEmpty() && voiceName != "Name") {
                voices.append(voiceName);
            }
        }
    }

    if (!voices.isEmpty()) {
        // Update cache
        s_cachedVoices = voices;
        s_cacheTimestamp = QDateTime::currentDateTime();

        // Persist to settings for next app launch
        QSettings settings;
        settings.setValue("edgeTTS/cachedVoices", voices);
        settings.setValue("edgeTTS/cacheTimestamp", s_cacheTimestamp);

        qDebug() << "EdgeTTSProvider: Discovered and cached" << voices.size() << "voices from edge-tts";
    }

    return voices;
}

QStringList EdgeTTSProvider::getVoicesForLanguage(const QString& languageCode, bool forceRefresh)
{
    QStringList allVoices = getAllAvailableVoices(forceRefresh);
    QStringList matchingVoices;

    for (const QString& voice : allVoices) {
        if (voice.startsWith(languageCode + "-", Qt::CaseInsensitive)) {
            matchingVoices.append(voice);
        }
    }

    qDebug() << "EdgeTTSProvider: Found" << matchingVoices.size() << "voices for language" << languageCode;
    return matchingVoices;
}

void EdgeTTSProvider::clearVoiceCache()
{
    QMutexLocker locker(&s_cacheMutex);

    s_cachedVoices.clear();
    s_cacheTimestamp = QDateTime();

    // Clear persistent cache
    QSettings settings;
    settings.remove("edgeTTS/cachedVoices");
    settings.remove("edgeTTS/cacheTimestamp");

    qDebug() << "EdgeTTSProvider: Voice cache cleared";
}

bool EdgeTTSProvider::isVoiceCacheValid()
{
    if (s_cacheTimestamp.isNull()) {
        // Try to load from persistent cache
        QSettings settings;
        s_cachedVoices = settings.value("edgeTTS/cachedVoices").toStringList();
        s_cacheTimestamp = settings.value("edgeTTS/cacheTimestamp").toDateTime();

        if (!s_cacheTimestamp.isNull()) {
            qDebug() << "EdgeTTSProvider: Loaded voice cache from settings (" << s_cachedVoices.size() << "voices)";
        }
    }

    if (s_cacheTimestamp.isNull()) {
        return false;
    }

    // Cache is valid for CACHE_HOURS hours
    qint64 hoursOld = s_cacheTimestamp.secsTo(QDateTime::currentDateTime()) / 3600;
    return hoursOld < CACHE_HOURS;
}
