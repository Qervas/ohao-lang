#include <QApplication>
#include <QSystemTrayIcon>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QLocalServer>
#include <QMessageBox>
#include <QDebug>
#include "ui/FloatingWidget.h"
#include "system/SystemTray.h"
#include "ui/ThemeManager.h"

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef _WIN32
#ifdef _DEBUG
    // Allocate console for debug builds only
    if (AllocConsole()) {
        freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
        freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
        freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio(true);
        SetConsoleTitle(L"OHAO Debug Console");
        qDebug() << "Debug console allocated successfully";
    }
#endif
#endif

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
    ThemeManager::instance().applyFromSettings();

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