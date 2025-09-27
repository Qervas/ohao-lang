#include "LanguageLearningOverlay.h"
#include "../tts/TTSManager.h"
#include "../ui/ThemeManager.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QScreen>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <algorithm>
#include <QHBoxLayout>
#include <QSplitter>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QTextBrowser>
#include <QScrollBar>
#include <QToolTip>
#include <QRegularExpression>
#include <QDebug>

LanguageLearningOverlay::LanguageLearningOverlay(QWidget* parent)
    : QWidget(parent)
    , m_currentMode(QuickView)
    , m_isPinned(false)
    , m_slideAnimation(nullptr)
    , m_fadeAnimation(nullptr)
    , m_currentTheme("light")
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Setup shadow effect
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(20);
    m_shadowEffect->setOffset(0, 5);
    m_shadowEffect->setColor(QColor(0, 0, 0, 100));
    setGraphicsEffect(m_shadowEffect);

    setupUI();
    applyTheme();

    // Start hidden
    setVisible(false);

    // Install event filter for global key handling
    installEventFilter(this);

    // Connect to application palette changes to respond to theme changes
    connect(qApp, &QApplication::paletteChanged, this, &LanguageLearningOverlay::onThemeChanged);
}

void LanguageLearningOverlay::setupUI()
{
    setFixedSize(450, 600);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_mainLayout->setSpacing(10);

    // Header with controls
    setupHeaderSection();

    // Scrollable content area
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setSpacing(15);

    setupContentSections();

    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea, 1);

    // Footer controls
    setupFooterSection();
}

void LanguageLearningOverlay::setupHeaderSection()
{
    m_headerFrame = new QFrame();
    m_headerFrame->setObjectName("headerFrame");

    auto* headerLayout = new QHBoxLayout(m_headerFrame);
    headerLayout->setContentsMargins(5, 5, 5, 5);

    // Title and language info
    auto* titleLayout = new QVBoxLayout();
    m_titleLabel = new QLabel("Language Learning");
    m_titleLabel->setObjectName("titleLabel");
    m_languageLabel = new QLabel("");
    m_languageLabel->setObjectName("languageLabel");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_languageLabel);
    headerLayout->addLayout(titleLayout);

    headerLayout->addStretch();

    // Control buttons
    m_modeToggleBtn = new QPushButton("📚");
    m_modeToggleBtn->setToolTip("Toggle learning mode");
    m_modeToggleBtn->setFixedSize(32, 32);

    m_pinBtn = new QPushButton("📌");
    m_pinBtn->setToolTip("Pin overlay");
    m_pinBtn->setCheckable(true);
    m_pinBtn->setFixedSize(32, 32);

    m_closeBtn = new QPushButton("✕");
    m_closeBtn->setToolTip("Close");
    m_closeBtn->setFixedSize(32, 32);

    headerLayout->addWidget(m_modeToggleBtn);
    headerLayout->addWidget(m_pinBtn);
    headerLayout->addWidget(m_closeBtn);

    // Transparency slider
    m_transparencySlider = new QSlider(Qt::Horizontal);
    m_transparencySlider->setRange(50, 100);
    m_transparencySlider->setValue(95);
    m_transparencySlider->setMaximumWidth(80);
    headerLayout->addWidget(m_transparencySlider);

    m_mainLayout->addWidget(m_headerFrame);

    // Connect signals
    connect(m_modeToggleBtn, &QPushButton::clicked, this, &LanguageLearningOverlay::onToggleMode);
    connect(m_pinBtn, &QPushButton::toggled, this, &LanguageLearningOverlay::onPinOverlay);
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::hide);
    connect(m_transparencySlider, &QSlider::valueChanged, this, &LanguageLearningOverlay::onTransparencyChanged);
}

