#pragma once

#include <QString>
#include "../../ui/core/LanguageManager.h"

namespace TesseractConfig {
    QString getLanguageCode(const QString& displayName);
    int getPSMForQualityLevel(int qualityLevel);
    bool shouldUseLSTM(const QString& language, int qualityLevel, bool autoDetectOrientation);
}
