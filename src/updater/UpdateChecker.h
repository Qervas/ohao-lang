#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QSettings>

struct UpdateInfo {
    QString version;
    QString downloadUrl;
    QString releaseNotesUrl;
    QString releaseNotes;
    QString publishedAt;
    bool isNewerVersion = false;
};

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker();

    // Check for updates manually
    void checkForUpdates();

    // Enable/disable automatic update checks
    void setAutoCheckEnabled(bool enabled);
    bool isAutoCheckEnabled() const;

    // Set check interval in hours (default: 24 hours)
    void setCheckInterval(int hours);

    // Get current application version
    static QString currentVersion();

signals:
    void updateAvailable(const UpdateInfo &info);
    void updateCheckFinished(bool updateAvailable);
    void updateCheckFailed(const QString &error);

private slots:
    void onNetworkReplyFinished();
    void onAutoCheckTimerTimeout();

private:
    void parseGitHubRelease(const QByteArray &response);
    bool compareVersions(const QString &remoteVersion, const QString &currentVersion);
    void showUpdateDialog(const UpdateInfo &info);
    void scheduleNextAutoCheck();

    QNetworkAccessManager *m_networkManager = nullptr;
    QTimer *m_autoCheckTimer = nullptr;
    QSettings *m_settings = nullptr;

    static const QString GITHUB_API_URL;
    static const QString GITHUB_REPO;
    static const int DEFAULT_CHECK_INTERVAL_HOURS = 24;
};
