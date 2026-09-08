#ifndef NODEREGISTRY_H
#define NODEREGISTRY_H

#include <QString>
#include <QHash>
#include <QVector>
#include <QObject>
#include <functional>
#include <memory>

#include "GraphModel.h"

// M2: 节点注册表 — 节点元信息 (端口名/数据类型) + 实例工厂 + 信号槽连接器
// 节点类型在注册处用静态类型写死连接器, Compiler 只做字符串/索引级别的通用查找

// 端口描述: 序号 + 名称 + 数据类型 (类型系统初版: audio/text/frame/event 等字符串)
struct NodePortDesc {
    PortIndex index;
    QString name;
    QString dataType;
};

// 某个 Out 端口与某个节点类型 In 端口之间的连接器
// 连接器闭包捕获成员指针知识 (注册处静态类型已知), Compiler 无需模板
struct PortConnector {
    PortIndex selfPort; // 本类型 Out 端口序号
    QString peerType;   // 目标节点类型
    PortIndex peerPort; // 目标 In 端口序号
    std::function<QMetaObject::Connection(QObject* src, QObject* dst)> connect;
};

// 节点类型描述
struct NodeTypeDesc {
    QString type;                                  // 注册键 (唯一)
    QString displayName;                           // 编辑器显示名 (M4)
    QVector<NodePortDesc> inPorts;
    QVector<NodePortDesc> outPorts;
    std::function<QObject*(const QVariantMap& params)> create; // 实例工厂

    QVector<PortConnector> connectors;             // 本类型全部合法出边连接器

    const NodePortDesc* findIn(PortIndex idx) const;
    const NodePortDesc* findOut(PortIndex idx) const;
    // 查找 本类型.outPort → peerType.peerPort 的连接器 (无则 nullptr)
    const PortConnector* findConnector(PortIndex outPort, const QString& peerType, PortIndex peerPort) const;
};

class NodeRegistry {
public:
    // 注册节点类型 (同键重复注册返回 false)
    bool registerType(NodeTypeDesc desc);

    const NodeTypeDesc* find(const QString& type) const;
    QStringList typeNames() const;

private:
    QHash<QString, NodeTypeDesc> m_types;
};

#endif // NODEREGISTRY_H
