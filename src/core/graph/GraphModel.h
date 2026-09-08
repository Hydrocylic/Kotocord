#ifndef GRAPHMODEL_H
#define GRAPHMODEL_H

#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QJsonObject>

// M2: 节点图纯数据模型 — 节点/端口/连接/参数, JSON 可序列化, 不依赖 UI
// (决策 D-003: 图即配置, 编译成现有管线; 将来换前端只动视图层 — 线头 T7)

using NodeId = int;
using PortIndex = int;

enum class PortDirection {
    In,   // 输入端口 (槽)
    Out   // 输出端口 (信号)
};

// 端口引用: 节点 + 端口序号 + 方向
struct PortRef {
    NodeId node = -1;
    PortIndex port = -1;
    PortDirection dir = PortDirection::In;

    bool isValid() const { return node >= 0 && port >= 0; }
};

// 一条连接: 源(Out 端口) → 目标(In 端口)
struct GraphEdge {
    int id = -1;
    PortRef from; // Out
    PortRef to;   // In
};

// 图节点: 类型 + 参数表 (参数 schema 由 NodeRegistry 提供, 模型只存数据)
struct GraphNodeDesc {
    NodeId id = -1;
    QString type;            // 节点类型名, 对应 NodeRegistry 注册键
    QVariantMap params;      // 节点参数 (字符串/数字等)
};

class GraphModel {
public:
    GraphModel() = default;

    // --- 构建 (编辑器/加载器调用) ---
    NodeId addNode(const QString& type, const QVariantMap& params = {});
    bool addEdge(const PortRef& from, const PortRef& to); // 基本引用检查, 拓扑校验交给 Compiler

    // --- 查询 ---
    const QVector<GraphNodeDesc>& nodes() const { return m_nodes; }
    const QVector<GraphEdge>& edges() const { return m_edges; }
    const GraphNodeDesc* findNode(NodeId id) const;

    // --- 序列化 (预置模板 JSON / 用户图保存) ---
    QJsonObject toJson() const;
    static GraphModel fromJson(const QJsonObject& obj);

    void clear();

private:
    QVector<GraphNodeDesc> m_nodes;
    QVector<GraphEdge> m_edges;
    NodeId m_nextNodeId = 0;
    int m_nextEdgeId = 0;
};

#endif // GRAPHMODEL_H
