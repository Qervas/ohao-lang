#include "TTSEngine.h"

#include "GoogleWebTTSProvider.h"
#include "EdgeTTSProvider.h"

#include <QCoreApplication>
#include <QtGlobal>

TTSEngine::TTSEngine(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName(), this))
{
    loadSettings();
    ensureProvider();
    applyProviderConfig();
}

TTSEngine::~TTSEngine() = default;

void TTSEngine::ensureProvider()
{
    if (m_provider && m_provider->id() == m_providerId) {
        return;
    }

    m_provider.reset();

    std::unique_ptr<TTSProvider> providerInstance;
    if (m_providerId == QStringLiteral("edge-free")) {
        providerInstance = std::make_unique<EdgeTTSProvider>();
    } else {
        providerInstance = std::make_unique<GoogleWebTTSProvider>();
        m_providerId = QStringLiteral("google-web");
    }

    auto *providerPtr = providerInstance.get();
    connect(providerPtr, &TTSProvider::started, this, [this]() {
        m_state = QTextToSpeech::Speaking;
        m_isSpeaking = true;
        emit stateChanged(m_state);
    });

    connect(providerPtr, &TTSProvider::finished, this, [this]() {
        m_state = QTextToSpeech::Ready;
        m_isSpeaking = false;
        emit stateChanged(m_state);
    });

    connect(providerPtr, &TTSProvider::errorOccurred, this, [this](const QString& error) {
        m_state = QTextToSpeech::Error;
        m_isSpeaking = false;
        emit stateChanged(m_state);
        emit errorOccurred(error);
    });

    m_provider = std::move(providerInstance);
}

void TTSEngine::configureGoogle(const QString& /*apiKey*/, const QString& voiceName, const QString& languageCode)
{
    m_googleVoice = voiceName.trimmed();
    m_googleLanguageCode = languageCode.trimmed();
    if (m_providerId == QStringLiteral("google-web")) {
        applyProviderConfig();
    }
}

void TTSEngine::setInputVoice(const QString &voiceName)
{
    m_inputVoice = voiceName.trimmed();
    m_settings->setValue("TTS/inputVoice", m_inputVoice);
}

void TTSEngine::setOutputVoice(const QString &voiceName)
{
    m_outputVoice = voiceName.trimmed();
    m_settings->setValue("TTS/outputVoice", m_outputVoice);
}

void TTSEngine::setPrimaryVoice(const QString& voiceName)
{
    if (m_providerId == QStringLiteral("edge-free")) {
        m_edgeVoice = voiceName.trimmed();
    } else {
        m_googleVoice = voiceName.trimmed();
    }
    applyProviderConfig();
}

QString TTSEngine::primaryVoice() const
{
    return m_providerId == QStringLiteral("edge-free") ? m_edgeVoice : m_googleVoice;
}

void TTSEngine::setProviderId(const QString& id)
{
    if (id == m_providerId) {
        return;
    }
    m_providerId = id;
    ensureProvider();
    applyProviderConfig();
}

QStringList TTSEngine::availableProviders() const
{
    return { QStringLiteral("google-web"), QStringLiteral("edge-free") };
}

QString TTSEngine::providerDisplayName(const QString& id) const
{
    if (id == QStringLiteral("edge-free")) {
        return QStringLiteral("Microsoft Edge (Free)");
    }
    return QStringLiteral("Google Translate (Free)");
}

void TTSEngine::setEdgeVoice(const QString& voiceName)
{
    m_edgeVoice = voiceName.trimmed();
    if (m_providerId == QStringLiteral("edge-free")) {
        applyProviderConfig();
    }
}

void TTSEngine::setEdgeExecutable(const QString& exePath)
{
    m_edgeExecutable = exePath.trimmed();
    if (m_providerId == QStringLiteral("edge-free")) {
        applyProviderConfig();
    }
}

QStringList TTSEngine::suggestedVoicesFor(const QLocale& locale) const
{
    auto* self = const_cast<TTSEngine*>(this);
    self->ensureProvider();
    if (const auto* prov = provider()) {
        return prov->suggestedVoicesFor(locale);
    }
    return {};
}

QString TTSEngine::providerName() const
{
    auto* self = const_cast<TTSEngine*>(this);
    self->ensureProvider();
    if (const auto* prov = provider()) {
        return prov->displayName();
    }
    return QString();
}

void TTSEngine::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
}

void TTSEngine::setPitch(double pitch)
{
    m_pitch = qBound(-1.0, pitch, 1.0);
}

void TTSEngine::setRate(double rate)
{
    m_rate = qBound(-1.0, rate, 1.0);
}

