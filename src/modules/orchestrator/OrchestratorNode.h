#ifndef ORCHESTRATORNODE_H
#define ORCHESTRATORNODE_H

#include <QObject>
#include <memory>

#include "../../core/DataTypes.h"

class AppController;
class KaomojiManager;
class ILanguageModel;
class MockLLMWorker;
class DeepSeekAPIWorker;

// M4a: 编排器复合节点 (处理) — AppController + KaomojiManager + 双 LLM 引擎
// 队列/锁控/帧ID 状态机是 AppController 内部逻辑, 原样保留
// 保守处理: LLM/情绪分析暂为复合内部件, 拆分为独立节点留待后续 (plan §2.3 细化)
// 入口: ASR 文本 (onASRTextReady) / 手动文本 (onManualTextEntered, UI 直连)
// 出口: subtitleReadyForRender (帧 → 渲染汇) / frame.displayText (→ TTS)
class OrchestratorNode : public QObject {
    Q_OBJECT
public:
    // llmEngine: "mock" | "deepseek" (与旧 main.cpp 默认 mock 一致)
    explicit OrchestratorNode(const QString& llmEngine = QStringLiteral("mock"),
                              QObject* parent = nullptr);
    ~OrchestratorNode() override; // unique_ptr 成员的前置类型, 析构定义在 cpp

    AppController* controller() const { return m_controller.get(); }

    // UI 侧运行控制
    void setLlmEngine(bool isDeepSeek);
    void setApiKey(const QString& key);

signals:
    // LLM 处理帧中继 (→ UI 情绪标签; 与旧 main.cpp 的 LLM 直连等价)
    void frameProcessed(const SubtitleFrame& frame);
    // DeepSeek 性能指标中继 (→ UI 延迟/Token 看板)
    void metricsReported(int prompt, int completion, qint64 latency);

private:
    std::unique_ptr<AppController> m_controller;
    std::unique_ptr<KaomojiManager> m_kaomoji;
    std::unique_ptr<MockLLMWorker> m_mockLLM;
    std::unique_ptr<DeepSeekAPIWorker> m_deepSeek;
};

#endif // ORCHESTRATORNODE_H
