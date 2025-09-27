#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QProgressBar>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSlider>
#include "OCREngine.h"
#include "../core/LanguageManager.h"
#include "../core/ThemeManager.h"

class LanguageLearningOverlay : public QWidget
{
    Q_OBJECT

signals:
    void escapePressed();

public:
    enum LearningMode {
        QuickView,      // Quick translation view
        StudyMode,      // Detailed learning with breakdowns
        PracticeMode,   // Interactive practice
        VocabMode       // Vocabulary building
    };

    explicit LanguageLearningOverlay(QWidget *parent = nullptr);

    void showLearningContent(const OCRResult& result);
    void setLearningMode(LearningMode mode);
    void positionNearSelection(const QRect& selectionRect);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;

public slots:
    void applyTheme();

private slots:
    void onWordClicked(const QString& word);
    void onPlayOriginal();
    void onPlayTranslation();
    void onToggleMode();
    void onSaveToVocab();
    void onStartPractice();
    void onPinOverlay(bool pinned);
    void onTransparencyChanged(int value);
    void onThemeChanged();

private:
    void setupUI();
    void setupHeaderSection();
    void setupContentSections();
    void setupFooterSection();
    void setupQuickViewMode();
    void setupStudyMode();
    void setupPracticeMode();
    void setupVocabMode();

    void updateContent();
    void createWordBreakdown();
    void createPronunciationGuide();
    void createGrammarHints();
    void createInteractiveElements();

    void animateIn();
    void animateOut();

    ThemeManager::Theme getCurrentTheme() const;
    QString getLanguageDirection(const QString& langCode);
    QFont getLanguageFont(const QString& langCode, int size = 12);

    // UI Components
    QVBoxLayout* m_mainLayout;
    QFrame* m_headerFrame;
    QFrame* m_contentFrame;
    QFrame* m_footerFrame;

    // Header
    QLabel* m_titleLabel;
    QLabel* m_languageLabel;
    QPushButton* m_modeToggleBtn;
    QPushButton* m_pinBtn;
    QPushButton* m_closeBtn;
    QSlider* m_transparencySlider;

    // Content Area
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;

    // Original Text Section
    QFrame* m_originalSection;
    QLabel* m_originalLabel;
    QTextEdit* m_originalText;
    QPushButton* m_playOriginalBtn;

    // Translation Section
    QFrame* m_translationSection;
    QLabel* m_translationLabel;
    QTextEdit* m_translationText;
    QPushButton* m_playTranslationBtn;

    // Word Breakdown Section
    QFrame* m_wordSection;
    QLabel* m_wordLabel;
    QVBoxLayout* m_wordLayout;

    // Grammar/Context Section
    QFrame* m_grammarSection;
    QLabel* m_grammarLabel;
    QTextEdit* m_grammarText;

    // Interactive Practice Section
    QFrame* m_practiceSection;
    QLabel* m_practiceLabel;
    QVBoxLayout* m_practiceLayout;
    QPushButton* m_startPracticeBtn;

    // Vocabulary Section
    QFrame* m_vocabSection;
    QPushButton* m_saveVocabBtn;
    QCheckBox* m_knownWordsCheck;
    QProgressBar* m_learningProgress;

    // Footer Controls
    QHBoxLayout* m_footerLayout;
    QPushButton* m_settingsBtn;
    QPushButton* m_helpBtn;

    // Data
    OCRResult m_currentResult;
    LearningMode m_currentMode;
    bool m_isPinned;

    // Animation
    QPropertyAnimation* m_slideAnimation;
    QPropertyAnimation* m_fadeAnimation;
    QGraphicsDropShadowEffect* m_shadowEffect;

    // Word interaction
    QVector<QPushButton*> m_wordButtons;
    QString m_selectedWord;

    // Styling
    QString m_currentTheme;
    QFont m_titleFont;
    QFont m_contentFont;
    QFont m_originalFont;
    QFont m_translationFont;

    // Drag functionality
    QPoint m_dragPosition;
};