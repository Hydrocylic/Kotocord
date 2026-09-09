#include "GraphQtBridge.h"

#include <QLabel>
#include <QVariant>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeData>
#include <QtNodes/AbstractGraphModel>

namespace {

// 泛型幻影委托模型: 由 NodeTypeDesc 描述端口, 不承载运行时逻辑
class CatalogDelegateModel : public QtNodes::NodeDelegateModel {
public:
    explicit CatalogDelegateModel(NodeTypeDesc desc)
        : m_desc(std::move(desc)) {}

    QString name() const override { return m_desc.type; }
    QString caption() const override { return m_desc.displayName; }

    bool portCaptionVisible(QtNodes::PortType, QtNodes::PortIndex) const override { return true; }
    QString portCaption(QtNodes::PortType t, QtNodes::PortIndex i) const override {
        const NodePortDesc* p = (t == QtNodes::PortType::In) ? m_desc.findIn(i) : m_desc.findOut(i);
        return p ? p->name : QString();
    }

    unsigned int nPorts(QtNodes::PortType t) const override {
        return (t == QtNodes::PortType::In) ? m_desc.inPorts.size()
                                            : m_desc.outPorts.size();
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType t, QtNodes::PortIndex i) const override {
        const NodePortDesc* p = (t == QtNodes::PortType::In) ? m_desc.findIn(i) : m_desc.findOut(i);
        if(!p) return {};
        return {p->dataType, p->name}; // id=数据类型(编译校验用), name=端口名
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex const) override {}
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const) override { return nullptr; }

    QWidget* embeddedWidget() override {
        auto* label = new QLabel(m_desc.type);
        label->setAlignment(Qt::AlignCenter);
        return label;
    }

private:
    NodeTypeDesc m_desc; // 按值持有: 目录可能是临时对象, 指针会悬垂 (测试即踩中)
};

} // namespace

namespace GraphQtBridge {

std::shared_ptr<QtNodes::NodeDelegateModelRegistry> makeEditorRegistry(const NodeRegistry& catalog) {
    auto registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
    for(const QString& type : catalog.typeNames()) {
        NodeTypeDesc desc = *catalog.find(type); // 拷贝进闭包, 生命周期与注册表一致
        registry->registerModel([desc]() { return std::make_unique<CatalogDelegateModel>(desc); },
                                QStringLiteral("Kotocord"));
    }
    return registry;
}

void loadInto(QtNodes::DataFlowGraphModel& editorModel, const GraphModel& graph) {
    QHash<NodeId, QtNodes::NodeId> idMap; // 我们的 id → QtNodes id
    for(const auto& node : graph.nodes()) {
        QtNodes::NodeId qtId = editorModel.addNode(node.type);
        idMap[node.id] = qtId;
        // 简单自动排布: 每行 4 个, 间距 (260, 180)
        int row = idMap.size() - 1;
        editorModel.setNodeData(qtId, QtNodes::NodeRole::Position,
                                QPointF(60 + (row % 4) * 260, 60 + (row / 4) * 180));
    }
    for(const auto& edge : graph.edges()) {
        editorModel.addConnection({idMap.value(edge.from.node), static_cast<QtNodes::PortIndex>(edge.from.port),
                                   idMap.value(edge.to.node), static_cast<QtNodes::PortIndex>(edge.to.port)});
    }}

GraphModel extractFrom(const QtNodes::DataFlowGraphModel& editorModel) {
    GraphModel out;
    QHash<QtNodes::NodeId, NodeId> idMap; // QtNodes id → 我们的 id
    for(QtNodes::NodeId qtId : editorModel.allNodeIds()) {
        QString type = editorModel.nodeData(qtId, QtNodes::NodeRole::Type).toString();
        if(type.isEmpty()) continue;
        idMap[qtId] = out.addNode(type);
    }
    // 连接枚举: 只从 Out 侧遍历, 避免重复
    for(QtNodes::NodeId qtId : editorModel.allNodeIds()) {
        const unsigned outCount = editorModel.nodeData(qtId, QtNodes::NodeRole::OutPortCount).toUInt();
        for(unsigned p = 0; p < outCount; ++p) {
            for(const auto& c : editorModel.connections(qtId, QtNodes::PortType::Out, p)) {
                out.addEdge({idMap.value(c.outNodeId), static_cast<int>(c.outPortIndex), PortDirection::Out},
                            {idMap.value(c.inNodeId), static_cast<int>(c.inPortIndex), PortDirection::In});
            }
        }
    }
    return out;
}

} // namespace GraphQtBridge
