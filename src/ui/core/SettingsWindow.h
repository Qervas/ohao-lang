#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QGroupBox>
#include <QSlider>
#include <QKeySequenceEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSettings>
#include <QScrollArea>
#include <QFrame>
#include <QListWidget>
#include <QList>
#include <QHash>
#include <QNetworkAccessManager>

class TTSEngine;
class GlobalShortcutManager;
class SystemTray;

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    ~SettingsWindow();
    
    void setShortcutManager(GlobalShortcutManager *manager);
    void setSystemTray(SystemTray *tray);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onOcrEngineChanged();
    void onTranslationEngineChanged();
    void onResetClicked();
    void onVoiceChanged();
    void onTestTTSClicked();

private:
    void setupUI();
    void setupGeneralTab();
    void setupOcrTab();
    void setupTranslationTab();
    void setupAppearanceTab();
    void setupTTSTab();
    void applyModernStyling();
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    void animateShow();
    void animateHide();
    void updateVoicesForLanguage();
    void updateProviderUI(const QString& providerId);
    void checkEdgeTTSAvailability();
    QString getTestTextForLanguage(const QString& languageName, bool isInputVoice) const;
    QLocale languageNameToLocale(const QString& languageName) const;
    void updateScreenshotPreview();

    // UI Components
    QTabWidget *tabWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;

    // General Tab
    QWidget *generalTab;
    QSlider *screenshotDimmingSlider;
    QLabel *screenshotDimmingValue;
    QLabel *screenshotPreview;
    QKeySequenceEdit *screenshotShortcutEdit;
    QKeySequenceEdit *toggleShortcutEdit;

    // OCR Tab
    QWidget *ocrTab;
    QComboBox *ocrEngineCombo;
    QComboBox *ocrLanguageCombo;

    // Translation Tab
    QWidget *translationTab;
    QCheckBox *autoTranslateCheck;
    QComboBox *overlayModeCombo;
    QComboBox *translationEngineCombo;
    QComboBox *sourceLanguageCombo;
    QComboBox *targetLanguageCombo;
    QCheckBox *autoDetectSourceCheck;
    QLineEdit *apiKeyEdit;
    QLineEdit *apiUrlEdit;
    QTextEdit *translationStatusText;

    // Appearance Tab
    QWidget *appearanceTab;
    QComboBox *themeCombo;
    QComboBox *widgetSizeCombo;
    QSlider *widgetWidthSlider;
    QLabel *widgetWidthLabel;
    QSlider *opacitySlider;
    QCheckBox *animationsCheck;
    QCheckBox *soundsCheck;

    // TTS Tab
    QWidget *ttsTab { nullptr };
    QCheckBox *ttsEnabledCheck { nullptr };
    QComboBox *ttsProviderCombo { nullptr };
    QLabel *providerInfoLabel { nullptr };
    QComboBox *voiceCombo { nullptr };
    QComboBox *inputVoiceCombo { nullptr };   // For input text (OCR language)
    QComboBox *outputVoiceCombo { nullptr };  // For output text (translation language)
    QLabel *ttsProviderLabel { nullptr };
    QCheckBox *advancedVoiceToggle { nullptr };
    QPushButton *refreshVoicesButton { nullptr };
    QLabel *inputVoiceLabel { nullptr };
    QLabel *outputVoiceLabel { nullptr };
    QLineEdit *testTextEdit { nullptr };
    QPushButton *testTTSBtn { nullptr };
    QPushButton *stopTTSBtn { nullptr };
    QTextEdit *ttsStatusText { nullptr };
    QLabel *edgeExeLabel { nullptr };
    QWidget *edgeExeRow { nullptr };
    QLabel *edgeHintLabel { nullptr };

    // Provider extras
    QLineEdit *edgeExePathEdit { nullptr };
    QPushButton *edgeBrowseButton { nullptr };

    // Buttons
    QPushButton *resetBtn;

    // Animation
    QPropertyAnimation *showAnimation;
    QGraphicsOpacityEffect *opacityEffect;

    // Settings
    QSettings *settings;

    // TTS Engine
    TTSEngine *ttsEngine;
    
    // Global Shortcut Manager
    GlobalShortcutManager *shortcutManager = nullptr;
    
    // System Tray
    SystemTray *systemTray = nullptr;

    // Language filtering
    // Language now follows Translation target language; no separate picker

    struct VoiceOption {
        QString label;
        QString id;
    };
    QHash<QString, QList<VoiceOption>> voiceCache;
    QHash<QString, bool> voiceFetchInFlight;
    QNetworkAccessManager *ttsNetworkManager { nullptr };
};
