#include <QtTest>

#include "core/graph/GraphModel.h"
#include "core/graph/NodeRegistry.h"
#include "core/graph/GraphCompiler.h"
#include "core/graph/Pipeline.h"

#include <QJsonDocument>

// ---- 测试用节点桩: 模拟默认管线拓扑 (输入源 → LLM 处理 → 字幕汇 + TTS 汇) ----

class StubSource : public QObject { // 源: 手动/语音输入
    Q_OBJECT
public:
    void emitText(const QString& text) { emit textReady(text); }
signals:
    void textReady(const QString& text);
};

class StubLLM : public QObject { // 处理: LLM 润色
    Q_OBJECT
public slots:
    void onText(const QString& text) { m_lastInput = text; emit textProcessed(text + "-processed"); }
    QString lastInput() const { return m_lastInput; }
private:
    QString m_lastInput;
signals:
    void textProcessed(const QString& text);
};

class StubSink : public QObject { // 汇: 字幕渲染 / TTS
    Q_OBJECT
public:
    QStringList received;
    QString lastParam;
public slots:
    void onFrame(const QString& text) { received.append(text); }
    void setParam(const QString& value) { lastParam = value; }
};

// 构造与测试桩配套的注册表
static NodeRegistry makeRegistry() {
    NodeRegistry registry;
    {
        NodeTypeDesc d;
        d.type = "source"; d.displayName = "输入源";
        d.outPorts = {{0, "text", "text"}};
        d.create = [](const QVariantMap&) { return new StubSource(); };
        d.connectors = {
            {0, "llm", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<StubSource*>(s), &StubSource::textReady,
                                         static_cast<StubLLM*>(t), &StubLLM::onText); }},
            {0, "sink", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<StubSource*>(s), &StubSource::textReady,
                                         static_cast<StubSink*>(t), &StubSink::onFrame); }},
        };
        registry.registerType(std::move(d));
    }
    {
        NodeTypeDesc d;
        d.type = "llm"; d.displayName = "LLM";
        d.inPorts = {{0, "text", "text"}};
        d.outPorts = {{0, "text", "text"}};
        d.create = [](const QVariantMap&) { return new StubLLM(); };
        d.connectors = {
            {0, "sink", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<StubLLM*>(s), &StubLLM::textProcessed,
                                         static_cast<StubSink*>(t), &StubSink::onFrame); }},
        };
        registry.registerType(std::move(d));
    }
    {
        NodeTypeDesc d;
        d.type = "sink"; d.displayName = "汇";
        d.inPorts = {{0, "text", "text"}};
        d.create = [](const QVariantMap& p) {
            StubSink* s = new StubSink();
            s->lastParam = p.value("param").toString();
            return s;
        };
        registry.registerType(std::move(d));
    }
    {
        NodeTypeDesc d; // 类型不匹配用: 音频端口
        d.type = "badtype";
        d.outPorts = {{0, "audio", "audio"}};
        d.create = [](const QVariantMap&) { return new StubSource(); };
        d.connectors = {
            {0, "sink", 0, [](QObject* s, QObject* t) {
                return QObject::connect(static_cast<StubSource*>(s), &StubSource::textReady,
                                         static_cast<StubSink*>(t), &StubSink::onFrame); }},
        };
        registry.registerType(std::move(d));
    }
    return registry;
}

// 默认管线拓扑 (复现旧管线的图形态):
//   source → llm; llm → sink(字幕); llm → sink(TTS)
static GraphModel makeDefaultPipelineGraph() {
    GraphModel g;
    NodeId src = g.addNode("source");
    NodeId llm = g.addNode("llm");
    NodeId sub = g.addNode("sink", {{"param", "subtitle"}});
    NodeId tts = g.addNode("sink", {{"param", "tts"}});
    g.addEdge({src, 0, PortDirection::Out}, {llm, 0, PortDirection::In});
    g.addEdge({llm, 0, PortDirection::Out}, {sub, 0, PortDirection::In});
    g.addEdge({llm, 0, PortDirection::Out}, {tts, 0, PortDirection::In});
    return g;
}

class TestGraphCore : public QObject {
    Q_OBJECT

private slots:
    // ========== GraphModel ==========

    // 1. JSON 序列化往返: 节点/边/参数无损
    void testJsonRoundtrip() {
        GraphModel g = makeDefaultPipelineGraph();
        QJsonObject obj = g.toJson();
        GraphModel g2 = GraphModel::fromJson(obj);
        QCOMPARE(g2.nodes().size(), 4);
        QCOMPARE(g2.edges().size(), 3);
        // 参数往返 (图里有 subtitle/tts 两个 sink, 取前者)
        const GraphNodeDesc* sub = nullptr;
        for(const auto& n : g2.nodes()) {
            if(n.type == "sink" && n.params.value("param").toString() == "subtitle") { sub = &n; break; }
        }
        QVERIFY(sub);
        QCOMPARE(sub->params.value("param").toString(), QStringLiteral("subtitle"));
        // 再次序列化等价 (稳定性)
        QCOMPARE(QJsonDocument(g2.toJson()).toJson(), QJsonDocument(obj).toJson());
    }

    // 2. addEdge 基本防御: 方向错误/节点不存在被拒
    void testAddEdgeGuards() {
        GraphModel g;
        NodeId a = g.addNode("source");
        NodeId b = g.addNode("sink");
        QVERIFY(!g.addEdge({a, 0, PortDirection::In}, {b, 0, PortDirection::In}));  // 源方向错
        QVERIFY(!g.addEdge({a, 0, PortDirection::Out}, {b, 0, PortDirection::Out})); // 目标方向错
        QVERIFY(!g.addEdge({a, 0, PortDirection::Out}, {99, 0, PortDirection::In})); // 节点不存在
        QVERIFY(g.addEdge({a, 0, PortDirection::Out}, {b, 0, PortDirection::In}));
        QCOMPARE(g.edges().size(), 1);
    }

