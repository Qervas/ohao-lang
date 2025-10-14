#include "UpdateChecker.h"
#include <QDebug>
#include <QDateTime>
#include <QVersionNumber>

// GitHub API endpoint for latest release
const QString UpdateChecker::GITHUB_API_URL = "https://api.github.com/repos/%1/releases/latest";
const QString UpdateChecker::GITHUB_REPO = "Qervas/ohao-lang";

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_autoCheckTimer(new QTimer(this))
    , m_settings(new QSettings("ohao", "ohao-lang", this))
{
    connect(m_autoCheckTimer, &QTimer::timeout, this, &UpdateChecker::onAutoCheckTimerTimeout);

    // Load auto-check setting
    bool autoCheckEnabled = m_settings->value("updates/autoCheck", true).toBool();
    setAutoCheckEnabled(autoCheckEnabled);

    // Load check interval
    int checkInterval = m_settings->value("updates/checkIntervalHours", DEFAULT_CHECK_INTERVAL_HOURS).toInt();
    setCheckInterval(checkInterval);

    qDebug() << "UpdateChecker initialized. Auto-check:" << autoCheckEnabled << "Interval:" << checkInterval << "hours";
}

UpdateChecker::~UpdateChecker()
{
    qDebug() << "UpdateChecker destroyed";
}

QString UpdateChecker::currentVersion()
{
    // Read version from CMakeLists.txt project version
    // This should match the version in CMakeLists.txt
    return "1.0.1";
}

void UpdateChecker::checkForUpdates()
{
    qDebug() << "Checking for updates...";

    QString apiUrl = GITHUB_API_URL.arg(GITHUB_REPO);
    QNetworkRequest request(apiUrl);

    // Set user agent (GitHub API requires it)
    request.setRawHeader("User-Agent", "OhaoLang-UpdateChecker");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &UpdateChecker::onNetworkReplyFinished);

    qDebug() << "Update check request sent to:" << apiUrl;
}

void UpdateChecker::setAutoCheckEnabled(bool enabled)
{
    m_settings->setValue("updates/autoCheck", enabled);

    if (enabled) {
        scheduleNextAutoCheck();
    } else {
        m_autoCheckTimer->stop();
    }

    qDebug() << "Auto-check enabled:" << enabled;
}

bool UpdateChecker::isAutoCheckEnabled() const
{
    return m_settings->value("updates/autoCheck", true).toBool();
}

void UpdateChecker::setCheckInterval(int hours)
{
    if (hours < 1) hours = 1;  // Minimum 1 hour
    if (hours > 168) hours = 168;  // Maximum 1 week

    m_settings->setValue("updates/checkIntervalHours", hours);

    if (isAutoCheckEnabled()) {
        scheduleNextAutoCheck();
    }

    qDebug() << "Update check interval set to:" << hours << "hours";
}

void UpdateChecker::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("Update check failed: %1").arg(reply->errorString());
        qDebug() << error;
        emit updateCheckFailed(error);
        emit updateCheckFinished(false);
        return;
    }

    QByteArray response = reply->readAll();
    qDebug() << "Update check response received, size:" << response.size();

    parseGitHubRelease(response);
}

void UpdateChecker::parseGitHubRelease(const QByteArray &response)
{
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        qDebug() << "Invalid JSON response from GitHub API";
        emit updateCheckFailed("Invalid response from update server");
        emit updateCheckFinished(false);
        return;
    }

    QJsonObject release = doc.object();

    // Extract version information
    QString tagName = release["tag_name"].toString();
    QString version = tagName;
    if (version.startsWith("v")) {
        version = version.mid(1);  // Remove 'v' prefix if present
    }

    // Extract download URL for Windows installer
    QString downloadUrl;
    QJsonArray assets = release["assets"].toArray();
    for (const QJsonValue &assetValue : assets) {
        QJsonObject asset = assetValue.toObject();
        QString assetName = asset["name"].toString();

        // Look for Windows installer (.exe)
        if (assetName.endsWith(".exe") && assetName.contains("setup", Qt::CaseInsensitive)) {
            downloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }

    // If no installer found, use the release page URL
    if (downloadUrl.isEmpty()) {
        downloadUrl = release["html_url"].toString();
    }

    // Create UpdateInfo struct
    UpdateInfo info;
    info.version = version;
    info.downloadUrl = downloadUrl;
    info.releaseNotesUrl = release["html_url"].toString();
    info.releaseNotes = release["body"].toString();
    info.publishedAt = release["published_at"].toString();

    // Compare versions
    QString currentVer = currentVersion();
    info.isNewerVersion = compareVersions(version, currentVer);

    qDebug() << "Latest release version:" << version;
    qDebug() << "Current version:" << currentVer;
    qDebug() << "Is newer version:" << info.isNewerVersion;
    qDebug() << "Download URL:" << downloadUrl;

    // Save last check timestamp
    m_settings->setValue("updates/lastCheck", QDateTime::currentDateTime());

    if (info.isNewerVersion) {
        emit updateAvailable(info);
        showUpdateDialog(info);
    }

    emit updateCheckFinished(info.isNewerVersion);
}

bool UpdateChecker::compareVersions(const QString &remoteVersion, const QString &currentVersion)
{
    // Use QVersionNumber for robust version comparison
    QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
    QVersionNumber current = QVersionNumber::fromString(currentVersion);

    qDebug() << "Comparing versions: remote=" << remote << "current=" << current;

    return remote > current;
}

void UpdateChecker::showUpdateDialog(const UpdateInfo &info)
{
    QString title = "Update Available";
    QString message = QString(
        "<h3>A new version of Ohao Language Learner is available!</h3>"
        "<p><b>Current version:</b> %1</p>"
        "<p><b>New version:</b> %2</p>"
        "<p><b>Published:</b> %3</p>"
        "<br>"
        "<p><b>What's new:</b></p>"
        "<p>%4</p>"
        "<br>"
        "<p>Would you like to download the update now?</p>"
    ).arg(currentVersion())
     .arg(info.version)
     .arg(QDateTime::fromString(info.publishedAt, Qt::ISODate).toString("yyyy-MM-dd"))
     .arg(info.releaseNotes.left(500).replace("\n", "<br>"));  // Limit release notes length

    QMessageBox msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    int result = msgBox.exec();

    if (result == QMessageBox::Yes) {
        // Open download URL in default browser
        QDesktopServices::openUrl(QUrl(info.downloadUrl));
        qDebug() << "Opening download URL:" << info.downloadUrl;
    }
}

void UpdateChecker::onAutoCheckTimerTimeout()
{
    qDebug() << "Auto-check timer triggered";
    checkForUpdates();
}

void UpdateChecker::scheduleNextAutoCheck()
{
    int intervalHours = m_settings->value("updates/checkIntervalHours", DEFAULT_CHECK_INTERVAL_HOURS).toInt();
    int intervalMs = intervalHours * 60 * 60 * 1000;  // Convert hours to milliseconds

    m_autoCheckTimer->stop();
    m_autoCheckTimer->start(intervalMs);

    qDebug() << "Next auto-check scheduled in" << intervalHours << "hours";
}
