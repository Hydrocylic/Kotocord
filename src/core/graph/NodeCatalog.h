#ifndef NODECATALOG_H
#define NODECATALOG_H

#include "NodeRegistry.h"

// M4a: 真实节点目录 — 把 kotocord 实际模块注册进 NodeRegistry
// 注册处用静态类型写连接器 (决策 D-003: 图→编译→管线)
// 端口数据类型 (字符串): asr_text(QString,bool) / text(QString) /
//                        subtitle_frame(SubtitleFrame) / audio(QByteArray)

class NodeCatalog {
public:
    static NodeRegistry build();
};

#endif // NODECATALOG_H
