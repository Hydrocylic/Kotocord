#include "DomainDictionary.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <limits>
#include <vector>

// 中文热词允许的编辑距离 (调参起点, plan 风险 #5):
//   1 字 = 0 (无纠偏意义), 2 字及以上 ≤ 1。
//   曾设 3 字以上 ≤ 2 — 实测失败: "我们打野味" 中 4 字窗口 "们打野味" 与 "打野位"
//   距离 2 (删 们 + 味→位) 被放行, 吞掉了正确的前置字 (见 log 2026-08-31 首轮 ctest)。
//   2 距离允许「删首字+插尾字」形态, 滑动窗口下歧义过高 — 欠纠优于误纠, 收紧为 ≤1。
static int maxAllowedDistance(int termLen) {
	return termLen >= 2 ? 1 : 0;
}

// 纯 ASCII 词 (游戏术语常见, 如 DPS/buff) — 仅做大小写归一, 不做模糊匹配
static bool isLatinTerm(const QString& term) {
	for(const QChar c : term) {
		if(c.unicode() >= 128) return false;
	}
	return true;
}

int levenshteinDistance(const QString& a, const QString& b) {
	const int m = a.size();
	const int n = b.size();
	std::vector<int> prev(n + 1), curr(n + 1);
	for(int j = 0; j <= n; ++j) prev[j] = j;
	for(int i = 1; i <= m; ++i) {
		curr[0] = i;
		for(int j = 1; j <= n; ++j) {
			const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
			curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
		}
		std::swap(prev, curr);
	}
	return prev[n];
}

bool DomainDictionary::loadFromFile(const QString& jsonPath) {
	QFile file(jsonPath);
	if(!file.open(QIODevice::ReadOnly)) {
		return false;
	}

	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	if(!doc.isObject()) {
		return false;
	}

	const QJsonArray arr = doc.object()["terms"].toArray();
	QStringList terms;
	for(const QJsonValue& v : arr) {
		const QString term = v.toString().trimmed();
		if(!term.isEmpty()) terms.append(term);
	}
	setTerms(terms);
	return true;
}

void DomainDictionary::setTerms(const QStringList& terms) {
	m_terms = terms;
	m_terms.removeDuplicates();
	std::sort(m_terms.begin(), m_terms.end(), [](const QString& a, const QString& b) {
		return a.size() > b.size();  // 长词优先: 避免短词抢先吞掉长词的匹配机会
	});
}

int DomainDictionary::matchAndReplace(const QString& text, int pos, const QString& term) const {
	const int termLen = term.size();
	const bool latin = isLatinTerm(term);
	// 窗口长度: 中文 ±1 容差 (识别增删字); 拉丁词必须等长 (仅大小写归一)
	const int minLen = latin ? termLen : std::max(1, termLen - 1);
	const int maxLen = latin ? termLen : termLen + 1;
	const int maxDist = maxAllowedDistance(termLen);

	// 候选窗口取「距离最小, 平局取更长窗口」:
	// 更短窗口距离更小仅发生在删字场景 (如 "打位"→"打野位" 的 len2 窗口 d=1),
	// 平局时更长窗口覆盖增字场景 ("打野两位" 的 len4 窗口) — 两者取长稳赢
	int bestLen = 0;
	int bestDist = std::numeric_limits<int>::max();

	for(int len = minLen; len <= maxLen; ++len) {
		if(pos + len > text.size()) break;
		const QString window = text.mid(pos, len);
		int dist = -1;
		if(latin) {
			if(window.compare(term, Qt::CaseInsensitive) == 0) dist = 0;
		} else {
			// ±1 窗口禁止首字符偏离: 防「前导删除」吞掉前一个正确字符
			// (如 "们打野味" 的 4 字窗口删 们 后匹配 打野位)。等长窗口不限制 (机杀→击杀 需首字替换)
			if(len != termLen && window.at(0) != term.at(0)) continue;
			dist = levenshteinDistance(window, term);
		}
		// 达标: 距离在阈值内 且 至少有一个字符真正匹配 (dist < len, 防单字窗口过度纠偏)
		if(dist < 0 || dist > maxDist || dist >= len) continue;
		if(dist < bestDist || (dist == bestDist && len > bestLen)) {
			bestDist = dist;
			bestLen = len;
		}
	}
	return bestLen;
}

QString DomainDictionary::correct(const QString& text) const {
	if(m_terms.isEmpty() || text.isEmpty())
		return text;

	QString result;
	result.reserve(text.size());

	int i = 0;
	while(i < text.size()) {
		bool replaced = false;
		for(const QString& term : m_terms) {
			const int consumed = matchAndReplace(text, i, term);
			if(consumed > 0) {
				result += term;
				i += consumed;
				replaced = true;
				break;
			}
		}
		if(!replaced) {
			result += text[i];
			++i;
		}
	}
	return result;
}
