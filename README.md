# 🌟 Ohao Language Learner

[![Windows](https://img.shields.io/badge/Windows-10%2F11-blue?logo=windows)](https://github.com/Qervas/ohao-lang)
[![Qt](https://img.shields.io/badge/Qt-6.9.2-green?logo=qt)](https://qt.io/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.0.0-red)](https://github.com/Qervas/ohao-lang/releases)

A powerful, portable OCR and translation tool with integrated Text-to-Speech functionality. Perfect for language learners, professionals, and anyone working with multilingual content.

## ✨ Features

### 🔍 **Advanced OCR (Optical Character Recognition)**

- Real-time text recognition from screenshots
- Intelligent paragraph merging for better text flow
- Support for multiple languages including **Swedish**
- High accuracy with Tesseract OCR engine
- Auto-detection of indented paragraphs

### 🌐 **Multi-Platform Translation**

- **Google Translate** integration
- **DeepL** translation support
- Swedish ↔ English translation
- Automatic language detection
- Context-aware translations

### 🗣️ **Advanced Text-to-Speech (TTS)**

- Multiple TTS backends (SAPI, WinRT, Cloud)
- **Swedish voice support** with native speakers
- Dynamic voice fetching from cloud providers
- Separate input/output voice selection
- Bilingual TTS capabilities

### 🎯 **User Experience**

- **Global shortcuts** (Ctrl+Shift+S, Ctrl+Shift+H)
- **System tray integration** with context menu
- Floating widget with modern UI
- **Portable executable** - no installation required
- Settings persistence across sessions

## 🚀 Quick Start

### Option 1: Download Portable Release (Recommended)

1. **Download** the latest release: [`ohao-lang-portable-v1.0.0.zip`](https://github.com/Qervas/ohao-lang/releases)
2. **Extract** to any folder (USB drive, Desktop, etc.)
3. **Install OCR dependencies** (see below)
4. **Run** `ohao-lang.exe` or `Start-OhaoLang.bat`

### Option 2: Build from Source

```bash
# Clone the repository
git clone https://github.com/Qervas/ohao-lang.git
cd ohao-lang

# Configure and build (Windows)
configure.bat
cmake --build build --config Release
```

## 📋 Requirements

### System Requirements

- **OS:** Windows 10/11 (64-bit)
- **RAM:** 4GB minimum, 8GB recommended
- **Storage:** 150MB for application + OCR data
- **Network:** Internet connection for translation services

### 🔧 OCR Setup (Required)

#### **Tesseract OCR Installation**

**Method 1: Scoop (Recommended)**

```powershell
# Install Scoop package manager (if not already installed)
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex

# Install Tesseract OCR
scoop install tesseract
```

**Method 2: Official Installer**

1. Download from: https://github.com/UB-Mannheim/tesseract/wiki
2. Install `tesseract-ocr-w64-setup-5.3.x.exe`
3. Add to PATH: `C:\Program Files\Tesseract-OCR`

**Method 3: Chocolatey**

```powershell
choco install tesseract
```

#### **Language Data Installation**

The application supports multiple languages. Install additional language packs:

```powershell
# For Swedish support (recommended)
scoop install tesseract-languages
# OR download from: https://github.com/tesseract-ocr/tessdata
```

**Supported OCR Languages:**

- English (`eng`) - Default
- Swedish (`swe`) - Full support
- German (`deu`)
- French (`fra`)
- Spanish (`spa`)
- Chinese (`chi_sim`, `chi_tra`)
- Japanese (`jpn`)

## 🎮 Usage

### Global Shortcuts

- **Ctrl + Shift + S** - Take screenshot for OCR
- **Ctrl + Shift + H** - Show/hide floating widget

### System Tray Menu

Right-click the system tray icon:

- 📷 **Take Screenshot** - Capture screen area for OCR
- 👁️ **Toggle Visibility** - Show/hide main widget
- ⚙️ **Settings** - Open configuration window
- ❌ **Quit** - Exit application

### Workflow

1. **Take Screenshot** (Ctrl+Shift+S or click camera button)
2. **Select text area** by dragging mouse
3. **Auto-OCR** extracts text from selection
4. **Translation** happens automatically (if enabled)
5. **TTS playback** reads the text aloud (if enabled)

## ⚙️ Configuration

### General Settings

- **Input Language** - OCR recognition language
- **Output Language** - Translation target language
- **TTS Options** - Enable input/output speech
- **Voice Selection** - Choose specific voices

### Translation Backends

- **Google Translate** - Free, good quality
- **DeepL** - Premium quality (API key required)

### TTS Backends

- **System (SAPI)** - Windows built-in voices
- **Cloud (Edge)** - High-quality neural voices
- **WinRT** - Modern Windows TTS engine

## 🛠️ Development

### Build Requirements

- **Qt 6.9.2** or later
- **CMake 3.16** or later
- **Visual Studio 2022** (MSVC compiler)
- **vcpkg** (for dependencies)

### Project Structure

```
ohao-lang/
├── src/                    # Source code
│   ├── FloatingWidget.*    # Main UI widget
│   ├── OCREngine.*         # OCR functionality
│   ├── TranslationEngine.* # Translation backends
│   ├── TTSEngine.*         # Text-to-Speech
│   └── SystemTray.*        # System tray integration
├── build/                  # Build outputs
├── CMakeLists.txt          # Build configuration
└── README.md              # This file
```

### Building

```bash
# Debug build (with console)
cmake --build build --config Debug

# Release build (no console)
cmake --build build --config Release

# Create portable release
powershell -ExecutionPolicy Bypass -File "create-release.ps1"
```

## 🌍 Language Support

### OCR Languages

| Language | Code            | Status     | Notes                  |
| -------- | --------------- | ---------- | ---------------------- |
| English  | `eng`         | ✅ Full    | Default language       |
| Swedish  | `swe`         | ✅ Full    | Native support         |
| German   | `deu`         | ✅ Good    | Requires language pack |
| French   | `fra`         | ✅ Good    | Requires language pack |
| Spanish  | `spa`         | ✅ Good    | Requires language pack |
| Chinese  | `chi_sim/tra` | ⚠️ Basic | Traditional/Simplified |
| Japanese | `jpn`         | ⚠️ Basic | Requires language pack |

### Translation Support

- **Google Translate:** 100+ languages
- **DeepL:** 30+ languages (premium quality)
- **Bidirectional:** Swedish ↔ English optimized

### TTS Voice Support

- **English:** Microsoft David/Zira + Cloud voices
- **Swedish:** SofieNeural, MattiasNeural + more
- **Multi-language:** 40+ languages via cloud TTS

## 🔧 Troubleshooting

### Common Issues

#### "Tesseract not found"

```bash
# Check if Tesseract is in PATH
tesseract --version

# If not found, add to PATH or reinstall
scoop install tesseract
```

#### "No OCR results"

- Ensure text is clear and high contrast
- Try different languages in settings
- Check if Tesseract language pack is installed

#### "Translation failed"

- Verify internet connection
- Check if API keys are configured (DeepL)
- Try switching translation backend

#### "TTS not working"

- Check audio output device
- Verify TTS backend in settings
- Test with different voices

### Debug Mode

For troubleshooting, build with console enabled:

```bash
cmake -DENABLE_CONSOLE=ON --build build --config Release
```

## 📦 Distribution

### Portable Release Features

- **Zero installation** - extract and run
- **Self-contained** - all Qt libraries included
- **USB portable** - runs from removable media
- **Settings preserved** - uses Windows registry
- **Silent execution** - no console window

### File Structure

```
ohao-lang-portable-v1.0.0/
├── ohao-lang.exe           # Main executable (480KB)
├── Start-OhaoLang.bat      # Convenience launcher
├── README.txt              # User instructions
├── Qt6*.dll               # Qt runtime libraries (~70MB)
├── platforms/             # Qt platform plugins
├── translations/          # UI translations
└── [other Qt plugins]     # Multimedia, TTS, etc.
```

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines

- Follow Qt coding conventions
- Add tests for new features
- Update documentation
- Ensure cross-platform compatibility

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Qt Framework** - Cross-platform application framework
- **Tesseract OCR** - Open source OCR engine
- **Google Translate** - Translation services
- **DeepL** - Premium translation API
- **FFmpeg** - Multimedia framework

## 📞 Support

- **Issues:** [GitHub Issues](https://github.com/Qervas/ohao-lang/issues)
- **Contact:** [Contact Developer](https://ohao.tech/contact)

## 🔗 Links

- **Documentation:** https://docs.ohao-lang.dev
- **Releases:** https://github.com/Qervas/ohao-lang/releases
- **Qt Framework:** https://qt.io
- **Tesseract OCR:** https://github.com/tesseract-ocr/tesseract
