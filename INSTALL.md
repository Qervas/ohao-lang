# 🚀 Quick Installation Guide

## Step 1: Download the Application
1. Download: [`ohao-lang-portable-v1.0.1.zip`](https://github.com/Qervas/ohao-lang/releases)
2. Extract to any folder (Desktop, USB drive, etc.)

## Step 2: Install OCR Engine (Required)
**Choose ONE method:**

### Option A: Scoop (Easiest)
```powershell
# 1. Install Scoop (package manager)
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex

# 2. Install Tesseract OCR
scoop install tesseract

# 3. Install language packs (optional)
scoop install tesseract-languages
```

### Option B: Direct Download
1. Go to: https://github.com/UB-Mannheim/tesseract/wiki
2. Download: `tesseract-ocr-w64-setup-5.3.x.exe`
3. Install with default settings
4. Make sure "Add to PATH" is checked

### Option C: Chocolatey
```powershell
choco install tesseract
```

## Step 3: Run the Application
1. Double-click `ohao-lang.exe` OR `Start-OhaoLang.bat`
2. Look for the system tray icon 🔍
3. Test with **Ctrl+Shift+S** to take a screenshot

## Step 4: First Use
1. Press **Ctrl+Shift+S**
2. Draw a rectangle around some text
3. Wait for OCR to process
4. Enjoy automatic translation and TTS!

## ✅ Verification
To check if OCR is working:
```powershell
# Open PowerShell and type:
tesseract --version
```
Should show: `tesseract 5.3.x`

## 🆘 Need Help?
- **OCR not working?** Make sure Tesseract is installed
- **No system tray icon?** Check Windows notification area settings  
- **Global shortcuts not working?** Try running as administrator once

**Full documentation:** See README.md for complete setup guide