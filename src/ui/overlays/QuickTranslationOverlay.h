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
    void setPositionNearRect(const QRect &selectionRect, const QSize &screenSize, const QList<QRect> &avoidRects = QList<QRect>());
    void setFontScaling(float factor);

public slots:
    void updateThemeColors();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void calculateOptimalPosition(const QRect &selectionRect, const QSize &screenSize, const QList<QRect> &avoidRects = QList<QRect>());
    void calculatePanelSize();
    QRect getTextRect(const QString &text, const QFont &font, int maxWidth) const;
    void drawPanel(QPainter &painter);
    void drawContent(QPainter &painter);
    bool rectsOverlap(const QRect &rect1, const QRect &rect2, int margin = 0) const;

    QString m_originalText;
    QString m_translatedText;
    Mode m_mode = ShowTranslated;
    PositionMode m_positionMode = AutoPosition;
    float m_fontScale = 1.0f;

    // Panel properties
    QSize m_panelSize;
    QPoint m_panelPosition;
    QRect m_selectionRect;  // Store selection to draw arrow pointing to it
    int m_cornerRadius = 12;
    int m_padding = 16;
    int m_spacing = 12;
    
    // Arrow/tail properties for comic-style bubble
    enum ArrowDirection { NoArrow, ArrowUp, ArrowDown, ArrowLeft, ArrowRight };
    ArrowDirection m_arrowDirection = NoArrow;
    QPoint m_arrowTipPosition;  // Where the arrow points to (on selection)

    // Typography
    QFont m_titleFont;
    QFont m_textFont;

    // Colors (will be updated based on theme)
    QColor m_backgroundColor;
    QColor m_textColor;
    QColor m_shadowColor;
    QColor m_borderColor;
};
