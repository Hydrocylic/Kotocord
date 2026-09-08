#include <QtTest>

#include "modules/input/PromptContext.h"

// Phase 5 L1: Whisper initial_prompt 上下文窗口
// 独立单测 PromptContext (避免在测试进程加载 466MB whisper 模型)
class TestPromptContext : public QObject {
    Q_OBJECT

private slots:
    // 1. 空窗口: 只含固定中文提示语
    void testBuildEmpty() {
        PromptContext ctx;
        QCOMPARE(ctx.build(), QStringLiteral("以下是普通话的句子。"));
        QVERIFY(ctx.isEmpty());
    }

    // 2. 默认窗口 N=3: 拼接顺序保持, 提示语垫底
    void testBuildWithHistory() {
        PromptContext ctx;
        ctx.push(QStringLiteral("第一句"));
        ctx.push(QStringLiteral("第二句"));
        QCOMPARE(ctx.build(), QStringLiteral("第一句\n第二句\n以下是普通话的句子。"));
    }

    // 3. 窗口滑动: 超过 N 时最旧句滑出
    void testWindowSliding() {
        PromptContext ctx; // N=3
        ctx.push(QStringLiteral("一"));
        ctx.push(QStringLiteral("二"));
        ctx.push(QStringLiteral("三"));
        ctx.push(QStringLiteral("四"));
        ctx.push(QStringLiteral("五"));
        QCOMPARE(ctx.build(), QStringLiteral("三\n四\n五\n以下是普通话的句子。"));
    }

    // 4. 自定义窗口大小 N=2
    void testCustomWindowSize() {
        PromptContext ctx(2);
        ctx.push(QStringLiteral("一"));
        ctx.push(QStringLiteral("二"));
        ctx.push(QStringLiteral("三"));
        QCOMPARE(ctx.build(), QStringLiteral("二\n三\n以下是普通话的句子。"));
    }

    // 5. 空白句不进入窗口
    void testWhitespaceIgnored() {
        PromptContext ctx;
        ctx.push(QStringLiteral("   "));
        ctx.push(QStringLiteral(""));
        QVERIFY(ctx.isEmpty());
        QCOMPARE(ctx.build(), QStringLiteral("以下是普通话的句子。"));
    }

    // 6. 推入句附带首尾空白被裁剪
    void testTrimmed() {
        PromptContext ctx;
        ctx.push(QStringLiteral("  有空格  "));
        QCOMPARE(ctx.build(), QStringLiteral("有空格\n以下是普通话的句子。"));
    }

    // 7. clear 后回到空窗口
    void testClear() {
        PromptContext ctx;
        ctx.push(QStringLiteral("一"));
        ctx.clear();
        QVERIFY(ctx.isEmpty());
        QCOMPARE(ctx.build(), QStringLiteral("以下是普通话的句子。"));
    }
};

QTEST_MAIN(TestPromptContext)
#include "TestPromptContext.moc"
