#pragma once

#include <QObject>
#include "TTSEngine.h"

class TTSManager : public QObject
{
    Q_OBJECT

public:
    static TTSManager& instance();
    
    TTSEngine* ttsEngine();
    
    // TTS convenience methods
    void speakInputText(const QString& text);   // For OCR results
    void speakInputText(const QString& text, const QString& languageCode);   // For OCR results with language
    void speakOutputText(const QString& text);  // For translation results
    void speakOutputText(const QString& text, const QString& languageCode);  // For translation results with language
    
    bool isInputTTSEnabled() const;
    bool isOutputTTSEnabled() const;

private:
    explicit TTSManager(QObject* parent = nullptr);
    ~TTSManager();
    
    TTSManager(const TTSManager&) = delete;
    TTSManager& operator=(const TTSManager&) = delete;
    
    TTSEngine* m_ttsEngine;
    static TTSManager* s_instance;
};