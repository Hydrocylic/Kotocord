#ifndef NODEEDITORVIEW_H
#define NODEEDITORVIEW_H

#include <QWidget>

// M4: 节点编辑器视图 — QtNodes GraphicsView 的宿主容器
// spike 版: 只验证 FetchContent 集成与头文件编译; 节点注册/图装配随后接入
class NodeEditorView : public QWidget {
    Q_OBJECT
public:
    explicit NodeEditorView(QWidget* parent = nullptr);
    ~NodeEditorView() override;

private:
    struct Impl;               // QtNodes 类型不进头文件, 减少 UI 层耦合
    Impl* d = nullptr;
};

#endif // NODEEDITORVIEW_H
