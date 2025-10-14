#pragma once

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QSettings>

/**
 * @brief Dialog shown on first launch to guide users through macOS permissions setup
 * 
 * This dialog helps users enable required permissions for:
 * - Screen Recording (for OCR screenshots)
 * - Accessibility (for global keyboard shortcuts)
 */
class PermissionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PermissionsDialog(QWidget *parent = nullptr);
    
    /**
     * @brief Check if this is the first launch and permissions dialog should be shown
     * @return true if dialog should be shown
     */
    static bool shouldShow();
    
    /**
     * @brief Mark that the permissions dialog has been shown
     */
    static void markAsShown();
    
    /**
     * @brief Check if Screen Recording permission is granted (macOS only)
     * @return true if permission is granted
     */
    static bool checkScreenRecordingPermission();
    
    /**
     * @brief Check if Accessibility permission is granted (macOS only)
     * @return true if permission is granted
     */
    static bool checkAccessibilityPermission();

private slots:
    void onOpenSystemPreferences();
    void onCheckPermissions();
    void onContinue();

private:
    void setupUI();
    void updatePermissionStatus();
    
    QLabel *titleLabel;
    QLabel *descriptionLabel;
    QLabel *screenRecordingStatus;
    QLabel *accessibilityStatus;
    QPushButton *openPrefsButton;
    QPushButton *checkButton;
    QPushButton *continueButton;
    
    bool screenRecordingGranted;
    bool accessibilityGranted;
};
