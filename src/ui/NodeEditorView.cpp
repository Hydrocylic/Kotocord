#include "NodeEditorView.h"
#include <QVBoxLayout>

#include <QtNodes/BasicGraphicsScene>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>

// M4 spike: 验证 QtNodes (FetchContent, commit 7bbcd3e) 与 Qt 6.9.3 编译链路
// 空 DataFlowGraphModel + 空 registry — 后续节点注册接入后即为可用编辑器
struct NodeEditorView::Impl {
    QtNodes::DataFlowGraphModel model;
    QtNodes::BasicGraphicsScene scene;
    QtNodes::GraphicsView view;
    Impl()
        : model(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
        , scene(model) {
        view.setScene(&scene);
    }
};

NodeEditorView::NodeEditorView(QWidget* parent)
    : QWidget(parent)
    , d(new Impl()) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&d->view);
}

NodeEditorView::~NodeEditorView() {
    delete d;
}
