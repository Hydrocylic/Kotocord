#include <QtTest>
#include <QApplication>

#include "core/graph/GraphModel.h"
#include "core/graph/GraphCompiler.h"
#include "core/graph/Pipeline.h"
#include "core/graph/NodeCatalog.h"
#include "core/AppController.h"
#include "modules/source/VoiceInputNode.h"
#include "modules/source/ReminderScheduler.h"
#include "modules/orchestrator/OrchestratorNode.h"
#include "modules/render/SubtitleRenderer.h"
#include "modules/tts/PythonEdgeTTS.h"
#include "modules/tts/TTSPlayer.h"

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>

#ifndef KOTO_PIPELINES_DIR
#define KOTO_PIPELINES_DIR "../../resources/pipelines"
#endif

// M4a: 真实节点目录 + default.json 装配验证
// (音频/TTS 后端不真跑; 验证图编译、实例类型、连接数、编排器数据流)
class TestPipelineCatalog : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(QFile::exists(QStringLiteral(KOTO_PIPELINES_DIR) + "/default.json"));
    }

    // 1. 目录类型齐全
    void testCatalogTypes() {
        NodeRegistry reg = NodeCatalog::build();
        for(const QString& t : {"voice_input", "reminder", "orchestrator",
                                "subtitle_render", "tts", "audio_out"}) {
            QVERIFY2(reg.find(t), qPrintable("缺少节点类型: " + t));
        }
    }

    // 2. default.json 编译: 6 节点 5 连接, 实例类型正确
    void testDefaultPipelineCompiles() {
        QFile f(QStringLiteral(KOTO_PIPELINES_DIR) + "/default.json");
        QVERIFY(f.open(QIODevice::ReadOnly));
        GraphModel graph = GraphModel::fromJson(QJsonDocument::fromJson(f.readAll()).object());

        Pipeline pipeline;
        CompileResult r = GraphCompiler::compile(graph, NodeCatalog::build(), pipeline);
        if(!r.ok) qDebug() << r.errors;
        QVERIFY(r.ok);
        QCOMPARE(pipeline.moduleCount(), 6);
        QCOMPARE(pipeline.connectionCount(), 5);

        QVERIFY(qobject_cast<VoiceInputNode*>(r.instances.value(0)));
        QVERIFY(qobject_cast<OrchestratorNode*>(r.instances.value(1)));
        QVERIFY(qobject_cast<SubtitleRenderer*>(r.instances.value(2)));
        QVERIFY(qobject_cast<PythonEdgeTTS*>(r.instances.value(3)));
        QVERIFY(qobject_cast<TTSPlayer*>(r.instances.value(4)));
        QVERIFY(qobject_cast<ReminderScheduler*>(r.instances.value(5)));
    }

    // 3. 编排器数据流: 手动输入 → subtitleReadyForRender + ttsReadyForSpeech (只对处理完的帧)
    void testOrchestratorDataflow() {
        OrchestratorNode orch("mock");
        QSignalSpy renderSpy(orch.controller(), &AppController::subtitleReadyForRender);
        QSignalSpy ttsSpy(orch.controller(), &AppController::ttsReadyForSpeech);

        orch.controller()->onManualTextEntered("测试句子");
        QTest::qWait(1500); // MockLLM 用 QTimer 模拟 1000ms 网络延迟

        // 首帧 (未处理, 上屏) + LLM 处理帧: 渲染两次, 播报一次 (M4a 等价性关键断言)
        QVERIFY(renderSpy.count() >= 2);
        QCOMPARE(ttsSpy.count(), 1);
    }

    // 4. 引擎切换 API (运行状态机, 不真启动音频)
    void testEngineSwitchAPI() {
        OrchestratorNode orch("mock");
        orch.setLlmEngine(true);   // → deepseek
        orch.setApiKey("sk-test"); // 不触发网络
        orch.setLlmEngine(false);  // → mock

        VoiceInputNode voice;
        QCOMPARE(voice.isWhisperEngine(), true); // 默认与旧 main 一致
        voice.setEngine(false);                 // 仅状态切换, 未运行不启停
        QCOMPARE(voice.isWhisperEngine(), false);
    }
};

QTEST_MAIN(TestPipelineCatalog)
#include "TestPipelineCatalog.moc"
