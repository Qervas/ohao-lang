#pragma once

#include <QColor>
#include <QString>

/**
 * Centralized theme color definitions for all UI components.
 * This eliminates duplicate color definitions across the codebase.
 */
namespace ThemeColors {

    // Light Theme Colors
    namespace Light {
        // Base colors
        const QColor Window(250, 252, 255);
        const QColor WindowText(45, 55, 72);
        const QColor Base(255, 255, 255);
        const QColor AlternateBase(245, 247, 250);
        const QColor Text(45, 55, 72);
        const QColor Button(245, 247, 250);
        const QColor ButtonText(45, 55, 72);
        const QColor Highlight(100, 150, 250);
        const QColor HighlightedText(255, 255, 255);

        // Component-specific colors
        const QColor FloatingWidgetBg(255, 255, 255, 205);
        const QColor FloatingWidgetBorder(190, 195, 205, 165);
        const QColor FloatingWidgetHighlight(74, 144, 226, 127);
        const QColor ButtonHover(0, 0, 0, 20);
        const QColor ButtonPressed(0, 0, 0, 38);

        // Screenshot toolbar colors
        const QColor ScreenshotToolbarBg(240, 242, 245, 240);
        const QColor ScreenshotToolbarBorder(190, 195, 205, 150);
        const QColor ScreenshotButtonBg(230, 233, 239, 200);
        const QColor ScreenshotButtonHover(210, 215, 225, 230);
        const QColor ScreenshotButtonPressed(190, 195, 205, 240);

        // Status colors
        const QColor Success(40, 167, 69);
        const QColor Error(220, 53, 69);

        // Tooltip colors
        const QColor ToolTipBase(255, 255, 220);
        const QColor ToolTipText(45, 55, 72);
    }

    // Dark Theme Colors
    namespace Dark {
        // Base colors
        const QColor Window(28, 32, 38);
        const QColor WindowText(230, 233, 239);
        const QColor Base(22, 26, 31);
        const QColor AlternateBase(32, 36, 43);
        const QColor Text(230, 233, 239);
        const QColor Button(38, 43, 50);
        const QColor ButtonText(230, 233, 239);
        const QColor Highlight(80, 130, 230);
        const QColor HighlightedText(255, 255, 255);

        // Component-specific colors
        const QColor FloatingWidgetBg(44, 49, 58, 212);
        const QColor FloatingWidgetBorder(58, 64, 75);
        const QColor FloatingWidgetHighlight(80, 130, 230);
        const QColor ButtonHover(48, 54, 64);
        const QColor ButtonPressed(37, 42, 49);

        // Screenshot toolbar colors
        const QColor ScreenshotToolbarBg(40, 42, 52, 230);
        const QColor ScreenshotToolbarBorder(58, 64, 75, 150);
        const QColor ScreenshotButtonBg(48, 54, 64, 200);
        const QColor ScreenshotButtonHover(58, 64, 75, 230);
        const QColor ScreenshotButtonPressed(37, 42, 49, 240);

        // Status colors
        const QColor Success(76, 202, 106);
        const QColor Error(255, 90, 95);

        // Tooltip colors
        const QColor ToolTipBase(60, 60, 70);
        const QColor ToolTipText(240, 240, 240);
    }


    // Cyberpunk Theme Colors
    namespace Cyberpunk {
        // Base colors
        const QColor Window(10, 12, 20);
        const QColor WindowText(255, 255, 255);
        const QColor Base(8, 10, 18);
        const QColor AlternateBase(14, 16, 26);
        const QColor Text(230, 255, 252);
        const QColor Button(14, 16, 26);
        const QColor ButtonText(255, 255, 255);
        const QColor Highlight(0, 255, 208);
        const QColor HighlightedText(10, 12, 20);

        // Component-specific colors
        const QColor FloatingWidgetBg(14, 16, 26, 225);
        const QColor FloatingWidgetBorder(255, 0, 128, 115);
        const QColor FloatingWidgetHighlight(0, 255, 208);
        const QColor ButtonHover(255, 0, 128, 46);
        const QColor ButtonPressed(0, 255, 208, 64);

