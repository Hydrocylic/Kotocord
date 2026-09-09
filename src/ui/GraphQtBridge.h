#ifndef GRAPHQTBRIDGE_H
#define GRAPHQTBRIDGE_H

// M4b: 自研图核心 (GraphModel/NodeCatalog) ↔ QtNodes 编辑器 的桥接层
// 编辑器里的节点是"幻影" (CatalogDelegateModel 只描述端口/名称, 不实例化运行时模块)
// 应用时 extractFrom() 导出 GraphModel → GraphCompiler → Pipeline 停机重建

#include <memory>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include "../core/graph/GraphModel.h"
#include "../core/graph/NodeRegistry.h"

namespace GraphQtBridge {

// 用节点目录构建 QtNodes 委托注册表 (每种类型一个泛型委托模型)
std::shared_ptr<QtNodes::NodeDelegateModelRegistry> makeEditorRegistry(const NodeRegistry& catalog);

// 我们的图 → QtNodes 编辑模型 (节点位置自动排布)
void loadInto(QtNodes::DataFlowGraphModel& editorModel, const GraphModel& graph);

// QtNodes 编辑模型 → 我们的图 (应用/保存时)
GraphModel extractFrom(const QtNodes::DataFlowGraphModel& editorModel);

} // namespace GraphQtBridge

#endif // GRAPHQTBRIDGE_H
