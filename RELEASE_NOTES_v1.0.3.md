# Ohao Lang v1.0.3 Release Notes

## 🎉 Complete Standalone Release

This release includes **everything you need** in one package - no additional installations required (except for optional Edge-TTS Python support).

## 📦 What's Included

- ✅ **Ohao Lang Application** (Release build, no console window)
- ✅ **Tesseract OCR 5.5.0** with 10+ language packs
- ✅ **All Qt 6.9.2 DLLs and dependencies**
- ✅ **Edge-TTS Setup Script** (automatic Python + edge-tts installation)
- ✅ **Complete Documentation** (README, CHANGELOG, LICENSE)

## 🚀 Key Features

### Modern TTS System
- **Edge TTS**: High-quality Microsoft voices (free, requires Python)
- **System TTS**: Windows built-in voices (works offline, no setup)
- **Google Web TTS**: Free online alternative

### Enhanced User Experience
- **Language-Specific Test Sentences**: Test voices with proper sentences for 38 languages
- **Unified Theme System**: Light, Dark, and Auto (System) themes
- **Voice Persistence**: Your selected voice is saved and restored
- **Translation Overlay**: Integrates perfectly with your theme

## 🐛 Bug Fixes

- ✅ Fixed theme dropdown showing wrong value on startup
- ✅ Fixed translation overlay being too bright in dark theme
- ✅ Fixed screenshot TTS being silent or using wrong provider
- ✅ Fixed Edge TTS "executable not found" error
- ✅ Fixed System TTS speaking at supersonic speed
- ✅ Fixed voice selection not persisting across app restarts

## 📥 Installation

1. Download `OhaoLang-v1.0.3-Windows-x64.zip`
2. Extract to any folder
3. (Optional) Run `setup_edge_tts.bat` for best voice quality
4. Run `ohao-lang.exe`

## 💡 First-Time Setup

### For Best Voice Quality (Edge-TTS)
Simply run `setup_edge_tts.bat` - it will:
- Download Python 3.11 embeddable (if not installed)
- Install Edge-TTS automatically
- Create launcher scripts

### Use Without Setup
The app works immediately with Windows System TTS - no setup required!

## 📊 Package Size

- **Total**: 161 MB (includes Tesseract language packs)
- **Tesseract Data**: ~210 MB (10+ languages)
- **Application + Qt**: ~70 MB

## 🔧 System Requirements

- Windows 10/11 (64-bit)
- 4GB RAM minimum
- Internet connection (for Edge-TTS and Google Web TTS)

## 🙏 Credits

- OCR: [Tesseract 5.5.0](https://github.com/tesseract-ocr/tesseract)
- TTS: [Edge-TTS](https://github.com/rany2/edge-tts)
- Framework: Qt 6.9.2
- Translation: Google Translate API

## 📝 Full Changelog

See [CHANGELOG.md](CHANGELOG.md) for complete version history.

## 🐛 Report Issues

https://github.com/Qervas/ohao-lang/issues

---

**Enjoy seamless OCR & Translation!** 🎯
