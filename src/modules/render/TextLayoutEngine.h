#ifndef TEXTLAYOUTENGINE_H
#define TEXTLAYOUTENGINE_H

#include <QString>
#include <QStringList>
#include <QFont>
#include <QVector>
#include <QSize>

// 单行排版结果: 行文本 + 首字符基线起点 (widget 坐标系)
struct LayoutLine {
	QString text;
	int x = 0;
	int y = 0; // 基线 y
};

// 一次完整排版的结果: 字体 + 各行位置, 呈现层据此绘制
struct LayoutResult {
	QFont font;
	QVector<LayoutLine> lines;
};

// 纯排版计算引擎: 文本 + 渲染区域 → 字号自适应 + 分词换行 + 居中定位
// 不依赖 Widgets, 可脱离窗口单测
class TextLayoutEngine {
public:
	TextLayoutEngine(const QString& fontFamily = "Arial",
	                 int initialFontSize = 60,
	                 int minFontSize = 12,
	                 int fontStep = 2,
	                 int weight = QFont::Black);

	// renderBox: 安全渲染区域 (已扣除内边距)
	LayoutResult layout(const QString& text, const QRect& renderBox) const;

private:
	// 智能分词: 空格与 CJK 字符为独立断点, 字母/数字/颜文字符号粘连成整体
	static QStringList tokenize(const QString& text);

	QString m_fontFamily;
	int m_initialFontSize;
	int m_minFontSize;
	int m_fontStep;
	int m_weight;
};

#endif // TEXTLAYOUTENGINE_H
