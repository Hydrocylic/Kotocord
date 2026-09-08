#ifndef DOMAINDICTIONARY_H
#define DOMAINDICTIONARY_H

#include <QString>
#include <QStringList>

// 热词后处理词典 (Phase 5 L2): 编辑距离模糊匹配纠正 ASR 输出, 引擎无关。
// 词表 JSON 格式: {"terms": ["击杀", "开团", ...]}
class DomainDictionary {
public:
	// 加载失败返回 false (correct 旁路原样返回, 程序不因此崩溃)
	bool loadFromFile(const QString& jsonPath);
	void setTerms(const QStringList& terms);  // 去重 + 按长度降序 (长词优先匹配)
	QString correct(const QString& text) const;
	bool isEmpty() const { return m_terms.isEmpty(); }

private:
	// 尝试在 text 的 pos 处用 term 替换, 成功返回消耗的字符数, 失败返回 0
	int matchAndReplace(const QString& text, int pos, const QString& term) const;

	QStringList m_terms;
};

// 编辑距离 (Levenshtein) — 滚动双行 DP; 独立暴露便于单测与复用
int levenshteinDistance(const QString& a, const QString& b);

#endif // DOMAINDICTIONARY_H
