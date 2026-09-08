#ifndef GRAPHCOMPILER_H
#define GRAPHCOMPILER_H

#include <QStringList>
#include <QHash>

#include "GraphModel.h"
#include "NodeRegistry.h"
#include "Pipeline.h"

// M2: GraphCompiler — 校验图并装配 Pipeline
// 校验: 节点类型已注册 / 边的端口存在且方向正确 / 端口数据类型匹配 / 无环 (DAG)
// 装配: 实例工厂创建节点 → 连接器建立信号槽 → 交给 Pipeline 持有
// 校验失败: 返回错误清单 (可读, 供编辑器展示), Pipeline 保持原状不装配

struct CompileResult {
    bool ok = false;
    QStringList errors;

    // 成功时的映射: 图节点 id → Pipeline 内实例 (下标对应 moduleAt)
    QHash<NodeId, QObject*> instances;
};

class GraphCompiler {
public:
    // 校验 + 装配一步完成; 只在全部校验通过后才动 pipeline
    static CompileResult compile(const GraphModel& model, const NodeRegistry& registry, Pipeline& pipeline);

    // 仅校验不装配 (编辑器实时反馈用)
    static CompileResult validate(const GraphModel& model, const NodeRegistry& registry);

private:
    static CompileResult validateInternal(const GraphModel& model, const NodeRegistry& registry);
    static bool hasCycle(const GraphModel& model);
};

#endif // GRAPHCOMPILER_H
