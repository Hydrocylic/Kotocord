#include <QtTest>

#include "modules/source/ReminderScheduler.h"

#include <QSettings>
#include <QSignalSpy>

// M3: 定时提醒单测 — 纯时间匹配逻辑 (dueTimes/parseTimes) + 配置持久化 + 事件触发
// (定时器 30s 粒度不直接测, checkNow 经由 enabled+时刻数据驱动)
class TestReminderScheduler : public QObject {
    Q_OBJECT

private slots:
    // 1. 解析: 合法保留、非法丢弃、空白容忍
    void testParseTimes() {
        QStringList raw = {"08:00", "12:30 ", "25:99", "abc", "", "21:05"};
        QList<QTime> parsed = ReminderScheduler::parseTimes(raw);
        QCOMPARE(parsed.size(), 3);
        QCOMPARE(parsed[0], QTime(8, 0));
        QCOMPARE(parsed[1], QTime(12, 30));
        QCOMPARE(parsed[2], QTime(21, 5));
    }

    // 2. 到点判定: now >= scheduled 触发
    void testDueBasic() {
        QHash<QString, QDate> lastFired;
        QDate today(2026, 9, 8);
        QStringList due = ReminderScheduler::dueTimes(
            {QTime(8, 0), QTime(12, 0)}, QTime(9, 0), today, lastFired);
        QCOMPARE(due, QStringList{"08:00"}); // 8 点已过触发, 12 点未到不触发
    }

    // 3. 防重复: 同一时刻当日只触发一次, 次日重新可触发
    void testDueDedupPerDay() {
        QHash<QString, QDate> lastFired;
        QDate day1(2026, 9, 8), day2(2026, 9, 9);
        QList<QTime> sched = {QTime(8, 0)};

        // 第一轮: 触发
        QCOMPARE(ReminderScheduler::dueTimes(sched, QTime(8, 30), day1, lastFired),
                 QStringList{"08:00"});
        // 同日再查: 不重复
        QVERIFY(ReminderScheduler::dueTimes(sched, QTime(8, 31), day1, lastFired).isEmpty());
        // 次日同一时刻之后: 重新触发
        QCOMPARE(ReminderScheduler::dueTimes(sched, QTime(8, 30), day2, lastFired),
                 QStringList{"08:00"});
    }

    // 4. 边界: 恰好整分钟命中 (30s 粒度两个周期都满足 now >= t)
    void testDueExactMinute() {
        QHash<QString, QDate> lastFired;
        QCOMPARE(ReminderScheduler::dueTimes({QTime(8, 0)}, QTime(8, 0), QDate(2026, 9, 8), lastFired),
                 QStringList{"08:00"});
    }

    // 5. 配置规范化: setTimes 丢弃非法项并规范化格式
    void testSetTimesNormalizes() {
        ReminderScheduler s;
        s.setTimes({"08:00", "junk", " 23:59 "});
        QCOMPARE(s.times(), QStringList({"08:00", "23:59"}));
    }

    // 6. 持久化往返 (备份/恢复现场, 避免污染真实用户配置)
    void testSettingsRoundtrip() {
        QSettings settings("MyStudio", "Kotocord");
        QVariant bakTimes = settings.value("Reminders/times");
        QVariant bakMsg = settings.value("Reminders/message");
        QVariant bakEnabled = settings.value("Reminders/enabled");

        ReminderScheduler s;
        s.setTimes({"08:00", "20:00"});
        s.setMessage("记得滴眼药水");
        s.setEnabled(true);
        s.saveToSettings();

        ReminderScheduler s2;
        s2.loadFromSettings();
        QCOMPARE(s2.times(), QStringList({"08:00", "20:00"}));
        QCOMPARE(s2.message(), QStringLiteral("记得滴眼药水"));
        QCOMPARE(s2.enabled(), true);

        // 恢复现场
        for(const QString& key : {QStringLiteral("Reminders/times"),
                                  QStringLiteral("Reminders/message"),
                                  QStringLiteral("Reminders/enabled")}) {
            settings.contains(key) ? settings.setValue(key, key.endsWith("times") ? bakTimes
                                : key.endsWith("message") ? bakMsg : bakEnabled)
                                   : settings.remove(key);
        }
    }

    // 7. 事件触发: enabled + 已过时刻 → reminderReady 携带播报文本
    void testReminderSignal() {
        ReminderScheduler s;
        s.setEnabled(true);
        s.setMessage("该吃药了");
        // 用一个"刚刚已过"的时刻, checkNow 即应触发
        QTime justPassed = QTime::currentTime().addSecs(-60);
        s.setTimes({justPassed.toString("HH:mm")});

        QSignalSpy spy(&s, &ReminderScheduler::reminderReady);
        s.checkNow();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("该吃药了"));
        // 同日再查不重复
        s.checkNow();
        QCOMPARE(spy.count(), 1);
    }

    // 8. 未启用: 不触发
    void testDisabledNoFire() {
        ReminderScheduler s;
        s.setEnabled(false);
        QTime justPassed = QTime::currentTime().addSecs(-60);
        s.setTimes({justPassed.toString("HH:mm")});
        QSignalSpy spy(&s, &ReminderScheduler::reminderReady);
        s.checkNow();
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestReminderScheduler)
#include "TestReminderScheduler.moc"