        // Screenshot toolbar colors
        const QColor ScreenshotToolbarBg(14, 16, 26, 235);
        const QColor ScreenshotToolbarBorder(255, 0, 128, 102);
        const QColor ScreenshotButtonBg(18, 20, 32, 200);
        const QColor ScreenshotButtonHover(255, 0, 128, 80);
        const QColor ScreenshotButtonPressed(0, 255, 208, 90);

        // Accent colors
        const QColor Teal(0, 255, 208);
        const QColor Magenta(255, 0, 128);

        // Status colors
        const QColor Success(0, 255, 208);
        const QColor Error(255, 71, 156);

        // Tooltip colors
        const QColor ToolTipBase(20, 24, 40);
        const QColor ToolTipText(255, 255, 255);
    }

    // Helper function to get colors by theme name
    struct ThemeColorSet {
        QColor window;
        QColor windowText;
        QColor base;
        QColor alternateBase;
        QColor text;
        QColor button;
        QColor buttonText;
        QColor highlight;
        QColor highlightedText;
        QColor floatingWidgetBg;
        QColor floatingWidgetBorder;
        QColor floatingWidgetHighlight;
        QColor buttonHover;
        QColor buttonPressed;
        QColor screenshotToolbarBg;
        QColor screenshotToolbarBorder;
        QColor screenshotButtonBg;
        QColor screenshotButtonHover;
        QColor screenshotButtonPressed;
        QColor success;
        QColor error;
        QColor toolTipBase;
        QColor toolTipText;
    };

    inline ThemeColorSet getColorSet(const QString& themeName) {
        QString theme = themeName.toLower();

        if (theme.contains("dark")) {
            return {
                Dark::Window, Dark::WindowText, Dark::Base, Dark::AlternateBase,
                Dark::Text, Dark::Button, Dark::ButtonText, Dark::Highlight,
                Dark::HighlightedText, Dark::FloatingWidgetBg, Dark::FloatingWidgetBorder,
                Dark::FloatingWidgetHighlight, Dark::ButtonHover, Dark::ButtonPressed,
                Dark::ScreenshotToolbarBg, Dark::ScreenshotToolbarBorder,
                Dark::ScreenshotButtonBg, Dark::ScreenshotButtonHover,
                Dark::ScreenshotButtonPressed, Dark::Success, Dark::Error,
                Dark::ToolTipBase, Dark::ToolTipText
            };
        } else if (theme.contains("cyber")) {
            return {
                Cyberpunk::Window, Cyberpunk::WindowText, Cyberpunk::Base,
                Cyberpunk::AlternateBase, Cyberpunk::Text, Cyberpunk::Button,
                Cyberpunk::ButtonText, Cyberpunk::Highlight, Cyberpunk::HighlightedText,
                Cyberpunk::FloatingWidgetBg, Cyberpunk::FloatingWidgetBorder,
                Cyberpunk::FloatingWidgetHighlight, Cyberpunk::ButtonHover,
                Cyberpunk::ButtonPressed, Cyberpunk::ScreenshotToolbarBg,
                Cyberpunk::ScreenshotToolbarBorder, Cyberpunk::ScreenshotButtonBg,
                Cyberpunk::ScreenshotButtonHover, Cyberpunk::ScreenshotButtonPressed,
                Cyberpunk::Success, Cyberpunk::Error,
                Cyberpunk::ToolTipBase, Cyberpunk::ToolTipText
            };
        } else {
            // Default to Light theme
            return {
                Light::Window, Light::WindowText, Light::Base, Light::AlternateBase,
                Light::Text, Light::Button, Light::ButtonText, Light::Highlight,
                Light::HighlightedText, Light::FloatingWidgetBg, Light::FloatingWidgetBorder,
                Light::FloatingWidgetHighlight, Light::ButtonHover, Light::ButtonPressed,
                Light::ScreenshotToolbarBg, Light::ScreenshotToolbarBorder,
                Light::ScreenshotButtonBg, Light::ScreenshotButtonHover,
                Light::ScreenshotButtonPressed, Light::Success, Light::Error,
                Light::ToolTipBase, Light::ToolTipText
            };
        }
    }

} // namespace ThemeColors
