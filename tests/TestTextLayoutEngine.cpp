#include <QtTest>

#include "modules/render/TextLayoutEngine.h"

#include <QFontMetrics>

// M1: 排版引擎单测 — 不依赖 QWidget, 验证分词/换行/字号自适应/居中定位
// (原逻辑内嵌于 SubtitleRenderer::buildTextPath, 无法单测)
class TestTextLayoutEngine : public QObject {
    Q_OBJECT

private slots:
    // 1. 空文本: 不产生任何行
    void testEmptyText() {
        TextLayoutEngine engine;
        LayoutResult r = engine.layout("", QRect(0, 0, 960, 160));
        QVERIFY(r.lines.isEmpty());
    }

    // 2. 短文本单行放下: 字号不缩小, 单行, 水平居中
    void testShortTextSingleLine() {
        TextLayoutEngine engine;
        LayoutResult r = engine.layout("Hello", QRect(0, 0, 960, 160));
        QCOMPARE(r.lines.size(), 1);
        QCOMPARE(r.font.pointSize(), 60); // 初始字号即容纳, 不缩小
        QFontMetrics fm(r.font);
        int textWidth = fm.horizontalAdvance(r.lines[0].text);
        int expectedX = (960 - textWidth) / 2;
        QCOMPARE(r.lines[0].x, expectedX);
        // 垂直居中: 基线 = 顶边 + (高-行高)/2 + ascent
        QCOMPARE(r.lines[0].y, (160 - fm.height()) / 2 + fm.ascent());
    }

    // 3. 长文本触发换行: CJK 字符逐字断行
    void testCjkWrapping() {
        TextLayoutEngine engine;
        QString text;
        for (int i = 0; i < 50; ++i) text += QStringLiteral("字");
        LayoutResult r = engine.layout(text, QRect(0, 0, 960, 160));
        QVERIFY(r.lines.size() > 1); // 必然换行
        // 每行都不超宽 (或字号已到下限仍尽量收缩)
        QFontMetrics fm(r.font);
        for (const LayoutLine& line : r.lines) {
            QVERIFY(fm.horizontalAdvance(line.text) <= 960);
        }
        // 总高度放进渲染区 (50 字在最小字号 12 下单行行高 ~16, 多行可容纳)
        QVERIFY(r.lines.size() * fm.height() <= 160);
    }

    // 4. 极端不可容纳: 字号收缩到下限附近, 不死循环
    //    (循环条件 fontSize > 12 + 步进 2: 最后一次实际执行字号是 14, 12 只触发退出)
    void testOverflowClampsToMinFontSize() {
        TextLayoutEngine engine;
        QString text;
        for (int i = 0; i < 500; ++i) text += QStringLiteral("字");
        LayoutResult r = engine.layout(text, QRect(0, 0, 960, 160));
        QCOMPARE(r.font.pointSize(), 14); // 有效下限
        QVERIFY(r.lines.size() > 1);
    }

    // 5. 混合文本: 颜文字符号粘成整体 token (不断开)
    void testKaomojiSticksTogether() {
        TextLayoutEngine engine;
        LayoutResult r = engine.layout(QStringLiteral("(╯°□°)╯︵ ┻━┻"), QRect(0, 0, 960, 160));
        QCOMPARE(r.lines.size(), 1);
        QCOMPARE(r.lines[0].text, QStringLiteral("(╯°□°)╯︵ ┻━┻"));
    }

    // 6. 换行的空格不吃进下一行行首
    void testSpaceNotLeadingNextLine() {
        TextLayoutEngine engine;
        QString text = QStringLiteral("word ");
        for (int i = 0; i < 40; ++i) text += QStringLiteral("word ");
        LayoutResult r = engine.layout(text, QRect(0, 0, 400, 160));
        QVERIFY(r.lines.size() > 1);
        for (int i = 1; i < r.lines.size(); ++i) {
            QVERIFY(!r.lines[i].text.startsWith(QLatin1Char(' ')));
        }
    }
};

QTEST_MAIN(TestTextLayoutEngine)
#include "TestTextLayoutEngine.moc"
