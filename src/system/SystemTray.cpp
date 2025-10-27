#include "SystemTray.h"
#include "../ui/core/FloatingWidget.h"
#include <QApplication>
#include <QStyle>
#include <QSettings>

SystemTray::SystemTray(FloatingWidget *widget, QObject *parent)
    : QSystemTrayIcon(parent), floatingWidget(widget)
{
    // Create tray menu
    trayMenu = new QMenu();

    // Screenshot action
    screenshotAction = new QAction("📷 Take Screenshot", this);
    connect(screenshotAction, &QAction::triggered, this, &SystemTray::takeScreenshot);
    trayMenu->addAction(screenshotAction);

    trayMenu->addSeparator();

    // Toggle visibility action
    toggleAction = new QAction("👁️ Toggle Visibility", this);
    connect(toggleAction, &QAction::triggered, this, &SystemTray::toggleVisibility);
    trayMenu->addAction(toggleAction);
    
    // Load and display current shortcuts
    updateShortcutLabels();

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

    // Use app icon for tray
    QIcon trayIcon(":/resources/icon-128.png");
    if (trayIcon.isNull()) {
        // Fallback: try loading from resources directory
        trayIcon = QIcon("resources/icon-128.png");
        if (trayIcon.isNull()) {
            // Last fallback: use built-in icon
            trayIcon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
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

void SystemTray::updateShortcutLabels()
{
    QSettings settings;
    
    // Load shortcuts from settings
    QString screenshotShortcut = settings.value("shortcuts/screenshot", "Meta+Shift+X").toString();
    QString toggleShortcut = settings.value("shortcuts/toggle", "Meta+Shift+Z").toString();
    
    // Convert Meta to Cmd/Win/Super based on platform
    QString screenshotDisplay = screenshotShortcut;
    QString toggleDisplay = toggleShortcut;
    
#ifdef Q_OS_MACOS
    screenshotDisplay.replace("Meta", "⌘");
    toggleDisplay.replace("Meta", "⌘");
    screenshotDisplay.replace("Shift", "⇧");
    toggleDisplay.replace("Shift", "⇧");
#elif defined(Q_OS_WIN)
    screenshotDisplay.replace("Meta", "Win");
#else
    screenshotDisplay.replace("Meta", "Super");
#endif
    
    // Update action text with shortcuts
    screenshotAction->setText(QString("📷 Take Screenshot (%1)").arg(screenshotDisplay));
    toggleAction->setText(QString("👁️ Toggle Visibility (%1)").arg(toggleDisplay));
}