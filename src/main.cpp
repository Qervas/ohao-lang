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
#include "ui/core/FloatingWidget.h"
#include "system/SystemTray.h"
#include "ui/core/ThemeManager.h"

#ifdef Q_OS_MACOS
#include "system/PermissionsDialog.h"
#endif

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

    // Parse command line arguments first (before single instance check)
    QCommandLineParser parser;
    parser.setApplicationDescription("Ohao Language Translator");
    parser.addHelpOption();

    QCommandLineOption screenshotOption("screenshot", "Take a screenshot");
    parser.addOption(screenshotOption);

    QCommandLineOption toggleOption("toggle", "Toggle widget visibility");
    parser.addOption(toggleOption);

    parser.process(app);

    // Single instance check using shared memory
    const QString uniqueKey = "ohao-lang-single-instance";
    QSharedMemory sharedMemory(uniqueKey);

    // Try to attach first - if it succeeds, another instance might be running
    if (sharedMemory.attach()) {
        qDebug() << "Found existing shared memory, checking if instance is actually running...";
        sharedMemory.detach();

        // Try to communicate with existing instance via local socket
        QLocalSocket socket;
        socket.connectToServer("ohao-lang-server");
        if (socket.waitForConnected(500)) {
            // Existing instance is alive - send command if specified
            qDebug() << "Existing instance is running...";

            if (parser.isSet(screenshotOption)) {
                qDebug() << "Sending screenshot command to existing instance";
                socket.write("screenshot");
            } else if (parser.isSet(toggleOption)) {
                qDebug() << "Sending toggle command to existing instance";
                socket.write("toggle");
            } else {
                qDebug() << "Activating existing instance";
                socket.write("activate");
            }

            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            return 0; // Exit silently
        }
        qDebug() << "No running instance found, cleaning up stale shared memory...";
    }
    
    // Create shared memory (will succeed even if old segment exists but no process owns it)
    if (!sharedMemory.create(1)) {
        // If create fails after cleanup attempt, force detach and retry
        qDebug() << "Failed to create shared memory, attempting to fix...";
        if (sharedMemory.error() == QSharedMemory::AlreadyExists) {
            // Attach to the existing segment and detach immediately to clean it up
            if (sharedMemory.attach()) {
                qDebug() << "Attached to stale shared memory, detaching...";
                sharedMemory.detach();
            }
            // Try creating again
            if (!sharedMemory.create(1)) {
                qDebug() << "Still failed to create shared memory after cleanup:" << sharedMemory.errorString();
                // Last resort: maybe there really is another instance?
                QLocalSocket socket;
                socket.connectToServer("ohao-lang-server");
                if (socket.waitForConnected(500)) {
                    socket.write("activate");
                    socket.waitForBytesWritten(1000);
                    socket.disconnectFromServer();
                    return 0;
                }
                // No instance running, proceed anyway
                qDebug() << "No instance detected via socket, proceeding without shared memory lock";
            }
        } else {
            qDebug() << "Unexpected shared memory error:" << sharedMemory.errorString();
        }
    } else {
        qDebug() << "Successfully created shared memory lock";
    }

    // Apply theme early so all widgets inherit styles
    ThemeManager::instance().applyFromSettings();

    // Initialize default OCR engine on first launch
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (!settings.contains("ocr/engine")) {
#ifdef Q_OS_MACOS
        settings.setValue("ocr/engine", "AppleVision");
        qDebug() << "First launch: Setting default OCR engine to Apple Vision";
#else
        settings.setValue("ocr/engine", "Tesseract");
        qDebug() << "First launch: Setting default OCR engine to Tesseract";
#endif
        settings.sync();
    }

#ifdef Q_OS_MACOS
    // Show permissions dialog on first launch (macOS only)
    if (PermissionsDialog::shouldShow()) {
        PermissionsDialog permDialog;
        permDialog.exec();
    }
#endif

    // Create and show floating widget as a top-level window
    FloatingWidget *widget = new FloatingWidget(nullptr);
    widget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Window);
    widget->show();

    // Create system tray for additional control
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        SystemTray *tray = new SystemTray(widget);
        widget->setSystemTray(tray);
    }

    // Handle command line arguments (for Linux desktop shortcut support)
    if (parser.isSet(screenshotOption)) {
        QTimer::singleShot(100, widget, &FloatingWidget::takeScreenshot);
    } else if (parser.isSet(toggleOption)) {
        QTimer::singleShot(100, widget, &FloatingWidget::toggleVisibility);
    }

    return app.exec();
}