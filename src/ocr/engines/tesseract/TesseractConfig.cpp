#include "TesseractConfig.h"

namespace TesseractConfig {

QString getLanguageCode(const QString& displayName)
{
    return LanguageManager::instance().getTesseractCode(displayName);
}

QString getCharacterWhitelist(const QString& displayName)
{
    return LanguageManager::instance().getCharacterWhitelist(displayName);
}

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
    return (language != "English" && language != "Auto-Detect") ||
           (autoDetectOrientation && qualityLevel >= 4);
}

} // namespace TesseractConfig
