#include "PermissionsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSpacerItem>
#include <QIcon>
#include <QStyle>
#include <QCoreApplication>
#include <QMessageBox>

#ifdef Q_OS_MACOS
#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#endif

PermissionsDialog::PermissionsDialog(QWidget *parent)
    : QDialog(parent)
    , screenRecordingGranted(false)
    , accessibilityGranted(false)
{
    setupUI();
    updatePermissionStatus();
}

void PermissionsDialog::setupUI()
{
    setWindowTitle("Welcome to Ohao Lang");
    setModal(true);
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Title
    titleLabel = new QLabel("🔐 Setup Permissions");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Description
    descriptionLabel = new QLabel(
        "Ohao Lang needs the following permissions to work properly:\n\n"
        "• <b>Screen Recording</b> - To capture screenshots for OCR\n"
        "• <b>Accessibility</b> - To register global keyboard shortcuts"
    );
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(descriptionLabel);

    // Permissions status group
    QGroupBox *statusGroup = new QGroupBox("Permission Status");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);

    screenRecordingStatus = new QLabel("Screen Recording: Checking...");
    screenRecordingStatus->setStyleSheet("padding: 5px;");
    statusLayout->addWidget(screenRecordingStatus);

    accessibilityStatus = new QLabel("Accessibility: Checking...");
    accessibilityStatus->setStyleSheet("padding: 5px;");
    statusLayout->addWidget(accessibilityStatus);

    mainLayout->addWidget(statusGroup);

    // Instructions
    QLabel *instructionsLabel = new QLabel(
        "<small><i>Click 'Open System Preferences' to grant permissions, "
        "then click 'Check Again' to verify.</i></small>"
    );
    instructionsLabel->setWordWrap(true);
    instructionsLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(instructionsLabel);

    mainLayout->addSpacing(10);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    openPrefsButton = new QPushButton("Open System Preferences");
    openPrefsButton->setMinimumHeight(35);
    connect(openPrefsButton, &QPushButton::clicked, this, &PermissionsDialog::onOpenSystemPreferences);
    buttonLayout->addWidget(openPrefsButton);

    checkButton = new QPushButton("Check Again");
    checkButton->setMinimumHeight(35);
    connect(checkButton, &QPushButton::clicked, this, &PermissionsDialog::onCheckPermissions);
    buttonLayout->addWidget(checkButton);

    continueButton = new QPushButton("Continue");
    continueButton->setMinimumHeight(35);
    continueButton->setDefault(true);
    connect(continueButton, &QPushButton::clicked, this, &PermissionsDialog::onContinue);
    buttonLayout->addWidget(continueButton);

    mainLayout->addLayout(buttonLayout);
}

void PermissionsDialog::updatePermissionStatus()
{
    screenRecordingGranted = checkScreenRecordingPermission();
    accessibilityGranted = checkAccessibilityPermission();

    // Update UI
    if (screenRecordingGranted) {
        screenRecordingStatus->setText("✅ Screen Recording: <b style='color: green;'>Granted</b>");
    } else {
        screenRecordingStatus->setText("❌ Screen Recording: <b style='color: red;'>Not Granted</b>");
    }

    if (accessibilityGranted) {
        accessibilityStatus->setText("✅ Accessibility: <b style='color: green;'>Granted</b>");
    } else {
        accessibilityStatus->setText("❌ Accessibility: <b style='color: red;'>Not Granted</b>");
    }

    // Enable continue button if at least one permission is granted
    // (app can still work with partial permissions)
    continueButton->setEnabled(screenRecordingGranted || accessibilityGranted);

    if (screenRecordingGranted && accessibilityGranted) {
        continueButton->setText("Continue ✓");
        continueButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    }
}

void PermissionsDialog::onOpenSystemPreferences()
{
#ifdef Q_OS_MACOS
    // Open System Preferences to Privacy & Security
    NSString *urlString = @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture";
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]];

    // Also show instructions in a separate message
    QMessageBox::information(this, "Grant Permissions",
        "Please grant the following permissions:\n\n"
        "1. Screen Recording - Enable 'ohao-lang' in the list\n"
        "2. Accessibility - Enable 'ohao-lang' in the list\n\n"
        "You may need to restart the app after granting permissions.\n\n"
        "Click 'Check Again' after granting permissions.");
#endif
}

void PermissionsDialog::onCheckPermissions()
{
    updatePermissionStatus();
}

void PermissionsDialog::onContinue()
{
    markAsShown();
    accept();
}

bool PermissionsDialog::shouldShow()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return !settings.value("permissions/dialog_shown", false).toBool();
}

void PermissionsDialog::markAsShown()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("permissions/dialog_shown", true);
}

bool PermissionsDialog::checkScreenRecordingPermission()
{
#ifdef Q_OS_MACOS
    // Try to check screen recording permission by attempting to capture
    // This will trigger the permission prompt if not granted
    if (@available(macOS 10.15, *)) {
        // Request screen capture permission
        CGImageRef screenshot = CGWindowListCreateImage(
            CGRectMake(0, 0, 1, 1),
            kCGWindowListOptionOnScreenOnly,
            kCGNullWindowID,
            kCGWindowImageDefault
        );

        if (screenshot != NULL) {
            CFRelease(screenshot);
            return true;
        }
        return false;
    }
    return true; // Assume granted on older macOS
#else
    return true; // Not macOS
#endif
}

bool PermissionsDialog::checkAccessibilityPermission()
{
#ifdef Q_OS_MACOS
    // Check if we have accessibility permissions
    NSDictionary *options = @{(__bridge id)kAXTrustedCheckOptionPrompt: @YES};
    Boolean hasAccess = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
    return hasAccess;
#else
    return true; // Not macOS
#endif
}
