#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QMoveEvent>

class SettingsWindow;
class GlobalShortcutManager;

class FloatingWidget : public QWidget
{
    Q_OBJECT

public:
    FloatingWidget(QWidget *parent = nullptr);

    // Public methods that can be called by SystemTray
    void takeScreenshot();
    void openSettings();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void toggleVisibility();
    void animateHover(bool hover);

private:
    void setupUI();
    void applyModernStyle();

    QPushButton *screenshotBtn;
    QPushButton *settingsBtn;
    QPropertyAnimation *hoverAnimation;
    QPropertyAnimation *scaleAnimation;
    QGraphicsDropShadowEffect *shadowEffect;

    // For dragging
    QPoint dragPosition;
    bool isDragging;

    // Animation properties
    qreal currentScale;
    int currentOpacity;

    // Settings for persisting window position
    QSettings settings{QCoreApplication::organizationName(), QCoreApplication::applicationName()};

    // Track Wayland compositor-driven move so we can persist position on release
    bool isWaylandSystemMove = false;

    // Debounce saving position while moving
    QTimer *savePosTimer = nullptr;

    // Settings window
    SettingsWindow *settingsWindow = nullptr;

    // Global shortcut manager (Windows only)
#ifdef Q_OS_WIN
    GlobalShortcutManager *shortcutManager = nullptr;
#endif

    bool screenshotInProgress = false;
};
