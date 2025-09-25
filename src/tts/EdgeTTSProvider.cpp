#include "EdgeTTSProvider.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QtGlobal>
#include <initializer_list>

EdgeTTSProvider::EdgeTTSProvider(QObject* parent)
    : TTSProvider(parent)
{
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
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

    if (m_executable.isEmpty()) {
        emit errorOccurred(tr("Set the edge-tts executable first."));
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
