#include "PromptContext.h"

// whisper 官方建议: 中文转写附加固定提示语可显著改善标点与选词
static const QString kMandarinHint = QStringLiteral("以下是普通话的句子。");

PromptContext::PromptContext(int maxContext)
	: m_maxContext(maxContext < 1 ? 1 : maxContext)
{
}

void PromptContext::push(const QString& text) {
	const QString trimmed = text.trimmed();
	if(trimmed.isEmpty()) return;
	m_context.append(trimmed);
	while(m_context.size() > m_maxContext)
		m_context.removeFirst();
}

QString PromptContext::build() const {
	QString prompt;
	if(!m_context.isEmpty())
		prompt = m_context.join('\n') + '\n';
	prompt += kMandarinHint;
	return prompt;
}

void PromptContext::clear() {
	m_context.clear();
}