void LanguageLearningOverlay::setupContentSections()
{
    // Original Text Section
    m_originalSection = new QFrame();
    m_originalSection->setObjectName("section");
    auto* originalLayout = new QVBoxLayout(m_originalSection);

    auto* originalHeader = new QHBoxLayout();
    m_originalLabel = new QLabel("📝 Original Text");
    m_originalLabel->setObjectName("sectionLabel");
    m_playOriginalBtn = new QPushButton("🔊");
    m_playOriginalBtn->setToolTip("Play original text");
    m_playOriginalBtn->setFixedSize(28, 28);

    originalHeader->addWidget(m_originalLabel);
    originalHeader->addStretch();
    originalHeader->addWidget(m_playOriginalBtn);

    m_originalText = new QTextEdit();
    m_originalText->setMaximumHeight(80);
    m_originalText->setReadOnly(true);

    originalLayout->addLayout(originalHeader);
    originalLayout->addWidget(m_originalText);
    m_contentLayout->addWidget(m_originalSection);

    // Translation Section
    m_translationSection = new QFrame();
    m_translationSection->setObjectName("section");
    auto* translationLayout = new QVBoxLayout(m_translationSection);

    auto* translationHeader = new QHBoxLayout();
    m_translationLabel = new QLabel("🌍 Translation");
    m_translationLabel->setObjectName("sectionLabel");
    m_playTranslationBtn = new QPushButton("🔊");
    m_playTranslationBtn->setToolTip("Play translation");
    m_playTranslationBtn->setFixedSize(28, 28);

    translationHeader->addWidget(m_translationLabel);
    translationHeader->addStretch();
    translationHeader->addWidget(m_playTranslationBtn);

    m_translationText = new QTextEdit();
    m_translationText->setMaximumHeight(80);
    m_translationText->setReadOnly(true);

    translationLayout->addLayout(translationHeader);
    translationLayout->addWidget(m_translationText);
    m_contentLayout->addWidget(m_translationSection);

    // Word Breakdown Section
    m_wordSection = new QFrame();
    m_wordSection->setObjectName("section");
    m_wordLayout = new QVBoxLayout(m_wordSection);

    m_wordLabel = new QLabel("🔍 Word Analysis");
    m_wordLabel->setObjectName("sectionLabel");
    m_wordLayout->addWidget(m_wordLabel);
    m_contentLayout->addWidget(m_wordSection);

    // Grammar/Context Section
    m_grammarSection = new QFrame();
    m_grammarSection->setObjectName("section");
    auto* grammarLayout = new QVBoxLayout(m_grammarSection);

    m_grammarLabel = new QLabel("📖 Grammar & Context");
    m_grammarLabel->setObjectName("sectionLabel");
    m_grammarText = new QTextEdit();
    m_grammarText->setMaximumHeight(100);
    m_grammarText->setReadOnly(true);

    grammarLayout->addWidget(m_grammarLabel);
    grammarLayout->addWidget(m_grammarText);
    m_contentLayout->addWidget(m_grammarSection);

    // Practice Section
    m_practiceSection = new QFrame();
    m_practiceSection->setObjectName("section");
    m_practiceLayout = new QVBoxLayout(m_practiceSection);

    m_practiceLabel = new QLabel("🎯 Interactive Practice");
    m_practiceLabel->setObjectName("sectionLabel");
    m_startPracticeBtn = new QPushButton("Start Practice Session");

    m_practiceLayout->addWidget(m_practiceLabel);
    m_practiceLayout->addWidget(m_startPracticeBtn);
    m_contentLayout->addWidget(m_practiceSection);

    // Vocabulary Section
    m_vocabSection = new QFrame();
    m_vocabSection->setObjectName("section");
    auto* vocabLayout = new QVBoxLayout(m_vocabSection);

    auto* vocabHeader = new QHBoxLayout();
    auto* vocabLabel = new QLabel("📚 Vocabulary");
    vocabLabel->setObjectName("sectionLabel");
    m_saveVocabBtn = new QPushButton("💾 Save Words");

    vocabHeader->addWidget(vocabLabel);
    vocabHeader->addStretch();
    vocabHeader->addWidget(m_saveVocabBtn);

    m_knownWordsCheck = new QCheckBox("Mark known words");
    m_learningProgress = new QProgressBar();
    m_learningProgress->setMaximumHeight(20);

    vocabLayout->addLayout(vocabHeader);
    vocabLayout->addWidget(m_knownWordsCheck);
    vocabLayout->addWidget(m_learningProgress);
    m_contentLayout->addWidget(m_vocabSection);

    // Connect audio buttons
    connect(m_playOriginalBtn, &QPushButton::clicked, this, &LanguageLearningOverlay::onPlayOriginal);
    connect(m_playTranslationBtn, &QPushButton::clicked, this, &LanguageLearningOverlay::onPlayTranslation);
    connect(m_startPracticeBtn, &QPushButton::clicked, this, &LanguageLearningOverlay::onStartPractice);
    connect(m_saveVocabBtn, &QPushButton::clicked, this, &LanguageLearningOverlay::onSaveToVocab);
}

