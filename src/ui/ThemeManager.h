#pragma once

#include <QObject>
#include <QPalette>
#include <QSettings>
#include <QString>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Theme {
        Auto,
        Light,
        Dark,
        HighContrast,
        Cyberpunk
    };

    // Singleton access
    static ThemeManager& instance();

    // Theme management
    static Theme fromString(const QString &name);
    static QString toString(Theme theme);

    void applyTheme(Theme theme);
    void applyFromSettings();
    void saveToSettings(Theme theme);

    // Get current theme colors
    QPalette getCurrentPalette() const;
    Theme getCurrentTheme() const;

signals:
    void themeChanged(Theme newTheme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() = default;

    // Singleton management
    static ThemeManager* s_instance;

    // Theme application methods
    void applyLight();
    void applyDark();
    void applyHighContrast();
    void applyCyberpunk();

    // Current state
    Theme m_currentTheme = Theme::Auto;
    QPalette m_currentPalette;
};
