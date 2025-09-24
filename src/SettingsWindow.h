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
#include <QSpinBox>
#include <QTextEdit>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QSettings>
#include <QScrollArea>
#include <QFrame>
#include <QListWidget>

class TTSEngine;

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    ~SettingsWindow();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onOcrEngineChanged();
    void onTranslationEngineChanged();
    void onApplyClicked();
    void onCancelClicked();
    void onResetClicked();
    void onTestOcrClicked();
    void onTestTranslationClicked();
    void onVoiceChanged();
    void onTestTTSClicked();
    void onEdgeAutoDetect();

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

    // UI Components
    QTabWidget *tabWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;

    // General Tab
    QWidget *generalTab;
    QCheckBox *ttsInputCheckBox;
    QCheckBox *ttsOutputCheckBox;

    // OCR Tab
    QWidget *ocrTab;
    QComboBox *ocrEngineCombo;
    QComboBox *ocrLanguageCombo;
    QSlider *ocrQualitySlider;
    QCheckBox *ocrPreprocessingCheck;
    QCheckBox *ocrAutoDetectCheck;
    QPushButton *testOcrBtn;
    QTextEdit *ocrStatusText;

    // Translation Tab
    QWidget *translationTab;
    QCheckBox *autoOcrCheck;
    QCheckBox *autoTranslateCheck;
    QComboBox *translationEngineCombo;
    QComboBox *sourceLanguageCombo;
    QComboBox *targetLanguageCombo;
    QLineEdit *apiKeyEdit;
    QLineEdit *apiUrlEdit;
    QPushButton *testTranslationBtn;
    QTextEdit *translationStatusText;

    // Appearance Tab
    QWidget *appearanceTab;
    QComboBox *themeCombo;
    QSlider *opacitySlider;
    QCheckBox *animationsCheck;
    QCheckBox *soundsCheck;

    // TTS Tab
    QWidget *ttsTab;
    QCheckBox *ttsEnabledCheck;
    QComboBox *voiceCombo;
    QComboBox *inputVoiceCombo;   // For input text (OCR language)
    QComboBox *outputVoiceCombo;  // For output text (translation language)
    QComboBox *ttsBackendCombo; // System or Azure
    QSlider *volumeSlider;
    QSlider *pitchSlider;
    QSlider *rateSlider;
    QLineEdit *testTextEdit;
    QPushButton *testTTSBtn;
    QPushButton *stopTTSBtn;
    QTextEdit *ttsStatusText;

    // Azure config controls
    QGroupBox *azureConfigGroup;
    QLineEdit *azureRegionEdit;
    QLineEdit *azureKeyEdit;
    QLineEdit *azureStyleEdit;

    // Google config controls
    QGroupBox *googleConfigGroup;
    QLineEdit *googleApiKeyEdit;
    QLineEdit *googleLanguageCodeEdit;

    // ElevenLabs config controls
    QGroupBox *elevenConfigGroup;
    QLineEdit *elevenApiKeyEdit;
    QLineEdit *elevenVoiceIdEdit;

    // Polly config controls
    QGroupBox *pollyConfigGroup;
    QLineEdit *pollyRegionEdit;
    QLineEdit *pollyAccessKeyEdit;
    QLineEdit *pollySecretKeyEdit;

    // Piper config controls (free)
    QGroupBox *piperConfigGroup;
    QLineEdit *piperExeEdit;
    QLineEdit *piperModelEdit;

    // Edge (free online) config controls
    QGroupBox *edgeConfigGroup;
    QLineEdit *edgeExeEdit;
    QLineEdit *edgeVoiceEdit;
    QPushButton *edgeAutoDetectBtn;

    // Buttons
    QPushButton *applyBtn;
    QPushButton *cancelBtn;
    QPushButton *resetBtn;

    // Animation
    QPropertyAnimation *showAnimation;
    QGraphicsOpacityEffect *opacityEffect;

    // Settings
    QSettings *settings;

    // TTS Engine
    TTSEngine *ttsEngine;

    // Language filtering
    // Language now follows Translation target language; no separate picker
};