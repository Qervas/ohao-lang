#pragma once

#include <QObject>
#include <QLocale>
#include <QVariantMap>
#include <QStringList>

// Lightweight abstraction for cloud/remote TTS backends. Each provider should
// implement this interface so that TTSEngine can switch between engines without
// knowing provider specifics.
class TTSProvider : public QObject
{
    Q_OBJECT
public:
    explicit TTSProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~TTSProvider() override = default;

    virtual QString id() const = 0;            // stable identifier, e.g. "google"
    virtual QString displayName() const = 0;   // human readable name
    virtual QStringList suggestedVoicesFor(const QLocale& locale) const = 0;

    struct Config {
        QString apiKey;
        QString voice;
        QString languageCode;
        QVariantMap extra; // provider specific options
    };

    virtual void applyConfig(const Config& config) = 0;
    virtual void speak(const QString& text,
                       const QLocale& locale,
                       double rate,
                       double pitch,
                       double volume) = 0;
    virtual void stop() = 0;

signals:
    void started();
    void finished();
    void errorOccurred(const QString& message);
};
