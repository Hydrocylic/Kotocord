#include <QtTest>
#include <QTemporaryFile>
#include <QDir>

#include "modules/input/DomainDictionary.h"

// Phase 5 L2: 热词后处理词典 — 编辑距离纠偏 (引擎无关)
class TestDomainDictionary : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        m_dict.setTerms({"击杀", "打野位", "走位", "CD", "DPS"});
    }

    // 1. 编辑距离基准
    void testLevenshteinBasics() {
        QCOMPARE(levenshteinDistance(QString("击杀"), QString("机杀")), 1);
        QCOMPARE(levenshteinDistance(QString("kitten"), QString("sitting")), 3);
        QCOMPARE(levenshteinDistance(QString(""), QString("")), 0);
        QCOMPARE(levenshteinDistance(QString(""), QString("abc")), 3);
    }

    // 2. 中文同音字纠偏: 机杀 → 击杀 (2 字词, 距离 1)
    void testChineseCorrection() {
        QCOMPARE(m_dict.correct(QStringLiteral("我去机杀那个boss")),
                 QStringLiteral("我去击杀那个boss"));
    }

    // 3. 三字词纠偏: 打野味 → 打野位 (距离 1)
    void testThreeCharCorrection() {
        QCOMPARE(m_dict.correct(QStringLiteral("我们打野味")),
                 QStringLiteral("我们打野位"));
    }

    // 4. 两字词距离 2 不替换 (防误纠): 机战 → 击杀? 不
    void testTwoCharDistanceTwoRejected() {
        QCOMPARE(m_dict.correct(QStringLiteral("机战")), QStringLiteral("机战"));
    }

    // 5. 拉丁词大小写归一: dps → DPS, cd → CD
    void testLatinNormalize() {
        QCOMPARE(m_dict.correct(QStringLiteral("我们cd到了")), QStringLiteral("我们CD到了"));
        QCOMPARE(m_dict.correct(QStringLiteral("这波dps很高")), QStringLiteral("这波DPS很高"));
    }

    // 6. 拉丁词不做模糊匹配: DP 不会被 CD 吞掉 (距离 1 也拒绝)
    void testLatinExactOnly() {
        QCOMPARE(m_dict.correct(QStringLiteral("DP")), QStringLiteral("DP"));
    }

    // 7. 无关文本不被误纠
    void testNoFalsePositive() {
        QCOMPARE(m_dict.correct(QStringLiteral("今天天气不错")), QStringLiteral("今天天气不错"));
    }

    // 8. 已正确文本保持不变 (幂等)
    void testIdempotent() {
        QCOMPARE(m_dict.correct(QStringLiteral("击杀")), QStringLiteral("击杀"));
    }

    // 9. 长词优先: 走位失误 整体匹配, 不被 走位 抢先截断
    void testLongerTermWins() {
        DomainDictionary dict;
        dict.setTerms({"走位", "走位失误"});
        QCOMPARE(dict.correct(QStringLiteral("走位失误")), QStringLiteral("走位失误"));
    }

    // 10. 回归: 精确词带前置字不被吞 (首轮 ctest 失败根因 — 4 字窗口前导删除)
    void testExactTermWithContextNotMangled() {
        QCOMPARE(m_dict.correct(QStringLiteral("他打野位")), QStringLiteral("他打野位"));
        QCOMPARE(m_dict.correct(QStringLiteral("我击杀了他")), QStringLiteral("我击杀了他"));
    }

    // 11. 从 JSON 文件加载词表
    void testLoadFromJson() {
        QTemporaryFile tmpFile;
        tmpFile.setFileTemplate(QDir::tempPath() + "/kotocord_dict_XXXXXX.json");
        QVERIFY(tmpFile.open());
        tmpFile.write(R"({"terms": ["击杀", "开团"]})");
        tmpFile.flush();
        tmpFile.close();

        DomainDictionary dict;
        QVERIFY(dict.loadFromFile(tmpFile.fileName()));
        QCOMPARE(dict.correct(QStringLiteral("机杀")), QStringLiteral("击杀"));
    }

    // 12. 加载不存在的文件 → false, correct 旁路原样返回
    void testLoadMissingFile() {
        DomainDictionary dict;
        QVERIFY(!dict.loadFromFile(QStringLiteral("/nonexistent/domain-dict.json")));
        QVERIFY(dict.isEmpty());
        QCOMPARE(dict.correct(QStringLiteral("机杀")), QStringLiteral("机杀"));
    }

    // 13. 空词典旁路 (未 setTerms / 未加载)
    void testEmptyDictionaryPassthrough() {
        DomainDictionary dict;
        QCOMPARE(dict.correct(QStringLiteral("任意文本")), QStringLiteral("任意文本"));
    }

private:
    DomainDictionary m_dict;
};

QTEST_MAIN(TestDomainDictionary)
#include "TestDomainDictionary.moc"
