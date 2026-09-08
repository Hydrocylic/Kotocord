#include "Pipeline.h"

Pipeline::~Pipeline() {
    clear();
}

void Pipeline::addModule(std::unique_ptr<QObject> module) {
    m_modules.push_back(std::move(module));
}

void Pipeline::addConnection(const QMetaObject::Connection& conn) {
    m_connections.append(conn);
}

QObject* Pipeline::moduleAt(int index) const {
    if(index < 0 || index >= static_cast<int>(m_modules.size())) return nullptr;
    return m_modules[static_cast<size_t>(index)].get();
}

void Pipeline::clear() {
    // 先断连再销毁: 防止已销毁对象的信号触发到半拆状态
    for(const auto& conn : m_connections) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
    // 逆序销毁: 汇节点先于源节点拆除
    for(auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
        it->reset();
    }
    m_modules.clear();
}
