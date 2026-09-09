#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "../modules/render/SubtitleRenderer.h"
#include "../core/DataTypes.h"

class AppController;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
	// externalOverlay: 外部注入的字幕窗口 (M4a: 图装配的 subtitle_render 节点实例); 传 nullptr 时自建 (旧路径)
	MainWindow(AppController* controller, SubtitleRenderer* externalOverlay = nullptr, QWidget* parent = nullptr);
    ~MainWindow();

	// M4b: 停机重建后重挂图实例 (控制器/字幕窗口被 Pipeline 换新)
	void reattach(AppController* controller, SubtitleRenderer* externalOverlay);

signals:
	//生成UI点击信号
	void asrToggleRequested(bool enabled);
	void asrEngineSwitched(bool isWhisper); // false: Vosk, true: Whisper
	void llmEngineSwitched(bool isDeepSeek); // false: Mock, true: DeepSeek
	//假如使用的并非两个模型，bool型就不能用了
	void apiKeyChanged(const QString& key);

public slots:
	// 接收来自外部的数据以刷新 UI
	void onSubtitleReady(const SubtitleFrame& frame);
	void updateCpuMem(double cpu,double mem);
	void updateLatencyAndTokens(int prompt,int completion,qint64 latency);
	void updateEmotionLabel(const SubtitleFrame& frame);
	void onSystemReady(bool ready); // 用于更新那个绿色/红色的状态文本

private:
    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<SubtitleRenderer> m_overlayWidget; // 自建模式 (旧路径)
    // M4a: 外部注入 overlay (图装配的 subtitle_render 节点) — 所有权在 Pipeline
    SubtitleRenderer* m_injectedOverlay = nullptr;
    AppController* m_appController;

    SubtitleRenderer* overlay() const { return m_injectedOverlay ? m_injectedOverlay : m_overlayWidget.get(); }
};
#endif // MAINWINDOW_H
