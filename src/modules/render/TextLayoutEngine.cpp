#include "TextLayoutEngine.h"
#include <QFontMetrics>
#include <QRect>

TextLayoutEngine::TextLayoutEngine(const QString& fontFamily,
                                   int initialFontSize,
                                   int minFontSize,
                                   int fontStep,
                                   int weight)
    : m_fontFamily(fontFamily)
    , m_initialFontSize(initialFontSize)
    , m_minFontSize(minFontSize)
    , m_fontStep(fontStep)
    , m_weight(weight) {}

QStringList TextLayoutEngine::tokenize(const QString& text) {
	QStringList tokens;
	QString currentToken;
	for(int i = 0; i < text.length(); ++i) {
		QChar c = text[i];
		// 如果是空格，或者是中日韩统一表意文字 (CJK)，视为独立的断句点
		if(c.isSpace() || (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FA5)) {
			if(!currentToken.isEmpty()) {// 把汉字或空格作为一个独立的 token
				tokens.append(currentToken);
				currentToken.clear();
			}
			tokens.append(QString(c));
		} else {// 字母、数字、颜文字符号，全部"粘"在一起作为一个整体 Token
			currentToken.append(c);
		}
	}
	if(!currentToken.isEmpty()) tokens.append(currentToken);
	return tokens;
}

LayoutResult TextLayoutEngine::layout(const QString& text, const QRect& renderBox) const {
	LayoutResult result;
	if(text.isEmpty()) return result;

	QFont font(m_fontFamily, m_initialFontSize, m_weight);
	QStringList lines;
	int lineHeight = 0;

	// 排版循环：动态换行与缩放
	int fontSize = m_initialFontSize;
	while(fontSize > m_minFontSize) {// 字体最小不能小于 12
		font.setPointSize(fontSize);
		QFontMetrics fm(font);
		lineHeight = fm.height();
		lines.clear();
		QStringList tokens = tokenize(text);

		QString currentLine = "";// 按 Token 拼装行
		for(const QString& token : tokens) {
			QString testLine = currentLine + token;
			// 如果加上这个 Token 超宽了，并且当前行不是空的，就强制换行
			if(fm.horizontalAdvance(testLine) > renderBox.width() && !currentLine.isEmpty()) {
				lines.append(currentLine);
				currentLine = token.trimmed().isEmpty() ? "" : token;
			} else {// 如果导致换行的是个空格，下一行就不需要以空格开头了
				currentLine = testLine;
			}
		}
		lines.append(currentLine); // 把最后一行加进去

		if(lines.size() * lineHeight <= renderBox.height()) {// 检查总高度是否能放进窗口
			break;// 完美容纳，跳出循环
		}
		fontSize -= m_fontStep;// 放不下，缩小字体继续算
	}

	result.font = font;
	QFontMetrics finalMetrics(font);
	int totalTextHeight = lines.size() * lineHeight;
	// 计算 Y 轴整体居中的起始位置
	int startY = renderBox.top() + (renderBox.height() - totalTextHeight) / 2 + finalMetrics.ascent();

	for(int i = 0; i < lines.size(); ++i) {
		int lineWidth = finalMetrics.horizontalAdvance(lines[i]);
		int startX = renderBox.left() + (renderBox.width() - lineWidth) / 2;// 计算每一行 X 轴居中的位置
		result.lines.append({lines[i], startX, startY + i * lineHeight});
	}
	return result;
}
