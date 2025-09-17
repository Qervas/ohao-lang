#include "SelectionToolbar.h"
#include <QApplication>
#include <QScreen>
#include <QGraphicsDropShadowEffect>
#include <iostream>

SelectionToolbar::SelectionToolbar(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void SelectionToolbar::setupUI()
{
    // Make frameless and always on top
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    // Create layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Create buttons with icons
    copyBtn = new QPushButton("📋", this);
    copyBtn->setToolTip("Copy to clipboard");
    copyBtn->setObjectName("toolbarButton");
    connect(copyBtn, &QPushButton::clicked, this, &SelectionToolbar::copyRequested);

    saveBtn = new QPushButton("💾", this);
    saveBtn->setToolTip("Save to file");
    saveBtn->setObjectName("toolbarButton");
    connect(saveBtn, &QPushButton::clicked, this, &SelectionToolbar::saveRequested);

    ocrBtn = new QPushButton("📝", this);
    ocrBtn->setToolTip("Extract text (OCR)");
    ocrBtn->setObjectName("toolbarButton");
    connect(ocrBtn, &QPushButton::clicked, this, &SelectionToolbar::ocrRequested);

    cancelBtn = new QPushButton("❌", this);
    cancelBtn->setToolTip("Cancel");
    cancelBtn->setObjectName("toolbarButton");
    connect(cancelBtn, &QPushButton::clicked, this, &SelectionToolbar::cancelRequested);

    // Add buttons to layout
    layout->addWidget(copyBtn);
    layout->addWidget(saveBtn);
    layout->addWidget(ocrBtn);
    layout->addWidget(cancelBtn);

    setObjectName("selectionToolbar");

    // Add drop shadow for better visibility when positioned outside selection
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 3);
    setGraphicsEffect(shadow);
}

void SelectionToolbar::positionNear(const QRect &selection)
{
    // Get screen geometry
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->geometry();
    const int margin = 8; // Distance from selection border

    // Make sure toolbar is properly sized before positioning
    adjustSize();

    // Get actual toolbar dimensions (fallback to estimated size if not available)
    int toolbarWidth = width() > 0 ? width() : 200; // Fallback estimated width
    int toolbarHeight = height() > 0 ? height() : 50; // Fallback estimated height

    std::cout << "*** Positioning toolbar - Size: " << toolbarWidth << "x" << toolbarHeight
              << " Selection: " << selection.x() << "," << selection.y()
              << " " << selection.width() << "x" << selection.height() << std::endl;

    // Calculate preferred positions (priority order)
    // 1. Below selection (preferred like Flameshot)
    // 2. Above selection
    // 3. Right of selection
    // 4. Left of selection

    QPoint bestPosition;

    // Try below selection first (Flameshot style)
    int x = selection.x() + (selection.width() - toolbarWidth) / 2;
    int y = selection.bottom() + margin;

    std::cout << "*** Trying position BELOW selection: " << x << "," << y << std::endl;

    if (y + toolbarHeight <= screenGeometry.height() &&
        x >= 0 && x + toolbarWidth <= screenGeometry.width()) {
        bestPosition = QPoint(x, y);
        std::cout << "*** BELOW selection works, using: " << bestPosition.x() << "," << bestPosition.y() << std::endl;
    }
    // Try above selection
    else {
        y = selection.top() - toolbarHeight - margin;
        qDebug() << "BELOW failed, trying ABOVE selection:" << QPoint(x, y);

        if (y >= 0 && x >= 0 && x + toolbarWidth <= screenGeometry.width()) {
            bestPosition = QPoint(x, y);
            qDebug() << "ABOVE selection works, using:" << bestPosition;
        }
        // Try right of selection
        else {
            x = selection.right() + margin;
            y = selection.y() + (selection.height() - toolbarHeight) / 2;
            qDebug() << "ABOVE failed, trying RIGHT of selection:" << QPoint(x, y);

            if (x + toolbarWidth <= screenGeometry.width() &&
                y >= 0 && y + toolbarHeight <= screenGeometry.height()) {
                bestPosition = QPoint(x, y);
                qDebug() << "RIGHT of selection works, using:" << bestPosition;
            }
            // Try left of selection
            else {
                x = selection.left() - toolbarWidth - margin;
                y = selection.y() + (selection.height() - toolbarHeight) / 2;
                qDebug() << "RIGHT failed, trying LEFT of selection:" << QPoint(x, y);

                if (x >= 0 && y >= 0 && y + toolbarHeight <= screenGeometry.height()) {
                    bestPosition = QPoint(x, y);
                    qDebug() << "LEFT of selection works, using:" << bestPosition;
                }
                // Fallback: force position below with screen constraints
                else {
                    x = qMax(0, qMin(selection.x() + (selection.width() - toolbarWidth) / 2,
                                   screenGeometry.width() - toolbarWidth));
                    y = qMin(selection.bottom() + margin, screenGeometry.height() - toolbarHeight);
                    bestPosition = QPoint(x, y);
                    qDebug() << "ALL positions failed, using FALLBACK:" << bestPosition;
                }
            }
        }
    }

    std::cout << "*** About to move toolbar to: " << bestPosition.x() << "," << bestPosition.y() << std::endl;
    std::cout << "*** Current toolbar position before move: " << pos().x() << "," << pos().y() << std::endl;

    move(bestPosition);

    // Force the widget to update its position
    update();
    repaint();

    std::cout << "*** Toolbar position after move: " << pos().x() << "," << pos().y() << std::endl;
    std::cout << "*** FINAL: Positioned toolbar at: " << bestPosition.x() << "," << bestPosition.y() << std::endl;
}