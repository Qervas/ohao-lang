#include "MainWindow.h"
#include "ScreenshotWidget.h"
#include <QApplication>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    layout = new QVBoxLayout(centralWidget);

    statusLabel = new QLabel("Ohao Language Learner", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(statusLabel);

    screenshotButton = new QPushButton("Take Screenshot", this);
    connect(screenshotButton, &QPushButton::clicked, this, &MainWindow::takeScreenshot);
    layout->addWidget(screenshotButton);

    setWindowTitle("Ohao Language Learner");
    resize(300, 150);

    // Hide by default - only show via tray
    hide();
}

MainWindow::~MainWindow() = default;

void MainWindow::takeScreenshot()
{
    statusLabel->setText("Taking screenshot...");

    // Hide this window first
    hide();

    // Give time for window to hide
    QApplication::processEvents();

    // Create and show screenshot widget
    ScreenshotWidget *screenshot = new ScreenshotWidget();
    screenshot->show();

    statusLabel->setText("Screenshot taken");
}

void MainWindow::showWidget()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::hideWidget()
{
    hide();
}