#include "OrchestratorNode.h"
#include "../../core/AppController.h"
#include "../../utils/AppPaths.h"
#include "../llm/ILanguageModel.h"
#include "../llm/MockLLMWorker.h"
#include "../llm/DeepSeekAPIWorker.h"
#include "../llm/KaomojiManager.h"

OrchestratorNode::OrchestratorNode(const QString& llmEngine, QObject* parent)
    : QObject(parent)
    , m_controller(std::make_unique<AppController>())
    , m_kaomoji(std::make_unique<KaomojiManager>())
    , m_mockLLM(std::make_unique<MockLLMWorker>())
    , m_deepSeek(std::make_unique<DeepSeekAPIWorker>()) {

    m_kaomoji->loadFromFile(AppPaths::getKaomojiPath());
    m_controller->setKaomojiManager(m_kaomoji.get());
    m_controller->setLanguageModel(llmEngine == QStringLiteral("deepseek")
                                       ? static_cast<ILanguageModel*>(m_deepSeek.get())
                                       : static_cast<ILanguageModel*>(m_mockLLM.get()));

    // LLM → UI 的中继 (旧 main.cpp 直连两个 LLM 的 textProcessed, 现统一走节点信号)
    connect(m_mockLLM.get(), &ILanguageModel::textProcessed,
            this, &OrchestratorNode::frameProcessed);
    connect(m_deepSeek.get(), &ILanguageModel::textProcessed,
            this, &OrchestratorNode::frameProcessed);
    connect(m_deepSeek.get(), &DeepSeekAPIWorker::performanceMetricsReported,
            this, &OrchestratorNode::metricsReported);
}

OrchestratorNode::~OrchestratorNode() = default;

void OrchestratorNode::setLlmEngine(bool isDeepSeek) {
    m_controller->setLanguageModel(isDeepSeek
        ? static_cast<ILanguageModel*>(m_deepSeek.get())
        : static_cast<ILanguageModel*>(m_mockLLM.get()));
}

void OrchestratorNode::setApiKey(const QString& key) {
    m_deepSeek->setApiConfig(key);
}
