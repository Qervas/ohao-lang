#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>

class FloatingWidget : public QWidget
{
    Q_OBJECT

public:
    FloatingWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void takeScreenshot();
    void openSettings();
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
};