#pragma once

/**
 * Common TTS definitions and forward declarations
 * This header ensures QTextToSpeech is properly declared once
 * and handles platforms where Qt TextToSpeech is not available
 */

#ifndef QT_TEXTTOSPEECH_AVAILABLE
// Forward declaration for when Qt TextToSpeech is not available
class QTextToSpeech {
public:
    enum State { Ready, Speaking, Paused, Error };
};
#else
// Include the actual Qt TextToSpeech header when available
#include <QTextToSpeech>
#endif