void LanguageLearningOverlay::setupFooterSection()
{
    m_footerFrame = new QFrame();
    m_footerFrame->setObjectName("footerFrame");
    m_footerLayout = new QHBoxLayout(m_footerFrame);

    m_settingsBtn = new QPushButton("⚙️ Settings");
    m_helpBtn = new QPushButton("❓ Help");

    m_footerLayout->addWidget(m_settingsBtn);
    m_footerLayout->addStretch();
    m_footerLayout->addWidget(m_helpBtn);

    m_mainLayout->addWidget(m_footerFrame);
}

void LanguageLearningOverlay::showLearningContent(const OCRResult& result)
{
    m_currentResult = result;
    updateContent();

    if (!isVisible()) {
        show();
        animateIn();
    }
}

void LanguageLearningOverlay::updateContent()
{
    if (m_currentResult.text.isEmpty()) return;

    // Update language info
    auto& langMgr = LanguageManager::instance();
    QString sourceLang = langMgr.displayName(m_currentResult.language);
    QString targetLang = langMgr.displayName(m_currentResult.targetLanguage);

    m_languageLabel->setText(QString("%1 → %2").arg(sourceLang, targetLang));

    // Update original text
    m_originalText->setPlainText(m_currentResult.text);
    m_originalText->setFont(getLanguageFont(m_currentResult.language, 11));

    // Update translation
    if (m_currentResult.hasTranslation) {
        m_translationText->setPlainText(m_currentResult.translatedText);
        m_translationText->setFont(getLanguageFont(m_currentResult.targetLanguage, 11));
        m_translationSection->setVisible(true);
    } else {
        m_translationSection->setVisible(false);
    }

    // Create word breakdown
    createWordBreakdown();

    // Update grammar hints
    createGrammarHints();

    // Update learning progress
    int totalWords = m_currentResult.text.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts).size();
    m_learningProgress->setMaximum(totalWords);
    m_learningProgress->setValue(totalWords / 2); // Mock progress
}

void LanguageLearningOverlay::createWordBreakdown()
{
    // Clear existing word buttons
    for (auto* btn : m_wordButtons) {
        btn->deleteLater();
    }
    m_wordButtons.clear();

    // Split text into words
    QStringList words = m_currentResult.text.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);

    auto* wordContainer = new QWidget();
    auto* wordGrid = new QHBoxLayout(wordContainer);
    wordGrid->setSpacing(5);

    int wordCount = 0;
    for (const QString& word : words) {
        if (wordCount >= 8) break; // Limit display

        auto* wordBtn = new QPushButton(word);
        wordBtn->setObjectName("wordButton");
        wordBtn->setToolTip(QString("Click to analyze: %1").arg(word));
        wordBtn->setCursor(Qt::PointingHandCursor);

        connect(wordBtn, &QPushButton::clicked, [this, word]() {
            onWordClicked(word);
        });

        m_wordButtons.append(wordBtn);
        wordGrid->addWidget(wordBtn);
        wordCount++;
    }

    if (wordCount >= 8) {
        auto* moreLabel = new QLabel(QString("... +%1 more").arg(words.size() - 8));
        moreLabel->setStyleSheet("color: gray; font-style: italic;");
        wordGrid->addWidget(moreLabel);
    }

    wordGrid->addStretch();
    m_wordLayout->addWidget(wordContainer);
}

