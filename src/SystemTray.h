#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class MainWindow;

class SystemTray : public QSystemTrayIcon
{
    Q_OBJECT

public:
    SystemTray(MainWindow *window, QObject *parent = nullptr);

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void takeScreenshot();
    void showMainWindow();
    void quitApplication();

private:
    MainWindow *mainWindow;
    QMenu *trayMenu;
    QAction *screenshotAction;
    QAction *showAction;
    QAction *quitAction;
};