#include <QtTest>
#include <QApplication>

#include "ui/GraphQtBridge.h"

#include "core/graph/GraphModel.h"
#include "core/graph/GraphCompiler.h"
#include "core/graph/Pipeline.h"
#include "core/graph/NodeCatalog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>

#ifndef KOTO_PIPELINES_DIR
#define KOTO_PIPELINES_DIR "../../resources/pipelines"
#endif

// M4b: 图核心 ↔ QtNodes 编辑器桥接
// 编辑器模型是"幻影"(委托模型不实例化运行时), 桥接负责双向转换无损
class TestGraphQtBridge : public QObject {
    Q_OBJECT

private slots:
    // 1. 桥接往返: GraphModel → QtNodes 编辑模型 → GraphModel, 节点/边无损
    void testBridgeRoundtrip() {
        GraphModel g;
        g.addNode("voice_input");
        g.addNode("orchestrator", {{"llmEngine", "deepseek"}});
        g.addNode("subtitle_render");
        g.addNode("tts");
        g.addEdge({0, 0, PortDirection::Out}, {1, 0, PortDirection::In});
        g.addEdge({1, 0, PortDirection::Out}, {2, 0, PortDirection::In});
        g.addEdge({1, 1, PortDirection::Out}, {3, 0, PortDirection::In});

        QtNodes::DataFlowGraphModel editor(GraphQtBridge::makeEditorRegistry(NodeCatalog::build()));
        GraphQtBridge::loadInto(editor, g);

        QCOMPARE(editor.allNodeIds().size(), static_cast<size_t>(4));

        GraphModel back = GraphQtBridge::extractFrom(editor);
        QCOMPARE(back.nodes().size(), 4);
        QCOMPARE(back.edges().size(), 3);
        // 类型集合保持 (id 重新分配, 顺序无关比较)
        QSet<QString> expectedTypes;
        expectedTypes << "voice_input" << "orchestrator" << "subtitle_render" << "tts";
        QSet<QString> types;
        for(const auto& n : back.nodes()) types.insert(n.type);
        QCOMPARE(types, expectedTypes);
        // 边的 (类型对, 端口) 保持
        int voiceToOrch = 0, orchToRender = 0, orchToTts = 0;
        for(const auto& e : back.edges()) {
            QString from = back.findNode(e.from.node)->type;
            QString to = back.findNode(e.to.node)->type;
            if(from == "voice_input" && to == "orchestrator" && e.from.port == 0 && e.to.port == 0) ++voiceToOrch;
            if(from == "orchestrator" && to == "subtitle_render" && e.from.port == 0) ++orchToRender;
            if(from == "orchestrator" && to == "tts" && e.from.port == 1 && e.to.port == 0) ++orchToTts;
        }
        QCOMPARE(voiceToOrch, 1);
        QCOMPARE(orchToRender, 1);
        QCOMPARE(orchToTts, 1);
    }

    // 2. 桥接往返后可编译 (端到端: 编辑器导出的图能装配管线)
    void testBridgedGraphCompiles() {
        GraphModel g;
        g.addNode("voice_input");
        g.addNode("orchestrator");
        g.addNode("subtitle_render");
        g.addEdge({0, 0, PortDirection::Out}, {1, 0, PortDirection::In});
        g.addEdge({1, 0, PortDirection::Out}, {2, 0, PortDirection::In});

        QtNodes::DataFlowGraphModel editor(GraphQtBridge::makeEditorRegistry(NodeCatalog::build()));
        GraphQtBridge::loadInto(editor, g);
        GraphModel back = GraphQtBridge::extractFrom(editor);

        Pipeline pipeline;
        CompileResult r = GraphCompiler::compile(back, NodeCatalog::build(), pipeline);
        if(!r.ok) qDebug() << r.errors;
        QVERIFY(r.ok);
        QCOMPARE(pipeline.moduleCount(), 3);
        QCOMPARE(pipeline.connectionCount(), 2);
    }

    // 3. 全部预置模板 (default + t1/t2/t3) 均可编译
    void testAllTemplatesCompile() {
        QDir dir(QStringLiteral(KOTO_PIPELINES_DIR));
        auto files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        QVERIFY(files.size() >= 4);
        for(const QFileInfo& fi : files) {
            QFile f(fi.absoluteFilePath());
            QVERIFY(f.open(QIODevice::ReadOnly));
            GraphModel g = GraphModel::fromJson(QJsonDocument::fromJson(f.readAll()).object());
            QVERIFY2(!g.nodes().isEmpty(), qPrintable(fi.fileName() + " 空图"));
            CompileResult r = GraphCompiler::validate(g, NodeCatalog::build());
            QVERIFY2(r.ok, qPrintable(fi.fileName() + ": " + r.errors.join("; ")));
        }
    }
};

QTEST_MAIN(TestGraphQtBridge)
#include "TestGraphQtBridge.moc"
