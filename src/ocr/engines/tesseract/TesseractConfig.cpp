#include "TesseractConfig.h"

namespace TesseractConfig {

QString getLanguageCode(const QString& displayName)
{
    // Use multi-language mode for Latin-based languages with diacritics
    // This fixes åäö, éèê, üö recognition issues by combining with English
    return LanguageManager::instance().getMultiLanguageTesseractCode(displayName);
}

// Character whitelist removed; plain text path relies on full language model

int getPSMForQualityLevel(int qualityLevel)
{
    switch (qualityLevel) {
    case 1: return 8;  // Single word
    case 2: return 7;  // Single text line
    case 3: return 6;  // Uniform block of text
    case 4: return 3;  // Fully automatic page segmentation
    case 5: return 1;  // Automatic with OSD
    default: return 6;
    }
}

bool shouldUseLSTM(const QString& language, int qualityLevel, bool autoDetectOrientation)
{
    // Always use LSTM (OEM 1) for now
    // Our traineddata files (from tessdata_fast/best) only support LSTM, not Legacy
    // Legacy mode (OEM 0) requires traineddata from the main tessdata repository
    return (language != "English" && language != "Auto-Detect") ||
           (autoDetectOrientation && qualityLevel >= 4);
}

} // namespace TesseractConfig
