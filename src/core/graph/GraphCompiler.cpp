#include "GraphCompiler.h"
#include <QQueue>
#include <QDebug>

CompileResult GraphCompiler::validateInternal(const GraphModel& model, const NodeRegistry& registry) {
    CompileResult result;

    // 1. 节点类型均已注册
    for(const auto& node : model.nodes()) {
        if(!registry.find(node.type)) {
            result.errors << QString("节点 %1: 未注册的节点类型 '%2'").arg(node.id).arg(node.type);
        }
    }

    // 2. 边校验: 端口存在、方向正确、数据类型匹配、连接器存在
    for(const auto& edge : model.edges()) {
        const GraphNodeDesc* srcNode = model.findNode(edge.from.node);
        const GraphNodeDesc* dstNode = model.findNode(edge.to.node);
        if(!srcNode || !dstNode) {
            result.errors << QString("边 %1: 引用了不存在的节点").arg(edge.id);
            continue;
        }
        const NodeTypeDesc* srcType = registry.find(srcNode->type);
        const NodeTypeDesc* dstType = registry.find(dstNode->type);
        if(!srcType || !dstType) continue; // 类型未注册已在上面报过

        const NodePortDesc* outPort = srcType->findOut(edge.from.port);
        const NodePortDesc* inPort = dstType->findIn(edge.to.port);
        if(!outPort) {
            result.errors << QString("边 %1: 节点 %2 (%3) 不存在输出端口 #%4")
                                .arg(edge.id).arg(srcNode->id).arg(srcNode->type).arg(edge.from.port);
            continue;
        }
        if(!inPort) {
            result.errors << QString("边 %1: 节点 %2 (%3) 不存在输入端口 #%4")
                                .arg(edge.id).arg(dstNode->id).arg(dstNode->type).arg(edge.to.port);
            continue;
        }
        if(outPort->dataType != inPort->dataType) {
            result.errors << QString("边 %1: 端口类型不匹配 (%2.%3 是 %4, %5.%6 需要 %7)")
                                .arg(edge.id).arg(srcNode->type).arg(outPort->name).arg(outPort->dataType)
                                .arg(dstNode->type).arg(inPort->name).arg(inPort->dataType);
            continue;
        }
        if(!srcType->findConnector(edge.from.port, dstNode->type, edge.to.port)) {
            result.errors << QString("边 %1: %2 → %3 之间没有已注册的连接器")
                                .arg(edge.id).arg(srcNode->type).arg(dstNode->type);
        }
    }

    // 3. 环检测 (DAG)
    if(hasCycle(model)) {
        result.errors << "图中存在环, 管线必须是 DAG";
    }

    result.ok = result.errors.isEmpty();
    return result;
}

bool GraphCompiler::hasCycle(const GraphModel& model) {
    // Kahn 拓扑: 入度归零法
    QHash<NodeId, int> inDegree;
    QHash<NodeId, QVector<NodeId>> adj; // 邻接表: src → dsts
    for(const auto& n : model.nodes()) inDegree[n.id] = 0;
    for(const auto& e : model.edges()) {
        adj[e.from.node].append(e.to.node);
        inDegree[e.to.node]++;
    }
    QQueue<NodeId> ready;
    for(auto it = inDegree.begin(); it != inDegree.end(); ++it) {
        if(it.value() == 0) ready.enqueue(it.key());
    }
    int processed = 0;
    while(!ready.isEmpty()) {
        NodeId n = ready.dequeue();
        ++processed;
        for(NodeId next : adj.value(n)) {
            if(--inDegree[next] == 0) ready.enqueue(next);
        }
    }
    return processed < inDegree.size(); // 有节点未归零 → 存在环
}

CompileResult GraphCompiler::validate(const GraphModel& model, const NodeRegistry& registry) {
    return validateInternal(model, registry);
}

CompileResult GraphCompiler::compile(const GraphModel& model, const NodeRegistry& registry, Pipeline& pipeline) {
    // 先完整校验, 失败则不动 pipeline (保守: 不产生半装配状态)
    CompileResult result = validateInternal(model, registry);
    if(!result.ok) return result;

    // 实例化 (按图内节点顺序)
    QHash<NodeId, int> indexByNode;
    for(const auto& node : model.nodes()) {
        const NodeTypeDesc* desc = registry.find(node.type);
        QObject* instance = desc->create(node.params);
        if(!instance) {
            result.ok = false;
            result.errors << QString("节点 %1 (%2): 实例化失败").arg(node.id).arg(node.type);
            return result;
        }
        indexByNode[node.id] = pipeline.moduleCount();
        pipeline.addModule(std::unique_ptr<QObject>(instance));
        result.instances[node.id] = instance;
    }

    // 连接 (先全部实例存在, 再建边 — 避免半连接态)
    for(const auto& edge : model.edges()) {
        const GraphNodeDesc* srcNode = model.findNode(edge.from.node);
        const GraphNodeDesc* dstNode = model.findNode(edge.to.node);
        const NodeTypeDesc* srcType = registry.find(srcNode->type);
        const PortConnector* conn = srcType->findConnector(edge.from.port, dstNode->type, edge.to.port);
        QObject* srcObj = result.instances.value(edge.from.node);
        QObject* dstObj = result.instances.value(edge.to.node);
        pipeline.addConnection(conn->connect(srcObj, dstObj));
    }
    return result;
}
