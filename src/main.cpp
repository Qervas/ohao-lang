#include <QApplication>
#include <QSystemTrayIcon>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
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