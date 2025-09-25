#include "SystemTray.h"
#include "FloatingWidget.h"
#include <QApplication>
#include <QStyle>

SystemTray::SystemTray(FloatingWidget *widget, QObject *parent)
    : QSystemTrayIcon(parent), floatingWidget(widget)
{
    // Create tray menu
    trayMenu = new QMenu();

    // Screenshot action
    screenshotAction = new QAction("📷 Take Screenshot", this);
    screenshotAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(screenshotAction, &QAction::triggered, this, &SystemTray::takeScreenshot);
    trayMenu->addAction(screenshotAction);

    trayMenu->addSeparator();

    // Toggle visibility action
    toggleAction = new QAction("👁️ Toggle Visibility", this);
    toggleAction->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(toggleAction, &QAction::triggered, this, &SystemTray::toggleVisibility);
    trayMenu->addAction(toggleAction);

    // Settings action
    settingsAction = new QAction("⚙️ Settings", this);
    connect(settingsAction, &QAction::triggered, this, &SystemTray::openSettings);
    trayMenu->addAction(settingsAction);

    trayMenu->addSeparator();

    // Quit action
    quitAction = new QAction("❌ Quit", this);
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitApplication);
    trayMenu->addAction(quitAction);

    setContextMenu(trayMenu);

    // Connect tray activation
    connect(this, &QSystemTrayIcon::activated, this, &SystemTray::onTrayActivated);

    // Create a simple tray icon using built-in style
    QIcon trayIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    if (trayIcon.isNull()) {
        // Fallback to creating a simple icon programmatically
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::blue);
        trayIcon = QIcon(pixmap);
    }
    setIcon(trayIcon);
    setToolTip("Ohao Language Learner - OCR & Translation Tool");

    show();
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        toggleVisibility();
    }
}

void SystemTray::takeScreenshot()
{
    if (floatingWidget) {
        floatingWidget->takeScreenshot();
    }
}

void SystemTray::toggleVisibility()
{
    if (floatingWidget) {
        if (floatingWidget->isVisible()) {
            floatingWidget->hide();
            toggleAction->setText("👁️ Show Widget");
        } else {
            floatingWidget->show();
            floatingWidget->raise();
            floatingWidget->activateWindow();
            toggleAction->setText("👁️ Hide Widget");
        }
    }
}

void SystemTray::openSettings()
{
    if (floatingWidget) {
        floatingWidget->openSettings();
    }
}

void SystemTray::quitApplication()
{
    QApplication::quit();
}