void LanguageLearningOverlay::createGrammarHints()
{
    QString grammarInfo;

    // Simple grammar analysis based on language
    if (m_currentResult.language == "ja") {
        grammarInfo = "Japanese text may contain Hiragana (ひらがな), Katakana (カタカナ), and Kanji (漢字) characters. ";
        grammarInfo += "Word order is typically Subject-Object-Verb (SOV).";
    } else if (m_currentResult.language == "zh") {
        grammarInfo = "Chinese text uses characters (汉字/漢字) where each character typically represents a syllable and meaning. ";
        grammarInfo += "No spaces separate words, and grammar is more positional.";
    } else if (m_currentResult.language == "ko") {
        grammarInfo = "Korean uses Hangul alphabet blocks. Word order is Subject-Object-Verb (SOV). ";
        grammarInfo += "Honorifics and formality levels are important in Korean grammar.";
    } else if (m_currentResult.language == "sv") {
        grammarInfo = "Swedish has definite articles as suffixes (-en, -et, -na). ";
        grammarInfo += "Word order is Subject-Verb-Object (SVO) in main clauses.";
    } else {
        grammarInfo = QString("Text is in %1. Click on individual words above to learn more about their meaning and usage.")
                     .arg(LanguageManager::instance().displayName(m_currentResult.language));
    }

    m_grammarText->setPlainText(grammarInfo);
}

void LanguageLearningOverlay::onWordClicked(const QString& word)
{
    m_selectedWord = word;

    // Show detailed word information in a tooltip or expand section
    QString wordInfo = QString("Analyzing word: \"%1\"\n\n").arg(word);
    wordInfo += "🔤 Romanization: [phonetic guide]\n";
    wordInfo += "📚 Definition: [word meaning]\n";
    wordInfo += "🏷️ Part of speech: [noun/verb/etc]\n";
    wordInfo += "📝 Usage examples: [sample sentences]\n";
    wordInfo += "💡 Related words: [similar terms]";

    // Update grammar section with word-specific info
    m_grammarText->setPlainText(wordInfo);

    // Highlight the word visually
    for (auto* btn : m_wordButtons) {
        if (btn->text() == word) {
            btn->setStyleSheet("QPushButton { background-color: #007ACC; color: white; font-weight: bold; }");
        } else {
            btn->setStyleSheet("");
        }
    }

    // Speak the word
    TTSManager::instance().speakInputText(word, m_currentResult.language);
}

void LanguageLearningOverlay::onPlayOriginal()
{
    TTSManager::instance().speakInputText(m_currentResult.text, m_currentResult.language);
}

void LanguageLearningOverlay::onPlayTranslation()
{
    if (m_currentResult.hasTranslation) {
        TTSManager::instance().speakOutputText(m_currentResult.translatedText, m_currentResult.targetLanguage);
    }
}

void LanguageLearningOverlay::onToggleMode()
{
    // Cycle through learning modes
    int currentMode = static_cast<int>(m_currentMode);
    currentMode = (currentMode + 1) % 4;
    setLearningMode(static_cast<LearningMode>(currentMode));
}

