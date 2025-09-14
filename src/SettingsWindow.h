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

private:
    void setupUI();
    void setupOcrTab();
    void setupTranslationTab();
    void setupAppearanceTab();
    void applyModernStyling();
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    void animateShow();
    void animateHide();

    // UI Components
    QTabWidget *tabWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;

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

    // Buttons
    QPushButton *applyBtn;
    QPushButton *cancelBtn;
    QPushButton *resetBtn;

    // Animation
    QPropertyAnimation *showAnimation;
    QGraphicsOpacityEffect *opacityEffect;

    // Settings
    QSettings *settings;
};