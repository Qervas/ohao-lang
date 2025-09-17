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

    explicit ThemeManager(QObject *parent = nullptr);

    static Theme fromString(const QString &name);
    static QString toString(Theme theme);

    static void applyTheme(Theme theme);
    static void applyFromSettings();
    static void saveToSettings(Theme theme);

private:
    static void applyLight();
    static void applyDark();
    static void applyHighContrast();
    static void applyCyberpunk();
};
