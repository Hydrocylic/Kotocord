#ifndef SUBTITLERENDERER_H
#define SUBTITLERENDERER_H

#include <QWidget>
#include <QString>
#include <QPoint>
#include <QPainterPath>
#include "../../core/DataTypes.h"
#include "TextLayoutEngine.h"

// 字幕呈现层: 无边框置顶透明窗口 + 三层描边绘制 + 拖拽
// 排版计算委托 TextLayoutEngine, 本类只负责窗口与绘制
class SubtitleRenderer : public QWidget {
    Q_OBJECT
public:
    explicit SubtitleRenderer(QWidget* parent = nullptr);

public slots:
	void updateFrame(const SubtitleFrame& frame);

protected:
    void paintEvent(QPaintEvent* event) override;// 核心绘制事件
	void resizeEvent(QResizeEvent* event) override;//窗口大小改变时触发

    // 鼠标事件，用于拖拽无边框窗口
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QPoint m_dragPosition;
	SubtitleFrame m_currentFrame; // 存下当前的完整状态帧
	QPainterPath m_textPath;// 将算好的文字图形缓存起来
	TextLayoutEngine m_layoutEngine;// 排版计算引擎 (M1 从本类抽出)

	void buildTextPath();// 调用排版引擎并把结果转成图形缓存
};

#endif // SUBTITLERENDERER_H
