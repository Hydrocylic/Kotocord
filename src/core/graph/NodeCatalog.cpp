#include "NodeCatalog.h"
#include "../DataTypes.h"

#include "../../modules/source/VoiceInputNode.h"
#include "../../modules/source/ReminderScheduler.h"
#include "../../modules/orchestrator/OrchestratorNode.h"
#include "../../modules/render/SubtitleRenderer.h"
#include "../../modules/tts/PythonEdgeTTS.h"
#include "../../modules/tts/TTSPlayer.h"
#include "../../core/AppController.h"

NodeRegistry NodeCatalog::build() {
    NodeRegistry registry;

    // ---- 源: 语音输入 (复合: mic + vosk/whisper) ----
    {
        NodeTypeDesc d;
        d.type = "voice_input"; d.displayName = "语音输入";
        d.outPorts = {{0, "text", "asr_text"}};
        d.create = [](const QVariantMap&) { return new VoiceInputNode(); };
        d.connectors = {
            {0, "orchestrator", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<VoiceInputNode*>(s), &VoiceInputNode::textReady,
                                        static_cast<OrchestratorNode*>(t)->controller(),
                                        &AppController::onASRTextReady); }},
        };
        registry.registerType(std::move(d));
    }

    // ---- 源: 定时提醒 ----
    {
        NodeTypeDesc d;
        d.type = "reminder"; d.displayName = "定时提醒";
        d.outPorts = {{0, "message", "text"}};
        d.create = [](const QVariantMap&) { return new ReminderScheduler(); };
        d.connectors = {
            {0, "tts", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<ReminderScheduler*>(s), &ReminderScheduler::reminderReady,
                                        static_cast<PythonEdgeTTS*>(t), &PythonEdgeTTS::synthesize); }},
            {0, "orchestrator", 1, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<ReminderScheduler*>(s), &ReminderScheduler::reminderReady,
                                        static_cast<OrchestratorNode*>(t)->controller(),
                                        &AppController::onManualTextEntered); }},
        };
        registry.registerType(std::move(d));
    }

    // ---- 处理: 编排器 (复合: AppController + LLM + Kaomoji) ----
    {
        NodeTypeDesc d;
        d.type = "orchestrator"; d.displayName = "字幕编排 (LLM)";
        d.inPorts = {{0, "text", "asr_text"}, {1, "manual", "text"}};
        d.outPorts = {{0, "frame", "subtitle_frame"}, {1, "say", "text"}};
        d.create = [](const QVariantMap& p) {
            return new OrchestratorNode(p.value("llmEngine", "mock").toString());
        };
        d.connectors = {
            // 帧 → 字幕渲染
            {0, "subtitle_render", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<OrchestratorNode*>(s)->controller(),
                                        &AppController::subtitleReadyForRender,
                                        static_cast<SubtitleRenderer*>(t), &SubtitleRenderer::updateFrame); }},
            // 待播报文本 → TTS (专用信号: 仅 LLM 处理完的帧, 不含 ASR 中间帧 — 与旧 m_tts 调用点等价)
            {1, "tts", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<OrchestratorNode*>(s)->controller(),
                                        &AppController::ttsReadyForSpeech,
                                        static_cast<PythonEdgeTTS*>(t), &PythonEdgeTTS::synthesize); }},
        };
        registry.registerType(std::move(d));
    }

    // ---- 汇: 字幕渲染 ----
    {
        NodeTypeDesc d;
        d.type = "subtitle_render"; d.displayName = "字幕渲染";
        d.inPorts = {{0, "frame", "subtitle_frame"}};
        d.create = [](const QVariantMap&) { return new SubtitleRenderer(); };
        registry.registerType(std::move(d));
    }

    // ---- 处理: TTS 合成 (Python 侧车) ----
    {
        NodeTypeDesc d;
        d.type = "tts"; d.displayName = "TTS 合成";
        d.inPorts = {{0, "text", "text"}};
        d.outPorts = {{0, "audio", "audio"}};
        d.create = [](const QVariantMap&) { return new PythonEdgeTTS(); };
        d.connectors = {
            {0, "audio_out", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<PythonEdgeTTS*>(s), &PythonEdgeTTS::audioReady,
                                        static_cast<TTSPlayer*>(t), &TTSPlayer::play); }},
        };
        registry.registerType(std::move(d));
    }

    // ---- 汇: 音频输出 ----
    {
        NodeTypeDesc d;
        d.type = "audio_out"; d.displayName = "音频输出";
        d.inPorts = {{0, "audio", "audio"}};
        d.create = [](const QVariantMap&) { return new TTSPlayer(); };
        registry.registerType(std::move(d));
    }

    return registry;
}
