#ifndef PROMPTCONTEXT_H
#define PROMPTCONTEXT_H

#include <QString>
#include <QStringList>

// Whisper initial_prompt 上下文窗口 (Phase 5 L1)
// 维护最近 N 句识别结果, 构建推理前缀提示语。
// 独立小类便于单测 (WhisperTranscriber 构造需加载模型, 不适合进测试进程)。
class PromptContext {
public:
	explicit PromptContext(int maxContext = 3);

	void push(const QString& text);   // 追加一句 (超过窗口自动滑出最旧)
	QString build() const;            // 历史句 + 固定中文提示语
	bool isEmpty() const { return m_context.isEmpty(); }
	void clear();

private:
	int m_maxContext;
	QStringList m_context;
};

#endif // PROMPTCONTEXT_H
