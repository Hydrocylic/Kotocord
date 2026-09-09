#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QDebug>

#include "ui/MainWindow.h"
#include "utils/AppPaths.h"
#include "core/graph/GraphModel.h"
#include "core/graph/GraphCompiler.h"
#include "core/graph/Pipeline.h"
#include "core/graph/NodeCatalog.h"     // M4a: 真实节点目录
#include "modules/source/VoiceInputNode.h"
#include "modules/source/ReminderScheduler.h"
#include "modules/orchestrator/OrchestratorNode.h"
#include "modules/render/SubtitleRenderer.h"
#include "modules/system/SystemResourceMonitor.h"

// M4a: 装配从硬编码连线改为 图(default.json) → Compiler → Pipeline
// 旁路 (非数据流, 不进图): SystemResourceMonitor → UI 监控; MainWindow 内部控件交互
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // ==========================================
    // 第一步：装配管线 (图即配置 — 决策 D-003)
    // ==========================================
    NodeRegistry catalog = NodeCatalog::build();

    GraphModel graph;
    QString pipelinePath = AppPaths::getDefaultPipelinePath();
    QFile pipelineFile(pipelinePath);
    if(pipelineFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        graph = GraphModel::fromJson(QJsonDocument::fromJson(pipelineFile.readAll()).object());
        pipelineFile.close();
    } else {
        qWarning() << "[Main] 管线模板缺失, 回退空图:" << pipelinePath;
    }

    Pipeline pipeline;
    CompileResult compiled = GraphCompiler::compile(graph, catalog, pipeline);
    if(!compiled.ok) {
        qCritical() << "[Main] 管线编译失败:" << compiled.errors;
        return 1;
    }
    qInfo() << "[Main] 管线装配完成:" << pipeline.moduleCount() << "个节点,"
            << pipeline.connectionCount() << "条连接";

    // 图实例 (节点 id 对应 default.json)
    auto* voice  = qobject_cast<VoiceInputNode*>(compiled.instances.value(0));
    auto* orch   = qobject_cast<OrchestratorNode*>(compiled.instances.value(1));
    auto* render = qobject_cast<SubtitleRenderer*>(compiled.instances.value(2));
    auto* reminder = qobject_cast<ReminderScheduler*>(compiled.instances.value(5));
    Q_ASSERT(voice && orch && render); // 模板被改坏时快速失败

    // ==========================================
    // 第二步：UI 与运行控制
    // ==========================================
    SystemResourceMonitor sysMonitor; // 旁路: 系统监控不进数据流图
    MainWindow window(orch->controller(), render);

    // --- UI 运行控制 → 图节点 ---
    QObject::connect(&window, &MainWindow::asrToggleRequested,
                     voice, &VoiceInputNode::setRunning);
    QObject::connect(&window, &MainWindow::asrEngineSwitched,
                     voice, &VoiceInputNode::setEngine);
    QObject::connect(&window, &MainWindow::llmEngineSwitched,
                     orch, &OrchestratorNode::setLlmEngine);
    QObject::connect(&window, &MainWindow::apiKeyChanged,
                     orch, &OrchestratorNode::setApiKey);

    // --- 旁路: 监控/看板 → UI ---
    QObject::connect(&sysMonitor, &SystemResourceMonitor::resourceUpdated,
                     &window, &MainWindow::updateCpuMem);
    QObject::connect(orch, &OrchestratorNode::frameProcessed,
                     &window, &MainWindow::updateEmotionLabel);
    QObject::connect(orch, &OrchestratorNode::metricsReported,
                     &window, &MainWindow::updateLatencyAndTokens);

    // --- M3: 定时提醒 (图内 reminder 节点, 事件经图边到 TTS) ---
    if(reminder) {
        reminder->loadFromSettings();
        reminder->start();
    }

    // ==========================================
    // 第三步：启动
    // ==========================================
    sysMonitor.start(1000);
    window.show();

    int rc = app.exec();

    // Pipeline 在栈上, 析构时先断连再逆序销毁 (汇先于源)
    Q_UNUSED(rc);
    return rc;
}
