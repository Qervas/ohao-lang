#include "SystemTray.h"
#include "MainWindow.h"
#include <QApplication>

SystemTray::SystemTray(MainWindow *window, QObject *parent)
    : QSystemTrayIcon(parent), mainWindow(window)
{
    // Create tray menu
    trayMenu = new QMenu();

    screenshotAction = new QAction("Take Screenshot", this);
    connect(screenshotAction, &QAction::triggered, this, &SystemTray::takeScreenshot);
    trayMenu->addAction(screenshotAction);

    trayMenu->addSeparator();

    showAction = new QAction("Show Window", this);
    connect(showAction, &QAction::triggered, this, &SystemTray::showMainWindow);
    trayMenu->addAction(showAction);

    quitAction = new QAction("Quit", this);
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitApplication);
    trayMenu->addAction(quitAction);

    setContextMenu(trayMenu);

    // Connect tray activation
    connect(this, &QSystemTrayIcon::activated, this, &SystemTray::onTrayActivated);

    // Set tray icon (will be a simple default for now)
    setIcon(QIcon::fromTheme("applications-graphics"));
    setToolTip("Ohao Language Learner");

    show();
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        takeScreenshot();
    }
}

void SystemTray::takeScreenshot()
{
    if (mainWindow) {
        mainWindow->takeScreenshot();
    }
}

void SystemTray::showMainWindow()
{
    if (mainWindow) {
        mainWindow->showWidget();
    }
}

void SystemTray::quitApplication()
{
    QApplication::quit();
}