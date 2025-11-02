#include "ChatWindow.h"
#include "../core/ThemeManager.h"
#include "../core/ThemeColors.h"
#include "../core/AppSettings.h"
#include "../core/LanguageManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QScrollBar>
#include <QMouseEvent>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
    , m_translationEngine(nullptr)
    , m_cornerRadius(12)
    , m_opacity(90)
    , m_fontSize(12)
    , m_translating(false)
    , m_dragging(false)
{
    // Window setup
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    setFixedSize(400, 500);

    setupUI();
    setupTranslation();
    updateThemeColors();
    positionWindow();

    // Connect to theme changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ChatWindow::updateThemeColors);

    qDebug() << "ChatWindow created";
}

ChatWindow::~ChatWindow()
{
    qDebug() << "ChatWindow destroyed";
}

void ChatWindow::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(10);

    // Title bar with close button
    QHBoxLayout *titleLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("💬 Translation Chat", this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: palette(window-text);");

    m_closeButton = new QPushButton("×", this);
    m_closeButton->setFixedSize(24, 24);
    m_closeButton->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   border: none;"
        "   font-size: 20px;"
        "   font-weight: bold;"
        "   color: palette(window-text);"
        "}"
        "QPushButton:hover {"
        "   background: rgba(255, 0, 0, 50);"
        "   border-radius: 12px;"
        "}"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &ChatWindow::hide);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeButton);
    m_mainLayout->addLayout(titleLayout);

    // History view (conversation area)
    m_historyView = new QTextEdit(this);
    m_historyView->setReadOnly(true);
    m_historyView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_historyView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_historyView->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_historyView->setPlaceholderText("Start typing to translate...");
    m_mainLayout->addWidget(m_historyView, 1); // Stretch factor

    // Input area
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);

    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText("Type message...");
    m_inputField->setMinimumHeight(36);
    connect(m_inputField, &QLineEdit::returnPressed, this, &ChatWindow::onSendButtonClicked);

    m_sendButton = new QPushButton("📤", this);
    m_sendButton->setFixedSize(40, 36);
    m_sendButton->setToolTip("Send (Enter)");
    m_sendButton->setCursor(Qt::PointingHandCursor);
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::onSendButtonClicked);

    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    m_mainLayout->addLayout(inputLayout);

    setLayout(m_mainLayout);
}

void ChatWindow::setupTranslation()
{
    m_translationEngine = new TranslationEngine(this);

    // Get language settings - reload to ensure fresh data
    AppSettings::instance().reload();
    auto translationConfig = AppSettings::instance().getTranslationConfig();
    auto ocrConfig = AppSettings::instance().getOCRConfig();

    qDebug() << "======================================";
    qDebug() << "ChatWindow: Reading translation settings...";
    qDebug() << "  OCR Language (from settings):" << ocrConfig.language;
    qDebug() << "  Translation Source (from settings):" << translationConfig.sourceLanguage;
    qDebug() << "  Translation Target (from settings):" << translationConfig.targetLanguage;
    qDebug() << "======================================";

    // USE OCR LANGUAGE as source, not translation.sourceLanguage
    // Because translation.sourceLanguage might be outdated
    m_sourceLanguage = ocrConfig.language.isEmpty() ? "Auto-Detect" : ocrConfig.language;
    m_targetLanguage = translationConfig.targetLanguage.isEmpty() ? "English" : translationConfig.targetLanguage;

    qDebug() << "ChatWindow: Will use for translation:";
    qDebug() << "  Source (OCR language):" << m_sourceLanguage;
    qDebug() << "  Target:" << m_targetLanguage;
    qDebug() << "======================================";

    // Set up translation engine
    m_translationEngine->setEngine(TranslationEngine::GoogleTranslate);

    // Connect signals
    connect(m_translationEngine, &TranslationEngine::translationFinished,
            this, &ChatWindow::onTranslationFinished);
    connect(m_translationEngine, &TranslationEngine::translationProgress,
            this, &ChatWindow::onTranslationProgress);
    connect(m_translationEngine, &TranslationEngine::translationError,
            this, &ChatWindow::onTranslationError);

    // Connect to settings changes to update languages dynamically
    connect(&AppSettings::instance(), &AppSettings::translationSettingsChanged,
            this, [this]() {
                auto ocrConfig = AppSettings::instance().getOCRConfig();
                auto translationConfig = AppSettings::instance().getTranslationConfig();
                m_sourceLanguage = ocrConfig.language.isEmpty() ? "Auto-Detect" : ocrConfig.language;
                m_targetLanguage = translationConfig.targetLanguage.isEmpty() ? "English" : translationConfig.targetLanguage;
                qDebug() << "ChatWindow: Settings changed! Updated to - Source:" << m_sourceLanguage << "Target:" << m_targetLanguage;
            });

    // Also listen to OCR settings changes (in case OCR language changes)
    connect(&AppSettings::instance(), &AppSettings::ocrSettingsChanged,
            this, [this]() {
                auto ocrConfig = AppSettings::instance().getOCRConfig();
                m_sourceLanguage = ocrConfig.language.isEmpty() ? "Auto-Detect" : ocrConfig.language;
                qDebug() << "ChatWindow: OCR language changed to:" << m_sourceLanguage;
            });
}