void TTSEngine::setTTSInputEnabled(bool enabled)
{
    m_ttsInputEnabled = enabled;
    m_settings->setValue("general/ttsInput", enabled);
}

void TTSEngine::setTTSOutputEnabled(bool enabled)
{
    m_ttsOutputEnabled = enabled;
    m_settings->setValue("general/ttsOutput", enabled);
}

void TTSEngine::speak(const QString &text)
{
    speak(text, false);
}

void TTSEngine::speak(const QString &text, bool isInputText)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    if (!isAvailable()) {
        emit errorOccurred(QStringLiteral("Voice provider not configured"));
        return;
    }

    applyProviderConfig(effectiveVoice(isInputText));
    ensureProvider();
    if (auto *prov = provider()) {
        prov->speak(text, QLocale(), m_rate, m_pitch, m_volume);
    }
}

void TTSEngine::stop()
{
    if (auto *prov = provider()) {
        prov->stop();
    }
    m_state = QTextToSpeech::Ready;
    m_isSpeaking = false;
    emit stateChanged(m_state);
}

bool TTSEngine::isAvailable() const
{
    if (m_providerId == QStringLiteral("edge-free")) {
        return !m_edgeExecutable.isEmpty() && !effectiveVoice(false).isEmpty();
    }
    return !effectiveVoice(false).isEmpty();
}

bool TTSEngine::isSpeaking() const
{
    return m_isSpeaking;
}

void TTSEngine::loadSettings()
{
    m_ttsInputEnabled = m_settings->value("general/ttsInput", false).toBool();
    m_ttsOutputEnabled = m_settings->value("general/ttsOutput", true).toBool();

    m_settings->beginGroup("TTS");
    m_volume = m_settings->value("volume", 0.8).toDouble();
    m_pitch = m_settings->value("pitch", 0.0).toDouble();
    m_rate = m_settings->value("rate", 1.0).toDouble();
    m_inputVoice = m_settings->value("inputVoice").toString();
    m_outputVoice = m_settings->value("outputVoice").toString();

    m_providerId = m_settings->value("provider", QStringLiteral("google-web")).toString();

    m_settings->beginGroup("GoogleWeb");
    m_googleVoice = m_settings->value("voice").toString();
    m_googleLanguageCode = m_settings->value("languageCode").toString();
    m_settings->endGroup();

    m_settings->beginGroup("Edge");
    m_edgeVoice = m_settings->value("voice").toString();
    m_edgeExecutable = m_settings->value("exePath").toString();
    m_settings->endGroup();

    m_settings->endGroup();
}

void TTSEngine::saveSettings()
{
    m_settings->setValue("general/ttsInput", m_ttsInputEnabled);
    m_settings->setValue("general/ttsOutput", m_ttsOutputEnabled);

    m_settings->beginGroup("TTS");
    m_settings->setValue("volume", m_volume);
    m_settings->setValue("pitch", m_pitch);
    m_settings->setValue("rate", m_rate);
    m_settings->setValue("inputVoice", m_inputVoice);
    m_settings->setValue("outputVoice", m_outputVoice);
    m_settings->setValue("provider", m_providerId);

    m_settings->beginGroup("GoogleWeb");
    m_settings->setValue("voice", m_googleVoice);
    m_settings->setValue("languageCode", m_googleLanguageCode);
    m_settings->endGroup();

    m_settings->beginGroup("Edge");
    m_settings->setValue("voice", m_edgeVoice);
    m_settings->setValue("exePath", m_edgeExecutable);
    m_settings->endGroup();

    m_settings->endGroup();

    m_settings->sync();
}

QString TTSEngine::effectiveVoice(bool isInputText) const
{
    if (isInputText && !m_inputVoice.isEmpty()) {
        return m_inputVoice;
    }
    if (!isInputText && !m_outputVoice.isEmpty()) {
        return m_outputVoice;
    }
    return primaryVoice();
}

void TTSEngine::applyProviderConfig(const QString& voiceOverride)
{
    ensureProvider();
    if (!provider()) {
        return;
    }

    TTSProvider::Config config;
    if (m_providerId == QStringLiteral("edge-free")) {
        const QString voice = voiceOverride.isEmpty() ? (m_edgeVoice.isEmpty() ? m_googleVoice : m_edgeVoice) : voiceOverride;
        config.voice = voice;
        config.extra.insert(QStringLiteral("exePath"), m_edgeExecutable);
    } else {
        const QString voice = voiceOverride.isEmpty() ? m_googleVoice : voiceOverride;
        config.voice = voice;
        config.languageCode = m_googleLanguageCode;
    }
    provider()->applyConfig(config);
}
