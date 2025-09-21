#include "TextReplacementOverlay.h"
#include <QPainter>
#include <QFontMetrics>
#include <QtMath>

TextReplacementOverlay::TextReplacementOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    hide();
}

void TextReplacementOverlay::setTokens(const QVector<OCRResult::OCRToken> &tokens, const QString &originalText, const QString &translatedText)
{
    m_tokens = tokens;
    m_originalFull = originalText;
    m_translatedFull = translatedText;
    rebuildLayout();
    update();
}

void TextReplacementOverlay::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    rebuildLayout();
    update();
}

void TextReplacementOverlay::setDebugBoxes(bool enabled)
{
    m_debugBoxes = enabled;
    update();
}

void TextReplacementOverlay::setSourceImageSize(const QSize &size)
{
    m_sourceImageSize = size;
}

void TextReplacementOverlay::setFontScaling(float factor)
{
    m_fontScale = factor;
    rebuildLayout();
    update();
}

QStringList TextReplacementOverlay::splitTranslatedToMatch(const QString &originalJoined, const QString &translatedJoined, int tokenCount) const
{
    // Simple heuristic: distribute translated text proportionally by character share of each original token
    QStringList originalTokens; originalTokens.reserve(tokenCount);
    // Build originalTokens from m_tokens order
    for (const auto &t : m_tokens) originalTokens << t.text;
    QList<int> lengths; lengths.reserve(tokenCount);
    int totalOrig = 0; for (const auto &s : originalTokens) { lengths << s.length(); totalOrig += s.length(); }
    if (totalOrig == 0) totalOrig = 1;
    QStringList result; result.reserve(tokenCount);
    int pos = 0;
    for (int i=0;i<tokenCount;i++) {
        int sliceLen = qRound(double(lengths[i]) / double(totalOrig) * translatedJoined.length());
        if (i == tokenCount - 1) sliceLen = translatedJoined.length() - pos; // remainder
        result << translatedJoined.mid(pos, sliceLen);
        pos += sliceLen;
    }
    return result;
}

void TextReplacementOverlay::rebuildLayout()
{
    m_renderTokens.clear();
    if (m_tokens.isEmpty()) return;

    QStringList translatedParts;
    if (!m_translatedFull.isEmpty() && m_mode == ShowTranslated) {
        translatedParts = splitTranslatedToMatch(m_originalFull, m_translatedFull, m_tokens.size());
    }

    for (int i=0;i<m_tokens.size();++i) {
        RenderToken rt; rt.token = m_tokens[i];
        rt.renderedText = (m_mode == ShowTranslated && !translatedParts.isEmpty()) ? translatedParts[i].trimmed() : rt.token.text;
        // Infer font size from box height (simple baseline heuristic)
        int h = rt.token.box.height();
        int fontPt = qMax(8, int(h * 0.8 * m_fontScale));
        QFont f; f.setPointSize(fontPt);
        rt.font = f;
        m_renderTokens.push_back(rt);
    }
}

void TextReplacementOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_renderTokens.isEmpty()) return;

    for (const auto &rt : m_renderTokens) {
        QRect box = rt.token.box;
        // Adjust if widget scaled differently than source image
        if (!m_sourceImageSize.isEmpty() && m_sourceImageSize != size()) {
            double sx = double(width()) / double(m_sourceImageSize.width());
            double sy = double(height()) / double(m_sourceImageSize.height());
            box = QRect(int(box.x()*sx), int(box.y()*sy), int(box.width()*sx), int(box.height()*sy));
        }
        if (m_debugBoxes) {
            QColor c = (m_mode == ShowTranslated) ? QColor(0,255,180,60) : QColor(0,120,255,60);
            p.fillRect(box, c);
            p.setPen(QPen(Qt::white, 1));
            p.drawRect(box.adjusted(0,0,-1,-1));
        }
        p.setPen(Qt::white);
        p.setFont(rt.font);
        // Simple fit: shrink font if text wider than box
        QFont f = rt.font;
        QFontMetrics fm(f);
        QString text = rt.renderedText;
        int maxWidth = box.width()-4;
        while (fm.horizontalAdvance(text) > maxWidth && f.pointSize() > 6) {
            f.setPointSize(f.pointSize()-1);
            p.setFont(f);
            fm = QFontMetrics(f);
        }
        p.drawText(box.adjusted(2,0,-2,0), Qt::AlignVCenter|Qt::AlignHCenter|Qt::TextWordWrap, text);
    }
}