void ChatWindow::positionWindow()
{
    // Position window in center of screen by default
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;

    move(x, y);
}

void ChatWindow::updateThemeColors()
{
    QString themeName = ThemeManager::toString(ThemeManager::instance().getCurrentTheme());
    ThemeColors::ThemeColorSet colors = ThemeColors::getColorSet(themeName);

    // Set colors with transparency
    m_backgroundColor = colors.base;
    m_backgroundColor.setAlpha(static_cast<int>(m_opacity * 2.55)); // Convert 0-100 to 0-255

    m_textColor = colors.windowText;
    m_borderColor = colors.floatingWidgetBorder;
    m_inputBgColor = colors.button;
    m_buttonColor = colors.highlight;

    // Apply styles to widgets
    QString historyStyle = QString(
        "QTextEdit {"
        "   background-color: rgba(%1, %2, %3, 200);"
        "   border: 1px solid rgba(%4, %5, %6, 100);"
        "   border-radius: 8px;"
        "   padding: 8px;"
        "   color: rgb(%7, %8, %9);"
        "   font-size: %10px;"
        "}"
    ).arg(m_inputBgColor.red()).arg(m_inputBgColor.green()).arg(m_inputBgColor.blue())
     .arg(m_borderColor.red()).arg(m_borderColor.green()).arg(m_borderColor.blue())
     .arg(m_textColor.red()).arg(m_textColor.green()).arg(m_textColor.blue())
     .arg(m_fontSize);

    m_historyView->setStyleSheet(historyStyle);

    QString inputStyle = QString(
        "QLineEdit {"
        "   background-color: rgba(%1, %2, %3, 220);"
        "   border: 1px solid rgba(%4, %5, %6, 150);"
        "   border-radius: 6px;"
        "   padding: 6px 10px;"
        "   color: rgb(%7, %8, %9);"
        "   font-size: %10px;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid rgb(%11, %12, %13);"
        "}"
    ).arg(m_inputBgColor.red()).arg(m_inputBgColor.green()).arg(m_inputBgColor.blue())
     .arg(m_borderColor.red()).arg(m_borderColor.green()).arg(m_borderColor.blue())
     .arg(m_textColor.red()).arg(m_textColor.green()).arg(m_textColor.blue())
     .arg(m_fontSize)
     .arg(m_buttonColor.red()).arg(m_buttonColor.green()).arg(m_buttonColor.blue());

    m_inputField->setStyleSheet(inputStyle);

    QString buttonStyle = QString(
        "QPushButton {"
        "   background-color: rgba(%1, %2, %3, 200);"
        "   border: 1px solid rgba(%4, %5, %6, 150);"
        "   border-radius: 6px;"
        "   font-size: 16px;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(%1, %2, %3, 255);"
        "}"
        "QPushButton:pressed {"
        "   background-color: rgba(%1, %2, %3, 180);"
        "}"
    ).arg(m_buttonColor.red()).arg(m_buttonColor.green()).arg(m_buttonColor.blue())
     .arg(m_borderColor.red()).arg(m_borderColor.green()).arg(m_borderColor.blue());

    m_sendButton->setStyleSheet(buttonStyle);

    update(); // Repaint window
}

void ChatWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded background
    QPainterPath path;
    path.addRoundedRect(rect(), m_cornerRadius, m_cornerRadius);

    painter.fillPath(path, m_backgroundColor);
    painter.setPen(QPen(m_borderColor, 2));
    painter.drawPath(path);
}

void ChatWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        emit closed();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    hide();
    emit closed();
    event->ignore(); // Don't actually close, just hide
}

void ChatWindow::show()
{
    QWidget::show();
    raise();
    activateWindow();
    m_inputField->setFocus();
}

void ChatWindow::hide()
{
    QWidget::hide();
}

void ChatWindow::clearHistory()
{
    m_historyView->clear();
}

void ChatWindow::setOpacity(int opacity)
{
    m_opacity = qBound(0, opacity, 100);
    updateThemeColors();
}

void ChatWindow::onSendButtonClicked()
{
    sendMessage();
}

