#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include <QComboBox>
#include "../../translation/TranslationEngine.h"

class AIEngine;

/**
 * @brief Smart Chat Window - Dual-mode translation and AI assistant interface
 *
 * Provides a chat-like interface with two modes:
 * 1. Translation Mode: Bidirectional translation (input↔output languages)
 * 2. AI Assistant Mode: Context-aware AI chat with language learning support
 *
 * Features:
 * - Mode selector to switch between translation and AI
 * - Auto-fallback to translation if AI service unavailable
 * - Bidirectional translation with auto language detection
 * - AI conversation history for context-aware responses
 * - Token usage tracking (AI mode)
 * - Scrollable conversation history
 * - Press Enter or button to send
 * - Transparent themed widget
 * - Draggable window
 * - ESC to close
 */
class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    enum ChatMode {
        TranslationMode,
        AIAssistantMode
    };

signals:
    void closed();

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();

    void show();
    void hide();
    void clearHistory();
    void setOpacity(int opacity); // 0-100

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onSendButtonClicked();
    void onTranslationFinished(const TranslationResult &result);
    void onTranslationProgress(const QString &status);
    void onTranslationError(const QString &error);
    void onAIResponseReceived(const QString &response, int tokensUsed);
    void onAIError(const QString &error);
    void onAIConnectionStatusChanged(bool connected);
    void onModeChanged(int index);
    void updateThemeColors();
    void clearContext();

private:
    void setupUI();
    void setupTranslation();
    void setupAI();
    void positionWindow();
    void sendMessage();
    void sendTranslationMessage(const QString &text);
    void sendAIMessage(const QString &text);
    void switchToTranslationMode();
    void appendToHistory(const QString &userText, const QString &translation,
                        const QString &detectedLang, bool isReverse);
    void appendAIResponse(const QString &response, int tokensUsed);
    void appendSystemMessage(const QString &message);
    QString detectInputLanguage(const QString &text);
    bool isTargetLanguage(const QString &text);

    // UI Components
    QTextEdit *m_historyView;
    QLineEdit *m_inputField;
    QPushButton *m_sendButton;
    QPushButton *m_closeButton;
    QComboBox *m_modeSelector;
    QVBoxLayout *m_mainLayout;

    // Translation
    TranslationEngine *m_translationEngine;
    QString m_sourceLanguage;  // From settings (OCR language)
    QString m_targetLanguage;  // From settings (user's target)
    QString m_currentInput;    // Pending translation/AI request text

    // AI Assistant
    AIEngine *m_aiEngine;
    QStringList m_conversationHistory;  // For AI context

    // Visual properties
    QColor m_backgroundColor;
    QColor m_textColor;
    QColor m_borderColor;
    QColor m_inputBgColor;
    QColor m_buttonColor;
    int m_cornerRadius;
    int m_opacity;  // 0-100
    int m_fontSize;

    // State
    ChatMode m_chatMode;
    bool m_translating;

    // Dragging support
    bool m_dragging;
    QPoint m_dragPosition;
};
