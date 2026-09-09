#ifndef PIPELINE_H
#define PIPELINE_H

#include <QObject>
#include <QVector>
#include <memory>
#include <vector>

// M2: Pipeline — 已编译图的运行容器
// 持有节点实例的所有权 + 建立的信号槽连接; clear() 时先断连再销毁 (依赖安全拆除)
// 启停策略由各节点自理 (M2 保守: 不做统一生命周期状态机, 见线头 T1 热替换)

class Pipeline {
public:
    Pipeline() = default;
    ~Pipeline(); // 释放前统一断连

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    // M4b: 停机重建需要 — 先在新 Pipeline 上编译成功, 再整体换入 (旧实例随之销毁)
    Pipeline(Pipeline&&) = default;
    Pipeline& operator=(Pipeline&&) = default;

    // Compiler 装配用
    void addModule(std::unique_ptr<QObject> module);
    void addConnection(const QMetaObject::Connection& conn);

    int moduleCount() const { return static_cast<int>(m_modules.size()); }
    int connectionCount() const { return m_connections.size(); }

    // 按节点在图中的加入序取实例 (Compiler 装配时记录的对应关系由调用方维护)
    QObject* moduleAt(int index) const;

    // 拆除: 断开全部连接, 销毁全部实例 (销毁序 = 逆序, 后建的先拆)
    void clear();

private:
    std::vector<std::unique_ptr<QObject>> m_modules;
    QVector<QMetaObject::Connection> m_connections;
};

#endif // PIPELINE_H
