#ifndef VOICEINPUTNODE_H
#define VOICEINPUTNODE_H

#include <QObject>
#include <memory>

#include "../capture/AudioCapture.h"
#include "../input/IAudioTranscriber.h"

class VoskTranscriber;
class WhisperTranscriber;

// M4a: 语音输入复合节点 (源) — AudioCapture + 双 ASR 引擎
// 保守处理 (plan §2.3): 内部链路 (mic→VAD→ASR) 不暴露为子节点, 展开留线头 T2
// 模型加载在 start() 延迟 (构造轻量, 图可先装配后启动)
class VoiceInputNode : public QObject {
    Q_OBJECT
public:
    explicit VoiceInputNode(QObject* parent = nullptr);
    ~VoiceInputNode() override; // unique_ptr 成员的前置类型, 析构定义在 cpp

    // 运行控制 (UI 侧直接驱动; 图是配置面, 不参与运行时 — 决策 D-003)
    void setRunning(bool on);
    bool isRunning() const { return m_running; }

    // 引擎切换 (运行中切换: 停旧起新)
    void setEngine(bool isWhisper);
    bool isWhisperEngine() const { return m_isWhisper; }

signals:
    void textReady(const QString& text, bool isFinal);// ASR 识别结果 (中继)

private:
    AudioCapture m_capture;
    std::unique_ptr<VoskTranscriber> m_vosk;
    std::unique_ptr<WhisperTranscriber> m_whisper;
    bool m_isWhisper = true; // 与旧 main.cpp 默认一致
    bool m_running = false;

    IAudioTranscriber* currentEngine();
    void stopEngines();
};

#endif // VOICEINPUTNODE_H
