#ifndef REMINDERSCHEDULER_H
#define REMINDERSCHEDULER_H

#include <QObject>
#include <QStringList>
#include <QTime>
#include <QTimer>
#include <QHash>

// M3: 定时/吃药提醒 — 源节点 (事件产生器, 与手动/语音输入同构)
// 读系统时间, 到点触发固定文本事件 → 下游 TTS 播报
// 最小实现: 每日 HH:MM 时刻列表 + 固定播报文本, QSettings 持久化
// (线头 T6: 重复规则表达式/播报模板/LLM 组合, 本期不做)

class ReminderScheduler : public QObject {
    Q_OBJECT
public:
    explicit ReminderScheduler(QObject* parent = nullptr);

    // --- 配置 (QSettings: MyStudio/Kotocord, Reminders/*) ---
    void loadFromSettings();
    void saveToSettings() const;

    void setTimes(const QStringList& times);   // "HH:MM" 列表 (无效项被忽略)
    QStringList times() const { return m_times; }
    void setMessage(const QString& msg) { m_message = msg; }
    QString message() const { return m_message; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    void start();   // 启动周期检查 (30s 粒度)
    void stop();
    void checkNow(); // 立即检查一次 (启动补查/测试可达)

    // --- 纯逻辑 (单测可达, 不涉定时器) ---
    // 解析 "HH:MM" 列表, 丢弃无效项并规范化
    static QList<QTime> parseTimes(const QStringList& raw);
    // 计算此刻应触发的时刻: now >= scheduled 且该时刻今日未触发过
    // lastFired: 时刻文本 → 上次触发日期 (toString(Qt::ISODate)); 命中后被调用方更新
    static QStringList dueTimes(const QList<QTime>& scheduled, const QTime& now,
                                const QDate& today, QHash<QString, QDate>& lastFired);

signals:
    void reminderReady(const QString& message);// 到点事件 (→ 下游 TTS)

private:
    QTimer m_timer;
    QStringList m_times;                    // 原始 "HH:MM" 文本 (持久化形态)
    QString m_message = QStringLiteral("该吃药了");
    bool m_enabled = false;
    QHash<QString, QDate> m_lastFired;      // 防重复: 每时刻每日只触发一次
};

#endif // REMINDERSCHEDULER_H