void LanguageLearningOverlay::setLearningMode(LearningMode mode)
{
    m_currentMode = mode;

    // Update mode button icon and tooltip
    switch (mode) {
    case QuickView:
        m_modeToggleBtn->setText("⚡");
        m_modeToggleBtn->setToolTip("Quick View Mode");
        m_titleLabel->setText("Quick Translation");
        break;
    case StudyMode:
        m_modeToggleBtn->setText("📚");
        m_modeToggleBtn->setToolTip("Study Mode");
        m_titleLabel->setText("Language Study");
        break;
    case PracticeMode:
        m_modeToggleBtn->setText("🎯");
        m_modeToggleBtn->setToolTip("Practice Mode");
        m_titleLabel->setText("Practice Session");
        break;
    case VocabMode:
        m_modeToggleBtn->setText("📝");
        m_modeToggleBtn->setToolTip("Vocabulary Mode");
        m_titleLabel->setText("Vocabulary Builder");
        break;
    }

    // Show/hide sections based on mode
    bool showDetailed = (mode != QuickView);
    m_wordSection->setVisible(showDetailed);
    m_grammarSection->setVisible(showDetailed);
    m_practiceSection->setVisible(mode == PracticeMode);
    m_vocabSection->setVisible(mode == VocabMode || mode == StudyMode);
}

void LanguageLearningOverlay::onSaveToVocab()
{
    // Mock vocabulary saving
    qDebug() << "Saving vocabulary from text:" << m_currentResult.text;
    m_saveVocabBtn->setText("✓ Saved!");

    QTimer::singleShot(2000, [this]() {
        m_saveVocabBtn->setText("💾 Save Words");
    });
}

void LanguageLearningOverlay::onStartPractice()
{
    // Mock practice session
    m_startPracticeBtn->setText("🎯 Practice Started!");

    // Could launch a practice dialog or mini-game
    qDebug() << "Starting practice session for:" << m_currentResult.text;

    QTimer::singleShot(2000, [this]() {
        m_startPracticeBtn->setText("Start Practice Session");
    });
}

void LanguageLearningOverlay::onPinOverlay(bool pinned)
{
    m_isPinned = pinned;

    if (pinned) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        m_pinBtn->setText("📍");
        m_pinBtn->setToolTip("Unpin overlay");
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        m_pinBtn->setText("📌");
        m_pinBtn->setToolTip("Pin overlay");
    }

    show(); // Refresh window flags
}

void LanguageLearningOverlay::onTransparencyChanged(int value)
{
    setWindowOpacity(value / 100.0);
}

void LanguageLearningOverlay::positionNearSelection(const QRect& selectionRect)
{
    // Use the same smart positioning logic as quick mode
    QScreen* screen = QApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->availableGeometry();
    const int margin = 20;

    int overlayWidth = width();
    int overlayHeight = height();

    // Smart positioning: completely avoid blocking the selection area
    int x, y;
    bool positioned = false;

    // Calculate available space in each direction
    int spaceRight = screenGeometry.width() - selectionRect.right() - margin;
    int spaceLeft = selectionRect.left() - margin;
    int spaceBelow = screenGeometry.height() - selectionRect.bottom() - margin;
    int spaceAbove = selectionRect.top() - margin;

    // Try positions in order of available space (largest first)
    struct Position {
        int space;
        int x, y;
        QString name;
    };

    QVector<Position> positions = {
        {spaceRight, selectionRect.right() + margin, selectionRect.top(), "right"},
        {spaceLeft, selectionRect.left() - overlayWidth - margin, selectionRect.top(), "left"},
        {spaceBelow, qMax(10, qMin(selectionRect.center().x() - overlayWidth/2, screenGeometry.width() - overlayWidth - 10)),
         selectionRect.bottom() + margin, "below"},
        {spaceAbove, qMax(10, qMin(selectionRect.center().x() - overlayWidth/2, screenGeometry.width() - overlayWidth - 10)),
         selectionRect.top() - overlayHeight - margin, "above"}
    };

    // Sort by available space (descending)
    std::sort(positions.begin(), positions.end(), [](const Position& a, const Position& b) {
        return a.space > b.space;
    });

    // Try each position until we find one that fits
    for (const auto& pos : positions) {
        if (pos.space >= (pos.name == "right" || pos.name == "left" ? overlayWidth : overlayHeight)) {
            x = pos.x;
            y = pos.y;
            positioned = true;
            qDebug() << "LanguageLearningOverlay: Positioned" << pos.name << "of selection";
            break;
        }
    }

    // Fallback: if no good position found, use corner of screen
    if (!positioned) {
        // Use bottom-right corner, away from selection
        x = screenGeometry.width() - overlayWidth - 20;
        y = screenGeometry.height() - overlayHeight - 20;
        qDebug() << "LanguageLearningOverlay: Using fallback corner position";
    }

    // Final bounds check to ensure it's on screen
    x = qBound(10, x, screenGeometry.width() - overlayWidth - 10);
    y = qBound(10, y, screenGeometry.height() - overlayHeight - 10);

    move(x, y);
}