void ChatWindow::sendMessage()
{
    QString text = m_inputField->text().trimmed();

    if (text.isEmpty()) {
        return;
    }

    if (m_translating) {
        qDebug() << "ChatWindow: Translation already in progress, ignoring";
        return;
    }

    m_currentInput = text;
    m_translating = true;
    m_sendButton->setEnabled(false);
    m_inputField->setEnabled(false);

    // Detect language direction
    bool isReverse = isTargetLanguage(text);

    QString fromLang, toLang;
    if (isReverse) {
        // User typed in target language → translate to source
        fromLang = m_targetLanguage;
        toLang = m_sourceLanguage;
        qDebug() << "ChatWindow: Reverse translation" << fromLang << "→" << toLang;
    } else {
        // User typed in source language → translate to target
        fromLang = m_sourceLanguage;
        toLang = m_targetLanguage;
        qDebug() << "ChatWindow: Forward translation" << fromLang << "→" << toLang;
    }

    // Always use Auto-Detect for best results
    m_translationEngine->setSourceLanguage("Auto-Detect");
    m_translationEngine->setTargetLanguage(toLang);
    m_translationEngine->translate(text);
}

bool ChatWindow::isTargetLanguage(const QString &text)
{
    // Simple heuristic: check if text contains characters typical of target language
    // For now, use a simple approach - can be enhanced later

    // If target is English and text is mostly ASCII, assume it's target language
    if (m_targetLanguage.contains("English", Qt::CaseInsensitive)) {
        int asciiCount = 0;
        for (QChar c : text) {
            if (c.unicode() < 128) asciiCount++;
        }
        if (asciiCount > text.length() * 0.8) {
            return true; // Probably English
        }
    }

    // If source is English and text is mostly ASCII, assume it's NOT target language
    if (m_sourceLanguage.contains("English", Qt::CaseInsensitive)) {
        int asciiCount = 0;
        for (QChar c : text) {
            if (c.unicode() < 128) asciiCount++;
        }
        if (asciiCount > text.length() * 0.8) {
            return false; // Probably English (source)
        }
    }

    // Default: assume forward translation (source → target)
    return false;
}

void ChatWindow::onTranslationFinished(const TranslationResult &result)
{
    m_translating = false;
    m_sendButton->setEnabled(true);
    m_inputField->setEnabled(true);

    if (result.success) {
        QString detectedLang = result.targetLanguage;
        bool isReverse = (result.targetLanguage == m_sourceLanguage ||
                         result.targetLanguage.contains(m_sourceLanguage, Qt::CaseInsensitive));

        appendToHistory(m_currentInput, result.translatedText, detectedLang, isReverse);
        m_inputField->clear();
        m_inputField->setFocus();
    } else {
        appendToHistory(m_currentInput, "❌ Translation failed: " + result.errorMessage, "", false);
    }
}

void ChatWindow::onTranslationProgress(const QString &status)
{
    qDebug() << "ChatWindow: Translation progress:" << status;
    // Could show loading indicator here
}

void ChatWindow::onTranslationError(const QString &error)
{
    qDebug() << "ChatWindow: Translation error:" << error;
    m_translating = false;
    m_sendButton->setEnabled(true);
    m_inputField->setEnabled(true);

    appendToHistory(m_currentInput, "❌ Error: " + error, "", false);
}

void ChatWindow::appendToHistory(const QString &userText, const QString &translation,
                                 const QString &detectedLang, bool isReverse)
{
    QString timestamp = QTime::currentTime().toString("hh:mm");
    QString direction = isReverse ? "⬆️" : "⬇️";
    QString langInfo = detectedLang.isEmpty() ? "" : QString(" (%1)").arg(detectedLang);

    QString html = QString(
        "<div style='margin-bottom: 12px;'>"
        "<div style='color: %1; font-weight: 500; margin-bottom: 4px;'>"
        "<span style='color: gray; font-size: 10px;'>%2</span> You:</div>"
        "<div style='margin-left: 8px; color: %3;'>%4</div>"
        "<div style='color: %5; font-weight: 500; margin: 4px 0 2px 0;'>%6 Translation%7:</div>"
        "<div style='margin-left: 8px; color: %8; background: rgba(100, 100, 255, 20); "
        "padding: 6px; border-radius: 4px;'>%9</div>"
        "</div>"
    ).arg(m_textColor.name())
     .arg(timestamp)
     .arg(m_textColor.name())
     .arg(userText.toHtmlEscaped())
     .arg(m_textColor.name())
     .arg(direction)
     .arg(langInfo)
     .arg(m_textColor.name())
     .arg(translation.toHtmlEscaped());

    m_historyView->append(html);

    // Scroll to bottom
    QScrollBar *scrollBar = m_historyView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void ChatWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ChatWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void ChatWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}
