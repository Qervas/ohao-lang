#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QSettings>
#include <QCoreApplication>
#include <QMoveEvent>
#include <QLocalServer>

class ModernSettingsWindow;
class GlobalShortcutManager;
class SystemTray;
class ChatWindow;

class FloatingWidget : public QWidget
{
    Q_OBJECT

public:
    FloatingWidget(QWidget *parent = nullptr);

    // Public methods that can be called by SystemTray
    void takeScreenshot();
    void openSettings();
    void openChatWindow();
    void toggleVisibility();
    void activateWindow();
    void setAlwaysOnTop(bool onTop);

    void setSystemTray(SystemTray *tray);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void animateHover(bool hover);
    void handleNewConnection();

private:
    void setupUI();
    void applyModernStyle();

    QPushButton *screenshotBtn;
    QPushButton *chatBtn;
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
    ModernSettingsWindow *settingsWindow = nullptr;

    // Chat window
    ChatWindow *chatWindow = nullptr;

    // Global shortcut manager
    GlobalShortcutManager *shortcutManager = nullptr;

    bool screenshotInProgress = false;
    bool wasVisibleBeforeScreenshot = false;

    // Single instance support
    QLocalServer *localServer = nullptr;
    
    // System tray reference
    SystemTray *systemTray = nullptr;
};