void LanguageLearningOverlay::animateIn()
{
    // Slide and fade in animation
    if (m_slideAnimation) {
        m_slideAnimation->deleteLater();
    }

    m_slideAnimation = new QPropertyAnimation(this, "pos");
    m_slideAnimation->setDuration(300);
    m_slideAnimation->setEasingCurve(QEasingCurve::OutCubic);

    QPoint startPos = pos() + QPoint(50, 0);
    QPoint endPos = pos();

    move(startPos);
    m_slideAnimation->setStartValue(startPos);
    m_slideAnimation->setEndValue(endPos);

    setWindowOpacity(0.0);

    m_fadeAnimation = new QPropertyAnimation(this, "windowOpacity");
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(m_transparencySlider->value() / 100.0);

    m_slideAnimation->start();
    m_fadeAnimation->start();
}

void LanguageLearningOverlay::applyTheme()
{
    ThemeManager::Theme currentTheme = getCurrentTheme();
    QString styleSheet;

    switch (currentTheme) {
        case ThemeManager::Theme::Dark:
            styleSheet = R"(
                LanguageLearningOverlay {
                    background-color: #1C2026;
                    border-radius: 12px;
                    border: 1px solid #3A404B;
                }

                QFrame#headerFrame {
                    background-color: #2A2F37;
                    border-radius: 8px;
                    border: none;
                    padding: 5px;
                }

                QFrame#section {
                    background-color: #22262C;
                    border-radius: 8px;
                    border: 1px solid #3A404B;
                    padding: 10px;
                }

                QLabel#titleLabel {
                    font-size: 16px;
                    font-weight: bold;
                    color: #E6E9EF;
                }

                QLabel#languageLabel {
                    font-size: 11px;
                    color: #C7CDD7;
                }

                QLabel#sectionLabel {
                    font-size: 13px;
                    font-weight: bold;
                    color: #E6E9EF;
                    margin-bottom: 5px;
                }

                QPushButton {
                    background-color: #5082E6;
                    color: #E6E9EF;
                    border: 1px solid #3C6DD9;
                    border-radius: 6px;
                    padding: 6px 12px;
                    font-weight: 500;
                }

                QPushButton:hover {
                    background-color: #3C6DD9;
                    border-color: #5082E6;
                }

                QPushButton:pressed {
                    background-color: #2A5BC4;
                }

                QPushButton#wordButton {
                    background-color: #2A2F37;
                    color: #E6E9EF;
                    border: 1px solid #3A404B;
                    font-size: 11px;
                    padding: 4px 8px;
                }

                QPushButton#wordButton:hover {
                    background-color: #303642;
                    border-color: #5082E6;
                }

                QTextEdit {
                    background-color: #22262C;
                    border: 1px solid #3A404B;
                    border-radius: 4px;
                    padding: 8px;
                    font-size: 12px;
                    color: #E6E9EF;
                }

                QProgressBar {
                    border: 1px solid #3A404B;
                    border-radius: 4px;
                    background-color: #2A2F37;
                }

                QProgressBar::chunk {
                    background-color: #4CCA6A;
                    border-radius: 3px;
                }

                QSlider::groove:horizontal {
                    height: 4px;
                    background-color: #2A2F37;
                    border-radius: 2px;
                }

                QSlider::handle:horizontal {
                    background-color: #5082E6;
                    width: 16px;
                    height: 16px;
                    border-radius: 8px;
                    margin: -6px 0;
                }

                QCheckBox {
                    color: #E6E9EF;
                }

                QScrollArea {
                    background-color: #1C2026;
                    border: none;
                }
            )";
            break;

        case ThemeManager::Theme::HighContrast:
            styleSheet = R"(
                LanguageLearningOverlay {
                    background-color: #000000;
                    border-radius: 12px;
                    border: 2px solid #FFFF00;
                }

                QFrame#headerFrame {
                    background-color: #000000;
                    border-radius: 8px;
                    border: 1px solid #FFFF00;
                    padding: 5px;
                }

                QFrame#section {
                    background-color: #000000;
                    border-radius: 8px;
                    border: 2px solid #FFFF00;
                    padding: 10px;
                }

                QLabel {
                    color: #FFFF00;
                    font-weight: bold;
                }

                QPushButton {
                    background-color: #FFFF00;
                    color: #000000;
                    border: 2px solid #FFFF00;
                    border-radius: 6px;
                    padding: 6px 12px;
                    font-weight: bold;
                }

                QPushButton:hover {
                    background-color: #000000;
                    color: #FFFF00;
                }

                QTextEdit {
                    background-color: #000000;
                    border: 2px solid #FFFF00;
                    color: #FFFF00;
                    font-weight: bold;
                }

                QProgressBar {
                    border: 2px solid #FFFF00;
                    background-color: #000000;
                }

                QProgressBar::chunk {
                    background-color: #FFFF00;
                }
            )";
            break;

        case ThemeManager::Theme::Cyberpunk:
            styleSheet = R"(
                LanguageLearningOverlay {
                    background-color: #0a0e1a;
                    border-radius: 12px;
                    border: 1px solid #00ff88;
                }

                QFrame#headerFrame {
                    background-color: #1a1f2e;
                    border-radius: 8px;
                    border: none;
                    padding: 5px;
                }

                QFrame#section {
                    background-color: #0f1419;
                    border-radius: 8px;
                    border: 1px solid #00ff88;
                    padding: 10px;
                }

                QLabel#titleLabel {
                    font-size: 16px;
                    font-weight: bold;
                    color: #00ff88;
                }

                QLabel#languageLabel {
                    font-size: 11px;
                    color: #ff006e;
                }

                QLabel#sectionLabel {
                    font-size: 13px;
                    font-weight: bold;
                    color: #00d4ff;
                    margin-bottom: 5px;
                }

                QPushButton {
                    background-color: #00ff88;
                    color: #0a0e1a;
                    border: none;
                    border-radius: 6px;
                    padding: 6px 12px;
                    font-weight: bold;
                }

                QPushButton:hover {
                    background-color: #00d4ff;
                }

                QPushButton#wordButton {
                    background-color: #1a1f2e;
                    color: #00ff88;
                    border: 1px solid #00ff88;
                    font-size: 11px;
                    padding: 4px 8px;
                }

                QTextEdit {
                    background-color: #0f1419;
                    border: 1px solid #00ff88;
                    color: #ffffff;
                }

                QProgressBar {
                    border: 1px solid #00ff88;
                    background-color: #1a1f2e;
                }

                QProgressBar::chunk {
                    background-color: #00ff88;
                }
            )";
            break;

        default: // Light theme
            styleSheet = R"(
                LanguageLearningOverlay {
                    background-color: #ffffff;
                    border-radius: 12px;
                    border: 1px solid #e0e0e0;
                }

                QFrame#headerFrame {
                    background-color: #f8f9fa;
                    border-radius: 8px;
                    border: none;
                    padding: 5px;
                }

                QFrame#section {
                    background-color: #fafbfc;
                    border-radius: 8px;
                    border: 1px solid #e6e8ea;
                    padding: 10px;
                }

                QLabel#titleLabel {
                    font-size: 16px;
                    font-weight: bold;
                    color: #2c3e50;
                }

                QLabel#languageLabel {
                    font-size: 11px;
                    color: #7f8c8d;
                }

                QLabel#sectionLabel {
                    font-size: 13px;
                    font-weight: bold;
                    color: #34495e;
                    margin-bottom: 5px;
                }

                QPushButton {
                    background-color: #3498db;
                    color: white;
                    border: none;
                    border-radius: 6px;
                    padding: 6px 12px;
                    font-weight: 500;
                }

                QPushButton:hover {
                    background-color: #2980b9;
                }

                QPushButton:pressed {
                    background-color: #21618c;
                }

                QPushButton#wordButton {
                    background-color: #ecf0f1;
                    color: #2c3e50;
                    border: 1px solid #bdc3c7;
                    font-size: 11px;
                    padding: 4px 8px;
                }

                QPushButton#wordButton:hover {
                    background-color: #d5dbdb;
                }

                QTextEdit {
                    background-color: #ffffff;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    padding: 8px;
                    font-size: 12px;
                }

                QProgressBar {
                    border: 1px solid #bdc3c7;
                    border-radius: 4px;
                    background-color: #ecf0f1;
                }

                QProgressBar::chunk {
                    background-color: #27ae60;
                    border-radius: 3px;
                }

                QSlider::groove:horizontal {
                    height: 4px;
                    background-color: #bdc3c7;
                    border-radius: 2px;
                }

                QSlider::handle:horizontal {
                    background-color: #3498db;
                    width: 16px;
                    height: 16px;
                    border-radius: 8px;
                    margin: -6px 0;
                }
            )";
            break;
    }

    setStyleSheet(styleSheet);
    update();
}

