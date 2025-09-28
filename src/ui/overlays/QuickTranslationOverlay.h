#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPointer>
#include "../ocr/OCREngine.h" // for OCRResult::OCRToken

// Elegant floating overlay that shows translation in a clean panel near the selection
class QuickTranslationOverlay : public QWidget {
    Q_OBJECT

signals:
    void escapePressed();

public:
    enum Mode { ShowOriginal, ShowTranslated, ShowBoth };
    enum PositionMode { AutoPosition, AboveSelection, BelowSelection, LeftOfSelection, RightOfSelection };

    explicit QuickTranslationOverlay(QWidget *parent = nullptr);
    void setContent(const QString &originalText, const QString &translatedText);
    void setMode(Mode mode);
    void setPositionNearRect(const QRect &selectionRect, const QSize &screenSize);
    void setFontScaling(float factor);

public slots:
    void updateThemeColors();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void calculateOptimalPosition(const QRect &selectionRect, const QSize &screenSize);
    void calculatePanelSize();
    QRect getTextRect(const QString &text, const QFont &font, int maxWidth) const;
    void drawPanel(QPainter &painter);
    void drawContent(QPainter &painter);

    QString m_originalText;
    QString m_translatedText;
    Mode m_mode = ShowTranslated;
    PositionMode m_positionMode = AutoPosition;
    float m_fontScale = 1.0f;

    // Panel properties
    QSize m_panelSize;
    QPoint m_panelPosition;
    int m_cornerRadius = 12;
    int m_padding = 16;
    int m_spacing = 12;

    // Typography
    QFont m_titleFont;
    QFont m_textFont;

    // Colors (will be updated based on theme)
    QColor m_backgroundColor;
    QColor m_textColor;
    QColor m_shadowColor;
    QColor m_borderColor;
};
