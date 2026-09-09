#ifndef SYSTEMRESOURCEMONITOR_H
#define SYSTEMRESOURCEMONITOR_H

#include <QObject>
#include <QTimer>

#ifdef Q_OS_WIN
// M4b: windows.h 的 min/max 宏会污染 QtNodes 头 (std::numeric_limits::max), 全局抑制
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class SystemResourceMonitor: public QObject {
	Q_OBJECT
public:
	explicit SystemResourceMonitor(QObject* parent = nullptr);
	void start(int intervalMs = 1000); // 默认每秒刷新一次
	void stop();

signals:
	// 发送当前的 CPU 占用率 (百分比) 和 内存占用 (MB)
	void resourceUpdated(double cpuPercent,double memoryMB);

private slots:
	void updateStats();

private:
	QTimer m_timer;

#ifdef Q_OS_WIN
	int m_numProcessors;
	ULARGE_INTEGER m_lastCpu;
	ULARGE_INTEGER m_lastSysCpu;
	ULARGE_INTEGER m_lastUserCpu;
#endif
};

#endif // SYSTEMRESOURCEMONITOR_H
