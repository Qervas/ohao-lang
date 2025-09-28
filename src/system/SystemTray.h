#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class FloatingWidget;

class SystemTray : public QSystemTrayIcon
{
    Q_OBJECT

public:
    SystemTray(FloatingWidget *widget, QObject *parent = nullptr);

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void takeScreenshot();
    void toggleVisibility();
    void openSettings();
    void quitApplication();

private:
    FloatingWidget *floatingWidget;
    QMenu *trayMenu;
    QAction *screenshotAction;
    QAction *toggleAction;
    QAction *settingsAction;
    QAction *quitAction;
};