#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPointer>
#include "OCREngine.h" // for OCRResult::OCRToken

// Lightweight overlay that can be positioned over the captured screen region
// It paints translated (or original) text inside the OCR token bounding boxes.
class TextReplacementOverlay : public QWidget {
    Q_OBJECT
public:
    enum Mode { ShowOriginal, ShowTranslated };

    explicit TextReplacementOverlay(QWidget *parent = nullptr);
    void setTokens(const QVector<OCRResult::OCRToken> &tokens, const QString &originalText, const QString &translatedText);
    void setMode(Mode mode);
    void setDebugBoxes(bool enabled);
    void setSourceImageSize(const QSize &size); // coordinate reference
    void setFontScaling(float factor); // global scale tweak

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct RenderToken {
        OCRResult::OCRToken token;
        QString renderedText; // text we attempt to place (may be original or translated segment)
        QFont   font;
    };

    void rebuildLayout();
    QStringList splitTranslatedToMatch(const QString &originalJoined, const QString &translatedJoined, int tokenCount) const;

    QVector<OCRResult::OCRToken> m_tokens;
    QVector<RenderToken> m_renderTokens;
    QString m_originalFull;
    QString m_translatedFull;
    QSize   m_sourceImageSize;
    Mode    m_mode = ShowTranslated;
    bool    m_debugBoxes = true;
    float   m_fontScale = 1.0f;
};
