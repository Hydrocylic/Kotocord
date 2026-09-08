#include "SubtitleRenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QWindow>
#include <QDebug>

SubtitleRenderer::SubtitleRenderer(QWidget* parent)
    : QWidget(parent) {

    // 设置窗口标志：无边框 | 保持置顶 | 作为工具窗口（不在任务栏显示独立图标，可选）
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);// 允许背景透明
    resize(1000, 200);// 设置一个比较宽的初始大小，适合放字幕

	// 初始化第一帧结构体
	m_currentFrame.frameId = -1;//-1，防止与真实的第一句(frameId=0)冲突
	m_currentFrame.displayText = "Kotocord Ready!";
	m_currentFrame.isFinal = true; // 设为 true 才能显示渐变色
	m_currentFrame.isLlmProcessed = true; // 假设以处理，强行激活渐变色

	buildTextPath(); // 初始化时生成一次图形
}

void SubtitleRenderer::updateFrame(const SubtitleFrame& frame) {
	// 简单的时序保护：丢弃旧结果
	if(m_currentFrame.frameId > frame.frameId) {
		return;
	}

	m_currentFrame = frame;
	buildTextPath();//重新计算图形轮廓
	update();
}

void SubtitleRenderer::resizeEvent(QResizeEvent* event) {
	QWidget::resizeEvent(event);
	buildTextPath();//窗口大小被改变时重新排版
}

// 委托排版引擎计算, 将结果转为图形缓存
void SubtitleRenderer::buildTextPath() {
	m_textPath = QPainterPath(); // 清空旧缓存
	if(m_currentFrame.displayText.isEmpty()) return;

	// 定义安全渲染区域 (留出 20 像素的内边距，防止描边被切断)
	QRect renderBox = rect().adjusted(20,20,-20,-20);
	LayoutResult layout = m_layoutEngine.layout(m_currentFrame.displayText, renderBox);

	for(const LayoutLine& line : layout.lines) {
		m_textPath.addText(line.x, line.y, layout.font, line.text);
	}
}

void SubtitleRenderer::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
	if(m_currentFrame.displayText.isEmpty() || m_textPath.isEmpty()) return;

    QPainter painter(this);
    // 每次绘制前清空背景，防止 OBS 捕获到残影
    painter.fillRect(rect(), Qt::transparent);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 底层：黑色半透明阴影
	painter.translate(4,4);
	painter.setPen(QPen(QColor(0,0,0,150),6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(m_textPath);
	painter.translate(-4,-4);

    // 中层：白色粗描边
	painter.setPen(QPen(QColor(255,255,255),4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
	painter.setBrush(Qt::NoBrush);
	painter.drawPath(m_textPath);

    // 顶层：文字内部渐变填充
	painter.setPen(Qt::NoPen);
	if(!m_currentFrame.isLlmProcessed && m_currentFrame.isFinal) {
		// 【第一阶段：大模型正在思考中】
		// 比如用纯白色，并且在句尾悄悄加上 "..." 提示观众 AI 正在算
		painter.setBrush(QColor(255,255,255));
		// 你甚至可以在上面 path.addText 的时候，给这段文字加上一个淡入淡出的动画
	} else {
		// 【第二阶段：大模型分析完毕】
		// 使用你之前酷炫的粉蓝渐变色
		QRect renderBox = rect().adjusted(20,20,-20,-20);
		QLinearGradient gradient(0,renderBox.top(),0,renderBox.bottom());
		gradient.setColorAt(0.0,QColor(255,175,204));
		gradient.setColorAt(1.0,QColor(162,210,255));
		painter.setBrush(gradient);
	}
	painter.drawPath(m_textPath);
}

// 鼠标拖拽逻辑实现
void SubtitleRenderer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Wayland 支持
		if(!windowHandle() || !windowHandle()->startSystemMove()) {
			// 如果不支持原生拖拽，降级为记录手动鼠标坐标
			m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
		}
		event->accept();
    }
}

void SubtitleRenderer::mouseMoveEvent(QMouseEvent* event) {
    // 仅非 Wayland
    if ((event->buttons() & Qt::LeftButton) && !m_dragPosition.isNull()) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}