ThemeManager::Theme LanguageLearningOverlay::getCurrentTheme() const
{
    // Detect current theme from application settings
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QString themeName = settings.value("appearance/theme", "Auto (System)").toString();

    ThemeManager::Theme theme = ThemeManager::fromString(themeName);

    // If it's auto, detect from system
    if (theme == ThemeManager::Theme::Auto) {
        auto hints = QGuiApplication::styleHints();
        const bool dark = hints ? hints->colorScheme() == Qt::ColorScheme::Dark : false;
        theme = dark ? ThemeManager::Theme::Dark : ThemeManager::Theme::Light;
    }

    return theme;
}

void LanguageLearningOverlay::onThemeChanged()
{
    applyTheme();
}

QFont LanguageLearningOverlay::getLanguageFont(const QString& langCode, int size)
{
    QFont font;
    font.setPointSize(size);

    // Use appropriate fonts for different languages
    if (langCode == "ja") {
        font.setFamily("MS Gothic, Yu Gothic, NotoSansCJK");
    } else if (langCode == "zh" || langCode == "zh-tw") {
        font.setFamily("Microsoft YaHei, SimHei, NotoSansCJK");
    } else if (langCode == "ko") {
        font.setFamily("Malgun Gothic, Gulim, NotoSansCJK");
    } else if (langCode == "ar") {
        font.setFamily("Arial Unicode MS, Tahoma");
    } else if (langCode == "th") {
        font.setFamily("Leelawadee UI, Tahoma");
    } else {
        font.setFamily("Segoe UI, Arial, sans-serif");
    }

    return font;
}

void LanguageLearningOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 245));
    painter.drawRoundedRect(rect(), 12, 12);

    QWidget::paintEvent(event);
}

void LanguageLearningOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Enable dragging
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
    QWidget::mousePressEvent(event);
}

void LanguageLearningOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }

    QWidget::keyPressEvent(event);
}

bool LanguageLearningOverlay::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseMove && (qobject_cast<QWidget*>(object) == this)) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) {
            move(mouseEvent->globalPosition().toPoint() - m_dragPosition);
        }
    }

    return QWidget::eventFilter(object, event);
}