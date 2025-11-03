<div align="center">
  <img src="resources/icon-128.png" alt="Ohao Language Learner" width="128" height="128">

  # Ohao Language Learner

  [![macOS](https://img.shields.io/badge/macOS-12.0+-000000?logo=apple)](https://github.com/Qervas/ohao-lang)
  [![Windows](https://img.shields.io/badge/Windows-10%2F11-blue?logo=windows)](https://github.com/Qervas/ohao-lang)
  [![Qt](https://img.shields.io/badge/Qt-6.9.2-green?logo=qt)](https://qt.io/)
  [![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
  [![Release](https://img.shields.io/badge/Release-v1.0.3-red)](https://github.com/Qervas/ohao-lang/releases)

  **OCR, translation, and AI-powered language learning tool**

  [Download for macOS (v1.0.2)](https://github.com/Qervas/ohao-lang/releases/tag/v1.0.2) • [Download for Windows (v1.0.3)](https://github.com/Qervas/ohao-lang/releases/tag/v1.0.3)
</div>

---

## ✨ Features

### 🔍 OCR
- Apple Vision OCR on macOS (built-in)
- Tesseract OCR with LSTM neural network on Windows/Linux
- **Hunspell spellcheck** - Post-OCR correction for 15 languages
- Online OCR fallback
- Multi-language support (18+ languages)
- Natural reading order (left-to-right, top-to-bottom)

### 🌐 Translation
- Google Translate
- DeepL
- Auto language detection

### 🗣️ Text-to-Speech
- **Edge TTS** - High-quality Microsoft voices (free, online)
- **System TTS** - Built-in voices (offline, no setup)
- **Google Web TTS** - Free online alternative
- Language-specific test sentences for 38 languages
- Voice persistence and per-language configuration

### 🤖 AI Assistant (Beta)
- **Dual-mode chat** - Translation mode + AI Assistant mode
- **GitHub Copilot API** integration via [copilot-api](https://github.com/ericc-ch/copilot-api)
- Context-aware responses with conversation history
- **28 available models** - 4 free models: gpt-4o, gpt-4.1, grok-code-fast-1, gpt-5-mini
- Token usage tracking for transparency
- Auto-fallback to translation when AI unavailable
- **External service** - Requires separate copilot-api setup

### ⚡ Features
- Global shortcuts (customizable)
- Menu bar/system tray integration
- Cross-platform (macOS 12.0+, Windows 10/11)
- Interactive translation chat window
- Draggable floating widget

### 📖 Hunspell Spellcheck

Automatic post-OCR spelling correction with dictionaries for 15 languages:

- English, Spanish, French, German, Russian
- Portuguese, Italian, Dutch, Polish, Swedish
- Vietnamese, Ukrainian, Danish, Norwegian, Turkish

**How it works:**
1. Tesseract performs OCR with LSTM neural network
2. Language-specific character cleanup removes foreign diacritics
3. Hunspell corrects misspelled words using 100,000+ word dictionaries
4. Result: Higher accuracy for diacritics (ä, ö, å, é, ñ, etc.)

## 📋 Changelog

### v1.0.3 (2025-10-25) - **Windows Only**
- **All-in-one package** - Includes Tesseract OCR + all dependencies
- **Modern TTS System** - Edge TTS, System TTS, Google Web TTS
- **Enhanced UX** - Language-specific test sentences (38 languages)
- **Unified Themes** - Light, Dark, Auto (System)
- **Bug Fixes** - Theme dropdown, overlay brightness, TTS voice selection
- **Easy Setup** - One-click Edge-TTS installer script
- See [CHANGELOG.md](CHANGELOG.md) for complete details
- **Note:** macOS remains at v1.0.2

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

1. Download [`OhaoLang-v1.0.3-Windows-x64.zip`](https://github.com/Qervas/ohao-lang/releases/tag/v1.0.3) (161 MB)
2. Extract to any folder
3. (Optional) Run `setup_edge_tts.bat` for premium voice quality
4. Launch `ohao-lang.exe` - Done!

**Everything included:** Tesseract OCR + all dependencies bundled in the package!

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

### Create Release Packages (Windows)

```powershell
# Build the application first
./build.sh

# Create Standard release (without Edge-TTS)
./create-release.ps1

# Create Full release (with Edge-TTS bundled)
./create-release.ps1 -IncludeEdgeTTS
```

**Note:** For Full release, you need Python and PyInstaller:
```bash
pip install pyinstaller edge-tts
```

The script will automatically:
- Build `edge-tts.exe` if needed (uses `edge_tts_wrapper.py`)
- Skip rebuild if `edge-tts.exe` is up-to-date
- Create ZIP package ready for distribution

## 📋 Requirements

**macOS:** 12.0+, 50MB disk space
**Windows:** 10/11, 161MB (all-in-one package, no additional setup)

### Optional: Edge-TTS Setup (Windows)

For best voice quality, run the included `setup_edge_tts.bat` which automatically:
- Downloads Python 3.11 embeddable (if needed)
- Installs Edge-TTS
- Creates launcher scripts

Or use the built-in System TTS - works immediately, no setup required!

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

## 🙏 Acknowledgements

- **[Hunspell](https://hunspell.github.io/)** - Spell checker and morphological analyzer
- **[wooorm/dictionaries](https://github.com/wooorm/dictionaries)** - Hunspell dictionaries for 15 languages
- **[Tesseract OCR](https://github.com/tesseract-ocr/tesseract)** - OCR engine
- **[copilot-api](https://github.com/ericc-ch/copilot-api)** - GitHub Copilot API proxy for AI assistant features
- **Qt Framework** - Cross-platform application framework

## 📄 License

MIT License - see [LICENSE](LICENSE)

## 🔗 Links

- [Issues](https://github.com/Qervas/ohao-lang/issues)
- [Releases](https://github.com/Qervas/ohao-lang/releases)
