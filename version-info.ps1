# Ohao-Lang Version Information
# This file contains version and build information

$script:VersionInfo = @{
    Version = "1.0.1"
    BuildDate = "2025-09-28"
    Author = "Ohao Development Team"
    Description = "OCR and Translation Tool with TTS Support"
    Homepage = "https://github.com/Qervas/ohao-lang"
    SupportedLanguages = @("English", "Swedish", "German", "French", "Spanish", "Chinese", "Japanese")
    Features = @(
        "Real-time OCR (Optical Character Recognition)",
        "Multi-language translation (Google Translate, DeepL)",
        "Text-to-Speech with multiple voice backends",
        "Swedish language support",
        "Intelligent paragraph merging",
        "Global keyboard shortcuts (Ctrl+Shift+S, Ctrl+Shift+H)",
        "System tray integration",
        "Portable executable"
    )
    Requirements = @(
        "Windows 10/11 (64-bit)",
        "Internet connection for translation services",
        "Tesseract OCR (auto-downloaded if needed)"
    )
    License = "MIT License"
}

# Export version info for other scripts
if ($args.Count -eq 0) {
    Write-Host "Ohao Language Learner v$($VersionInfo.Version)" -ForegroundColor Cyan
    Write-Host "Build Date: $($VersionInfo.BuildDate)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Features:" -ForegroundColor Green
    $VersionInfo.Features | ForEach-Object { Write-Host "  • $_" -ForegroundColor Yellow }
    Write-Host ""
    Write-Host "Requirements:" -ForegroundColor Green
    $VersionInfo.Requirements | ForEach-Object { Write-Host "  • $_" -ForegroundColor Yellow }
}

return $VersionInfo