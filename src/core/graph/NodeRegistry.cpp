#include "NodeRegistry.h"

bool NodeRegistry::registerType(NodeTypeDesc desc) {
    if(m_types.contains(desc.type)) return false;
    m_types.insert(desc.type, std::move(desc));
    return true;
}

const NodeTypeDesc* NodeRegistry::find(const QString& type) const {
    auto it = m_types.constFind(type);
    return it == m_types.constEnd() ? nullptr : &it.value();
}

QStringList NodeRegistry::typeNames() const {
    return QStringList(m_types.keyBegin(), m_types.keyEnd());
}

const NodePortDesc* NodeTypeDesc::findIn(PortIndex idx) const {
    for(const auto& p : inPorts) {
        if(p.index == idx) return &p;
    }
    return nullptr;
}

const NodePortDesc* NodeTypeDesc::findOut(PortIndex idx) const {
    for(const auto& p : outPorts) {
        if(p.index == idx) return &p;
    }
    return nullptr;
}

const PortConnector* NodeTypeDesc::findConnector(PortIndex outPort, const QString& peerType, PortIndex peerPort) const {
    for(const auto& c : connectors) {
        if(c.selfPort == outPort && c.peerType == peerType && c.peerPort == peerPort) return &c;
    }
    return nullptr;
}
