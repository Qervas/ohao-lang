#include <QApplication>
#include <QSystemTrayIcon>
#include <QCoreApplication>
#include "FloatingWidget.h"
#include "SystemTray.h"
#include "ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("ohao-lang");
    QCoreApplication::setOrganizationName("ohao");
    QCoreApplication::setOrganizationDomain("ohao.local");

    // Apply theme early so all widgets inherit styles
    ThemeManager::applyFromSettings();

    // Create and show floating widget as a top-level window
    FloatingWidget *widget = new FloatingWidget(nullptr);
    widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
    widget->show();

    // Optional: Still have system tray for additional control
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        SystemTray *tray = new SystemTray(nullptr);
    }

    return app.exec();
}