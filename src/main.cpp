#include <QApplication>
#include <QSystemTrayIcon>
#include "FloatingWidget.h"
#include "SystemTray.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("Ohao Language Learner");

    // Create and show floating widget
    FloatingWidget *widget = new FloatingWidget();
    widget->show();

    // Optional: Still have system tray for additional control
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        SystemTray *tray = new SystemTray(nullptr);
    }

    return app.exec();
}