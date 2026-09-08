#include "GraphModel.h"
#include <QJsonArray>

NodeId GraphModel::addNode(const QString& type, const QVariantMap& params) {
    GraphNodeDesc node;
    node.id = m_nextNodeId++;
    node.type = type;
    node.params = params;
    m_nodes.append(node);
    return node.id;
}

bool GraphModel::addEdge(const PortRef& from, const PortRef& to) {
    // 基本引用检查 (存在性/方向); 端口类型匹配与环检测是 Compiler 的职责
    if(!from.isValid() || !to.isValid()) return false;
    if(from.dir != PortDirection::Out || to.dir != PortDirection::In) return false;
    if(!findNode(from.node) || !findNode(to.node)) return false;

    GraphEdge edge;
    edge.id = m_nextEdgeId++;
    edge.from = from;
    edge.to = to;
    m_edges.append(edge);
    return true;
}

const GraphNodeDesc* GraphModel::findNode(NodeId id) const {
    for(const auto& n : m_nodes) {
        if(n.id == id) return &n;
    }
    return nullptr;
}

QJsonObject GraphModel::toJson() const {
    QJsonArray nodesArr;
    for(const auto& n : m_nodes) {
        QJsonObject nObj;
        nObj["id"] = n.id;
        nObj["type"] = n.type;
        nObj["params"] = QJsonObject::fromVariantMap(n.params);
        nodesArr.append(nObj);
    }
    QJsonArray edgesArr;
    for(const auto& e : m_edges) {
        QJsonObject eObj;
        eObj["id"] = e.id;
        eObj["fromNode"] = e.from.node;
        eObj["fromPort"] = e.from.port;
        eObj["toNode"] = e.to.node;
        eObj["toPort"] = e.to.port;
        edgesArr.append(eObj);
    }
    QJsonObject root;
    root["nodes"] = nodesArr;
    root["edges"] = edgesArr;
    return root;
}

GraphModel GraphModel::fromJson(const QJsonObject& obj) {
    GraphModel model;
    for(const auto& v : obj["nodes"].toArray()) {
        QJsonObject nObj = v.toObject();
        GraphNodeDesc node;
        node.id = nObj["id"].toInt();
        node.type = nObj["type"].toString();
        node.params = nObj["params"].toObject().toVariantMap();
        model.m_nodes.append(node);
        if(node.id >= model.m_nextNodeId) model.m_nextNodeId = node.id + 1;
    }
    for(const auto& v : obj["edges"].toArray()) {
        QJsonObject eObj = v.toObject();
        GraphEdge edge;
        edge.id = eObj["id"].toInt();
        edge.from = {eObj["fromNode"].toInt(), eObj["fromPort"].toInt(), PortDirection::Out};
        edge.to = {eObj["toNode"].toInt(), eObj["toPort"].toInt(), PortDirection::In};
        model.m_edges.append(edge);
        if(edge.id >= model.m_nextEdgeId) model.m_nextEdgeId = edge.id + 1;
    }
    return model;
}

void GraphModel::clear() {
    m_nodes.clear();
    m_edges.clear();
    m_nextNodeId = 0;
    m_nextEdgeId = 0;
}
