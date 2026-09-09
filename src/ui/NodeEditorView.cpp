#include "NodeEditorView.h"
#include "GraphQtBridge.h"
#include "../core/graph/GraphCompiler.h"

#include <QVBoxLayout>

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/GraphicsView>

struct NodeEditorView::Impl {
    QtNodes::DataFlowGraphModel model;
    QtNodes::BasicGraphicsScene scene;
    QtNodes::GraphicsView view;

    explicit Impl(std::shared_ptr<QtNodes::NodeDelegateModelRegistry> registry)
        : model(std::move(registry))
        , scene(model) {
        view.setScene(&scene);
    }
};

NodeEditorView::NodeEditorView(const NodeRegistry& catalog, QWidget* parent)
    : QWidget(parent)
    , d(new Impl(GraphQtBridge::makeEditorRegistry(catalog))) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&d->view);

    // 用户编辑后发轻提示信号 (应用需点 Apply — 停机重建语义, 不自动生效)
    connect(&d->model, &QtNodes::AbstractGraphModel::connectionCreated,
            this, &NodeEditorView::graphChangedByUser);
    connect(&d->model, &QtNodes::AbstractGraphModel::connectionDeleted,
            this, &NodeEditorView::graphChangedByUser);
}

NodeEditorView::~NodeEditorView() {
    delete d;
}

void NodeEditorView::setGraph(const GraphModel& graph) {
    // 清空画布重载: 移除全部节点 (QtNodes 自管内部一致性)
    auto ids = d->model.allNodeIds();
    for(QtNodes::NodeId id : ids) {
        d->model.deleteNode(id);
    }
    GraphQtBridge::loadInto(d->model, graph);
}

GraphModel NodeEditorView::graph() const {
    return GraphQtBridge::extractFrom(d->model);
}

QStringList NodeEditorView::validateAgainst(const NodeRegistry& catalog) const {
    return GraphCompiler::validate(graph(), catalog).errors;
}
