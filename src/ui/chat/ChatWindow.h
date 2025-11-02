#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QKeyEvent>
#include "../../translation/TranslationEngine.h"

/**
 * @brief Translation Chat Window - Bidirectional translation chat interface
 *
 * Provides a chat-like interface for quick bidirectional translation between
 * input and output languages. Automatically detects language direction and
 * maintains conversation history.
 *
 * Features:
 * - Bidirectional translation (input↔output languages)
 * - Auto-detect language direction
 * - Scrollable conversation history
 * - Press Enter or button to translate
 * - Transparent themed widget
 * - ESC to close
 */
class ChatWindow : public QWidget
{
    Q_OBJECT

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
    void updateThemeColors();

private:
    void setupUI();
    void setupTranslation();
    void positionWindow();
    void sendMessage();
    void appendToHistory(const QString &userText, const QString &translation,
                        const QString &detectedLang, bool isReverse);
    QString detectInputLanguage(const QString &text);
    bool isTargetLanguage(const QString &text);

    // UI Components
    QTextEdit *m_historyView;
    QLineEdit *m_inputField;
    QPushButton *m_sendButton;
    QPushButton *m_closeButton;
    QVBoxLayout *m_mainLayout;

    // Translation
    TranslationEngine *m_translationEngine;
    QString m_sourceLanguage;  // From settings (OCR language)
    QString m_targetLanguage;  // From settings (user's target)
    QString m_currentInput;    // Pending translation text

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
    bool m_translating;

    // Dragging support
    bool m_dragging;
    QPoint m_dragPosition;
};
