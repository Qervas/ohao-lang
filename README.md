<div align="center">
  <img src="resources/icon-128.png" alt="Ohao Language Learner" width="128" height="128">

  # Ohao Language Learner

  [![macOS](https://img.shields.io/badge/macOS-12.0+-000000?logo=apple)](https://github.com/Qervas/ohao-lang)
  [![Windows](https://img.shields.io/badge/Windows-10%2F11-blue?logo=windows)](https://github.com/Qervas/ohao-lang)
  [![Qt](https://img.shields.io/badge/Qt-6.9.2-green?logo=qt)](https://qt.io/)
  [![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
  [![Release](https://img.shields.io/badge/Release-v1.0.2-red)](https://github.com/Qervas/ohao-lang/releases)

  **OCR and translation tool with Text-to-Speech**

  [Download for macOS](https://github.com/Qervas/ohao-lang/releases/latest) • [Download for Windows](https://github.com/Qervas/ohao-lang/releases/tag/v1.0.1)
</div>

---

## ✨ Features

### 🔍 OCR
- Apple Vision OCR on macOS (built-in)
- Tesseract OCR on Windows/Linux
- Online OCR fallback
- Multi-language support (English, Swedish, German, French, Spanish, Chinese, Japanese)

### 🌐 Translation
- Google Translate
- DeepL
- Auto language detection

### 🗣️ Text-to-Speech
- System voices (macOS/Windows)
- Cloud voices (Edge TTS)
- Multiple language support

### ⚡ Features
- Global shortcuts (customizable)
- Menu bar/system tray integration
- Cross-platform (macOS 12.0+, Windows 10/11)

## 📋 Changelog

### v1.0.2 (2025-10-14)
- First macOS release with Apple Vision OCR
- Fixed screenshot fullscreen bug
- Auto-save settings
- Native DMG installer

### v1.0.1 (2025-09-28)
- Dynamic TTS voice discovery
- Quick translation overlay
- Settings window
- Single instance support

### v1.0.0 (2025-09-24)
- Initial Windows release

## 🚀 Quick Start

### macOS

1. Download [`ohao-lang-installer.dmg`](https://github.com/Qervas/ohao-lang/releases/latest)
2. Open DMG and drag app to Applications
3. Launch and grant Screen Recording permission
4. Done - no dependencies needed

### Windows

1. Download from [v1.0.1 release](https://github.com/Qervas/ohao-lang/releases/tag/v1.0.1)
2. Install Tesseract OCR (see below)
3. Run the app

### Build from Source

```bash
# Clone the repository
git clone https://github.com/Qervas/ohao-lang.git
cd ohao-lang

# macOS/Linux
cmake -B build
cmake --build build

# Windows
configure.bat
cmake --build build --config Release
```

## 📋 Requirements

**macOS:** 12.0+, 50MB disk space  
**Windows:** 10/11, 150MB + Tesseract OCR

### Tesseract Setup (Windows only)

```powershell
# Using Scoop
scoop install tesseract
```

Or download from [tesseract-ocr](https://github.com/UB-Mannheim/tesseract/wiki)

## 🎮 Usage

**Take Screenshot:** Cmd+Shift+X (macOS) / Ctrl+Shift+S (Windows)  
**Toggle Widget:** Cmd+Shift+Z (macOS) / Ctrl+Shift+H (Windows)

1. Take screenshot
2. Select text area
3. OCR extracts text automatically
4. Translation and TTS available

## ⚙️ Configuration

Settings available in app:
- OCR engine selection
- Translation backend (Google/DeepL)
- TTS voices
- Global shortcuts

## 📄 License

MIT License - see [LICENSE](LICENSE)

## 🔗 Links

- [Issues](https://github.com/Qervas/ohao-lang/issues)
- [Releases](https://github.com/Qervas/ohao-lang/releases)
