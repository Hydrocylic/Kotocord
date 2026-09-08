#include "ReminderScheduler.h"
#include <QSettings>
#include <QDateTime>
#include <QDebug>

static const int kCheckIntervalMs = 30 * 1000; // 检查粒度 30s, 分钟级提醒足够

ReminderScheduler::ReminderScheduler(QObject* parent)
    : QObject(parent) {
    m_timer.setInterval(kCheckIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ReminderScheduler::checkNow);
}

QList<QTime> ReminderScheduler::parseTimes(const QStringList& raw) {
    QList<QTime> result;
    for(const QString& item : raw) {
        QTime t = QTime::fromString(item.trimmed(), "HH:mm");
        if(t.isValid()) result.append(t);
    }
    return result;
}

QStringList ReminderScheduler::dueTimes(const QList<QTime>& scheduled, const QTime& now,
                                        const QDate& today, QHash<QString, QDate>& lastFired) {
    QStringList due;
    for(const QTime& t : scheduled) {
        const QString key = t.toString("HH:mm");
        // 已过时刻 (含当分钟) 且今日未触发 → 触发
        // 30s 粒度下 now == t 的两个检查周期都命中, lastFired 去重保证只发一次
        if(now >= t && lastFired.value(key) != today) {
            due.append(key);
            lastFired.insert(key, today);
        }
    }
    return due;
}

void ReminderScheduler::setTimes(const QStringList& times) {
    m_times.clear();
    for(const QString& item : times) {
        QTime t = QTime::fromString(item.trimmed(), "HH:mm");
        if(t.isValid()) m_times.append(t.toString("HH:mm")); // 规范化后保存
    }
}

void ReminderScheduler::loadFromSettings() {
    QSettings settings("MyStudio", "Kotocord");
    m_times = settings.value("Reminders/times").toStringList();
    m_message = settings.value("Reminders/message", m_message).toString();
    m_enabled = settings.value("Reminders/enabled", false).toBool();
}

void ReminderScheduler::saveToSettings() const {
    QSettings settings("MyStudio", "Kotocord");
    settings.setValue("Reminders/times", m_times);
    settings.setValue("Reminders/message", m_message);
    settings.setValue("Reminders/enabled", m_enabled);
}

void ReminderScheduler::start() {
    if(m_enabled && !m_times.isEmpty()) {
        m_timer.start();
        checkNow(); // 启动即查一次, 补上停机期间错过的当日时刻
    }
}

void ReminderScheduler::stop() {
    m_timer.stop();
}

void ReminderScheduler::checkNow() {
    if(!m_enabled) return;

    QDateTime now = QDateTime::currentDateTime();
    QHash<QString, QDate> lastFired = m_lastFired; // dueTimes 内部更新, 命中才写回
    QStringList due = dueTimes(parseTimes(m_times), now.time(), now.date(), lastFired);
    if(!due.isEmpty()) {
        m_lastFired = lastFired;
        qDebug() << "[Reminder] 到点触发:" << due << "→" << m_message;
        emit reminderReady(m_message);
    }
}
