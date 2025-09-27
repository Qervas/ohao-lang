#include <QApplication>
#include <QSystemTrayIcon>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QLocalServer>
#include <QMessageBox>
#include "ui/FloatingWidget.h"
#include "system/SystemTray.h"
#include "ui/ThemeManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("ohao-lang");
    QCoreApplication::setOrganizationName("ohao");
    QCoreApplication::setOrganizationDomain("ohao.local");

    // Single instance check using shared memory
    const QString uniqueKey = "ohao-lang-single-instance";
    QSharedMemory sharedMemory(uniqueKey);

    if (!sharedMemory.create(1)) {
        // Another instance is already running
        // Try to communicate with existing instance via local socket
        QLocalSocket socket;
        socket.connectToServer("ohao-lang-server");
        if (socket.waitForConnected(1000)) {
            // Send activation signal to existing instance
            socket.write("activate");
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
        }
        return 0; // Exit silently
    }

    // Parse command line arguments for Linux desktop shortcuts
    QCommandLineParser parser;
    parser.setApplicationDescription("Ohao Language Translator");
    parser.addHelpOption();

    QCommandLineOption screenshotOption("screenshot", "Take a screenshot");
    parser.addOption(screenshotOption);

    QCommandLineOption toggleOption("toggle", "Toggle widget visibility");
    parser.addOption(toggleOption);

    parser.process(app);

    // Apply theme early so all widgets inherit styles
    ThemeManager::applyFromSettings();

    // Create and show floating widget as a top-level window
    FloatingWidget *widget = new FloatingWidget(nullptr);
    widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
    widget->show();

    // Create system tray for additional control
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        SystemTray *tray = new SystemTray(widget);
        Q_UNUSED(tray); // Prevent unused variable warning
    }

    // Handle command line arguments (for Linux desktop shortcut support)
    if (parser.isSet(screenshotOption)) {
        QTimer::singleShot(100, widget, &FloatingWidget::takeScreenshot);
    } else if (parser.isSet(toggleOption)) {
        QTimer::singleShot(100, widget, &FloatingWidget::toggleVisibility);
    }

    return app.exec();
}