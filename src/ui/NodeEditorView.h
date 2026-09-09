#ifndef NODEEDITORVIEW_H
#define NODEEDITORVIEW_H

#include <QWidget>

#include <QtNodes/DataFlowGraphModel>

#include "../core/graph/GraphModel.h"
#include "../core/graph/NodeRegistry.h"

// M4b: 节点编辑器视图 — QtNodes 编辑画布 + 桥接
// 编辑的是"幻影"节点 (端口/名称), 应用时导出 GraphModel 走编译重建 (决策 D-003)
class NodeEditorView : public QWidget {
    Q_OBJECT
public:
    explicit NodeEditorView(const NodeRegistry& catalog, QWidget* parent = nullptr);
    ~NodeEditorView() override;

    // 载入一张图 (模板/当前运行图) 到编辑画布
    void setGraph(const GraphModel& graph);
    // 导出编辑中的图 (应用/校验)
    GraphModel graph() const;

    // 只读校验 (供 Apply 前反馈; 不装配)
    QStringList validateAgainst(const NodeRegistry& catalog) const;

signals:
    void graphChangedByUser();// 用户在画布上增删节点/连接 (轻提示用)

private:
    struct Impl;
    Impl* d = nullptr;
};

#endif // NODEEDITORVIEW_H
