#include "VoiceInputNode.h"
#include "../input/VoskTranscriber.h"
#include "../input/WhisperTranscriber.h"

VoiceInputNode::VoiceInputNode(QObject* parent)
    : QObject(parent)
    , m_vosk(std::make_unique<VoskTranscriber>())
    , m_whisper(std::make_unique<WhisperTranscriber>()) {

    // 内部固定连线: 麦克风 → 两个引擎 (与旧 main.cpp 相同, 切换只是启停选择)
    connect(&m_capture, &AudioCapture::audioDataReady,
            m_vosk.get(), &VoskTranscriber::onAudioDataReady);
    connect(&m_capture, &AudioCapture::audioDataReady,
            m_whisper.get(), &WhisperTranscriber::onAudioDataReady);

    // 引擎结果 → 本节点输出 (信号中继)
    connect(m_vosk.get(), &IAudioTranscriber::textReady,
            this, &VoiceInputNode::textReady);
    connect(m_whisper.get(), &IAudioTranscriber::textReady,
            this, &VoiceInputNode::textReady);
}

VoiceInputNode::~VoiceInputNode() = default;

IAudioTranscriber* VoiceInputNode::currentEngine() {
    return m_isWhisper ? static_cast<IAudioTranscriber*>(m_whisper.get())
                       : static_cast<IAudioTranscriber*>(m_vosk.get());
}

void VoiceInputNode::stopEngines() {
    m_capture.stop();
    m_vosk->stop();
    m_whisper->stop();
}

void VoiceInputNode::setRunning(bool on) {
    if(m_running == on) return;
    m_running = on;
    if(on) {
        if(currentEngine()->start()) m_capture.start();
    } else {
        stopEngines();
    }
}

void VoiceInputNode::setEngine(bool isWhisper) {
    if(m_isWhisper == isWhisper) return;
    m_isWhisper = isWhisper;
    if(m_running) {
        stopEngines();
        if(currentEngine()->start()) m_capture.start();
    }
}
