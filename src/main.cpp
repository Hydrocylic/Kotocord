#include <QApplication>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QDebug>
#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>

#include "ui/MainWindow.h"
#include "ui/NodeEditorView.h"
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

static GraphModel loadGraphFile(const QString& path) {
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return GraphModel::fromJson(QJsonDocument::fromJson(f.readAll()).object());
}

// M4b: 装配从硬编码连线改为 图(默认 default.json) → Compiler → Pipeline
// 节点编辑器 (QDockWidget) 可加载模板/编辑图, Apply 触发停机重建 (保守: 不做热插拔, 线头 T1)
// 旁路 (非数据流, 不进图): SystemResourceMonitor → UI 监控; MainWindow 内部控件交互
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // ==========================================
    // 第一步：装配管线 (图即配置 — 决策 D-003)
    // ==========================================
    NodeRegistry catalog = NodeCatalog::build();
    GraphModel graph = loadGraphFile(AppPaths::getDefaultPipelinePath());
    if(graph.nodes().isEmpty()) {
        qCritical() << "[Main] 管线模板缺失或为空:" << AppPaths::getDefaultPipelinePath();
        return 1;
    }

    Pipeline pipeline;
    CompileResult compiled = GraphCompiler::compile(graph, catalog, pipeline);
    if(!compiled.ok) {
        qCritical() << "[Main] 管线编译失败:" << compiled.errors;
        return 1;
    }
    qInfo() << "[Main] 管线装配完成:" << pipeline.moduleCount() << "个节点,"
            << pipeline.connectionCount() << "条连接";

    SystemResourceMonitor sysMonitor; // 旁路: 系统监控不进数据流图
    MainWindow window(nullptr);       // 控制器稍后由首次 rewire 注入 (与重建共用一条路径)

    QVector<QMetaObject::Connection> glue; // window ↔ 图实例 的可重接线集合
    auto currentGraph = graph;              // 当前运行图 (编辑器初值)

    // ---- 把 window 挂到当前图实例上 (首次装配与停机重建共用) ----
    auto rewire = [&]() {
        for(const auto& c : glue) QObject::disconnect(c);
        glue.clear();

        // 按类型找实例 (模板节点 id 不固定, 不可按下标取)
        VoiceInputNode* voice = nullptr;
        OrchestratorNode* orch = nullptr;
        SubtitleRenderer* render = nullptr;
        ReminderScheduler* reminder = nullptr;
        for(QObject* obj : compiled.instances) {
            if(!voice) voice = qobject_cast<VoiceInputNode*>(obj);
            if(!orch) orch = qobject_cast<OrchestratorNode*>(obj);
            if(!render) render = qobject_cast<SubtitleRenderer*>(obj);
            if(!reminder) reminder = qobject_cast<ReminderScheduler*>(obj);
        }

        window.reattach(orch ? orch->controller() : nullptr, render);

        if(voice) {
            glue << QObject::connect(&window, &MainWindow::asrToggleRequested,
                                     voice, &VoiceInputNode::setRunning);
            glue << QObject::connect(&window, &MainWindow::asrEngineSwitched,
                                     voice, &VoiceInputNode::setEngine);
        }
        if(orch) {
            glue << QObject::connect(&window, &MainWindow::llmEngineSwitched,
                                     orch, &OrchestratorNode::setLlmEngine);
            glue << QObject::connect(&window, &MainWindow::apiKeyChanged,
                                     orch, &OrchestratorNode::setApiKey);
            glue << QObject::connect(orch, &OrchestratorNode::frameProcessed,
                                     &window, &MainWindow::updateEmotionLabel);
            glue << QObject::connect(orch, &OrchestratorNode::metricsReported,
                                     &window, &MainWindow::updateLatencyAndTokens);
        }
        if(reminder) {
            reminder->loadFromSettings();
            reminder->start();
        }
        // 新图没有的节点类型随 pipeline 换入自动销毁 (Qt 连接自动断开), 无需处理
    };
    rewire();

    // ==========================================
    // 第二步：节点编辑器 (M4b: 模板选择 + 编辑 + 停机重建)
    // ==========================================
    auto* editorDock = new QDockWidget(QStringLiteral("管线编辑器"), &window);
    auto* editorBody = new QWidget(editorDock);
    auto* editorLayout = new QVBoxLayout(editorBody);
    auto* editorBar = new QHBoxLayout();

    auto* editor = new NodeEditorView(catalog, editorBody);
    auto* tplCombo = new QComboBox(editorBody);
    auto* btnLoad = new QPushButton(QStringLiteral("加载模板"), editorBody);
    auto* btnApply = new QPushButton(QStringLiteral("应用到运行时 (停机重建)"), editorBody);
    auto* statusLabel = new QLabel(QStringLiteral("就绪"), editorBody);

    // 模板列表 = resources/pipelines/*.json
    QDir tplDir(AppPaths::getPipelineDir());
    for(const QFileInfo& fi : tplDir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        tplCombo->addItem(fi.completeBaseName(), fi.absoluteFilePath());
    }
    editor->setGraph(currentGraph);

    editorBar->addWidget(tplCombo, 1);
    editorBar->addWidget(btnLoad);
    editorBar->addWidget(btnApply);
    editorLayout->addLayout(editorBar);
    editorLayout->addWidget(editor, 1);
    editorLayout->addWidget(statusLabel);
    editorDock->setWidget(editorBody);
    window.addDockWidget(Qt::RightDockWidgetArea, editorDock);

    QObject::connect(btnLoad, &QPushButton::clicked, [&]() {
        QString path = tplCombo->currentData().toString();
        GraphModel g = loadGraphFile(path);
        if(g.nodes().isEmpty()) {
            statusLabel->setText(QStringLiteral("模板读取失败: %1").arg(path));
            return;
        }
        editor->setGraph(g);
        statusLabel->setText(QStringLiteral("已加载 %1 (尚未应用)").arg(tplCombo->currentText()));
    });

    // 停机重建: 校验 → 新 Pipeline 编译成功 → 换入 → 重挂 UI (失败不动运行时)
    QObject::connect(btnApply, &QPushButton::clicked, [&]() {
        GraphModel edited = editor->graph();
        Pipeline candidate;
        CompileResult r = GraphCompiler::compile(edited, catalog, candidate);
        if(!r.ok) {
            statusLabel->setText(QStringLiteral("编译失败: %1").arg(r.errors.join("; ")));
            return;
        }
        for(const auto& c : glue) QObject::disconnect(c);
        glue.clear();
        pipeline = std::move(candidate); // 旧实例销毁 (Qt 自动断连)
        compiled = std::move(r);
        currentGraph = edited;
        rewire();
        statusLabel->setText(QStringLiteral("已重建: %1 节点 / %2 连接")
                                 .arg(pipeline.moduleCount()).arg(pipeline.connectionCount()));
    });

    // ==========================================
    // 第三步：启动
    // ==========================================
    QObject::connect(&sysMonitor, &SystemResourceMonitor::resourceUpdated,
                     &window, &MainWindow::updateCpuMem);
    sysMonitor.start(1000);
    window.show();

    return app.exec();
}