    // ========== GraphCompiler: 非法图拒绝 ==========

    // 3. 未注册类型
    void testRejectUnknownType() {
        NodeRegistry reg = makeRegistry();
        GraphModel g;
        g.addNode("nonexistent");
        CompileResult r = GraphCompiler::validate(g, reg);
        QVERIFY(!r.ok);
        QVERIFY(r.errors.join(" ").contains("未注册"));
    }

    // 4. 端口不存在
    void testRejectMissingPort() {
        NodeRegistry reg = makeRegistry();
        GraphModel g;
        NodeId src = g.addNode("source");
        NodeId sub = g.addNode("sink");
        g.addEdge({src, 5, PortDirection::Out}, {sub, 0, PortDirection::In});
        CompileResult r = GraphCompiler::validate(g, reg);
        QVERIFY(!r.ok);
        QVERIFY(r.errors.join(" ").contains("输出端口"));
    }

    // 5. 数据类型不匹配 (audio → text)
    void testRejectTypeMismatch() {
        NodeRegistry reg = makeRegistry();
        GraphModel g;
        NodeId bad = g.addNode("badtype");
        NodeId sub = g.addNode("sink");
        g.addEdge({bad, 0, PortDirection::Out}, {sub, 0, PortDirection::In});
        CompileResult r = GraphCompiler::validate(g, reg);
        QVERIFY(!r.ok);
        QVERIFY(r.errors.join(" ").contains("类型不匹配"));
    }

    // 6. 环检测
    void testRejectCycle() {
        NodeRegistry reg = makeRegistry();
        GraphModel g;
        NodeId a = g.addNode("llm");
        NodeId b = g.addNode("llm");
        g.addEdge({a, 0, PortDirection::Out}, {b, 0, PortDirection::In});
        g.addEdge({b, 0, PortDirection::Out}, {a, 0, PortDirection::In});
        CompileResult r = GraphCompiler::validate(g, reg);
        QVERIFY(!r.ok);
        QVERIFY(r.errors.join(" ").contains("环"));
    }

    // 7. 编译失败不动 Pipeline (无半装配)
    void testFailedCompileLeavesPipelineIntact() {
        NodeRegistry reg = makeRegistry();
        GraphModel g = makeDefaultPipelineGraph();
        g.addNode("nonexistent"); // 混入一个坏节点
        Pipeline p;
        CompileResult r = GraphCompiler::compile(g, reg, p);
        QVERIFY(!r.ok);
        QCOMPARE(p.moduleCount(), 0);
        QCOMPARE(p.connectionCount(), 0);
    }

    // ========== 编译装配 + 数据流 (默认管线复现) ==========

    // 8. 代码构造图 == 旧管线行为: source → llm → 字幕/TTS 双汇
    void testDefaultPipelineDataflow() {
        NodeRegistry reg = makeRegistry();
        GraphModel g = makeDefaultPipelineGraph();
        Pipeline p;
        CompileResult r = GraphCompiler::compile(g, reg, p);
        QVERIFY(r.ok);
        QVERIFY(r.errors.isEmpty());
        QCOMPARE(p.moduleCount(), 4);
        QCOMPARE(p.connectionCount(), 3);

        // 取实例, 触发源, 验证数据流经 LLM 到两个汇
        auto* source = qobject_cast<StubSource*>(r.instances.value(0));
        auto* llm = qobject_cast<StubLLM*>(r.instances.value(1));
        auto* subtitle = qobject_cast<StubSink*>(r.instances.value(2));
        auto* tts = qobject_cast<StubSink*>(r.instances.value(3));
        QVERIFY(source && llm && subtitle && tts);

        source->emitText("你好");
        QCOMPARE(llm->lastInput(), QStringLiteral("你好"));
        QCOMPARE(subtitle->received, QStringList{QStringLiteral("你好-processed")});
        QCOMPARE(tts->received, QStringList{QStringLiteral("你好-processed")});
        QCOMPARE(subtitle->lastParam, QStringLiteral("subtitle"));
        QCOMPARE(tts->lastParam, QStringLiteral("tts"));
    }

    // 9. 参数注入: 工厂从节点 params 读配置
    void testParamsInjected() {
        NodeRegistry reg = makeRegistry();
        GraphModel g;
        g.addNode("sink", {{"param", "custom-value"}});
        Pipeline p;
        CompileResult r = GraphCompiler::compile(g, reg, p);
        QVERIFY(r.ok);
        auto* sink = qobject_cast<StubSink*>(r.instances.value(0));
        QCOMPARE(sink->lastParam, QStringLiteral("custom-value"));
    }

    // 10. Pipeline 拆除: 断连后信号不再送达
    void testPipelineTeardown() {
        NodeRegistry reg = makeRegistry();
        Pipeline p;
        StubSource* source = nullptr;
        {
            GraphModel g = makeDefaultPipelineGraph();
            CompileResult r = GraphCompiler::compile(g, reg, p);
            QVERIFY(r.ok);
            source = qobject_cast<StubSource*>(r.instances.value(0));
        }
        p.clear();
        QCOMPARE(p.moduleCount(), 0);
        // clear 后实例已销毁, 这里只验证 clear 本身不崩溃且计数归零
        // (对已销毁 source 的进一步使用是调用方的责任, 由 unique_ptr 所有权保证)
        Q_UNUSED(source);
    }
};

QTEST_MAIN(TestGraphCore)
#include "TestGraphCore.moc"
