# KotoCord

**VTuber 中间件**——把语音/文本输入转化为字幕、情绪、语音和虚拟形象驱动信号的节点化管线工具。

从语音/手动输入出发，你可以自由组合：艺术字字幕渲染、LLM 情绪分析与颜文字、TTS 语音播报、定时提醒；未来的 OSC 输出可将文本直接映射为 VRChat 虚拟形象的嘴型与表情。管线以**节点图**描述，内置编辑器可视化编排，编辑结果编译为 C++ 实时管线运行。

## 功能

- **节点化管线** — 内置 QtNodes 图形编辑器：模板加载、节点增删、连线编辑、一键应用到运行时（停机重建，编译失败不影响当前运行）
- **双引擎语音识别** — Vosk（轻量离线流式）+ Whisper.cpp（高精度离线），运行时切换
- **LLM 情绪分析** — DeepSeek API（或 Mock），识别情绪附加颜文字，延迟/Token 看板
- **TTS 语音播报** — Edge-TTS（Python 侧车），打断语义（新句打断旧句）
- **可视化字幕** — 情绪艺术字渲染、队列调度与视觉锁、字号自适应排版
- **定时提醒** — 每日时刻表（如吃药提醒），到点 TTS 播报，QSettings 持久化
- **系统资源监控** — CPU / 内存实时面板

## 架构：图即配置

管线不是硬编码的——启动时从 `resources/pipelines/*.json` 读取节点图，经 `GraphCompiler` 校验（DAG / 端口类型 / 连接器）后装配为运行时管线。节点编辑器编辑的是"幻影"节点（端口与元信息），应用时导出图重新编译。实际数据流始终在 C++ 中以 Qt 信号槽运行，编辑器技术栈不影响实时性。

```
源节点                    处理节点                        汇节点
────────                ──────────────                 ──────────
voice_input ──┐
(manual UI) ──┼─→ orchestrator ──┬─→ subtitle_render (字幕窗口)
reminder ─────┘   (队列/锁/LLM/   ├─→ tts → audio_out   (语音播报)
                  颜文字, 复合)    └─→ (osc_out — 线头 T3, 未落地)
```

### 模块

| 目录 | 职责 |
|---|---|
| `src/core/graph/` | 图核心：`GraphModel`(纯数据+JSON) / `NodeRegistry`(元信息+连接器) / `GraphCompiler`(校验+装配) / `Pipeline`(生命周期) / `NodeCatalog`(真实节点目录) |
| `src/core/` | `AppController` 编排状态机（队列+视觉锁）、`DataTypes.h` 公用类型 |
| `src/ui/` | `MainWindow`、`NodeEditorView`(QtNodes 画布)、`GraphQtBridge`(图核心↔编辑器桥接) |
| `src/modules/source/` | 事件源节点：`VoiceInputNode`(语音复合)、`ReminderScheduler`(定时) |
| `src/modules/orchestrator/` | `OrchestratorNode` 编排复合节点（AppController+LLM+颜文字） |
| `src/modules/input/` | ASR 抽象层 + Vosk/Whisper 实现 + 上下文偏置（PromptContext/DomainDictionary） |
| `src/modules/capture/` | 音频采集 |
| `src/modules/llm/` | LLM 抽象层 + DeepSeek/Mock + `KaomojiManager` |
| `src/modules/tts/` | TTS 抽象层 + PythonEdgeTTS 侧车 + `TTSPlayer` |
| `src/modules/render/` | `TextLayoutEngine`(纯排版) + `SubtitleRenderer`(呈现) |
| `src/modules/system/` | 系统资源监控（旁路，不进图） |
| `src/utils/` | `AppPaths` 统一资源路径 |

### 管线模板

| 模板 | 内容 | 对应工作流 |
|---|---|---|
| `default.json` | 全功能：语音→编排→字幕+TTS+提醒 | 综合 |
| `t1-subtitle.json` | 语音→编排→字幕 | 工作流 1/2/4（情绪/对话渲染同拓扑） |
| `t2-subtitle-tts.json` | 字幕 + TTS 播报 | 工作流 5 |
| `t3-reminder-tts.json` | 定时提醒→TTS | 提醒场景 |
| （待 T3） | OSC 驱动虚拟形象 | 工作流 3/6 |

### 依赖

| 依赖 | 管理方式 | 说明 |
|---|---|---|
| Qt 6.9 LTS | MaintenanceTool + 环境变量 | Widgets / Multimedia / OpenGL(QtNodes) |
| whisper.cpp | FetchContent (v1.7.5, 静态) | 自动拉取 |
| QtNodes | FetchContent (pin commit, 静态) | 节点编辑器，BSD-3 |
| Vosk API | `third_party/vosk/` 手动 | C 库无 CMake |
| Edge-TTS | Python 侧车 (venv) | QProcess 调用 |

## 快速开始

```powershell
# 设环境变量 (每台机器一次)
setx Qt6_DIR "C:\Qt\6.9.x\msvc2022_64\lib\cmake\Qt6"

# 配置 + 构建 (FetchContent 需网络拉 whisper/QtNodes)
cmake --preset msvc-debug
cmake --build build/msvc-debug --config Debug

# 运行 (开发机需要 Qt bin 在 PATH)
$env:PATH = "C:\Qt\6.9.x\msvc2022_64\bin;" + $env:PATH
.\bin\Debug\Kotocord.exe
```

> 完整指南见 **[BUILD.md](BUILD.md)**，IDE 配置实录见 **[DEV_GUIDE.md](DEV_GUIDE.md)**。

## 项目结构

```
Kotocord/
├── CMakeLists.txt                     # 构建脚本 (FetchContent: whisper/QtNodes)
├── CMakePresets.json.example          # Preset 模板
├── check-deps.ps1                     # 依赖扫描 (check only)
├── CLAUDE.md                          # AI 协作约定
├── BUILD.md / DEV_GUIDE.md
├── src/                               # 源码 (core/ui/modules/utils)
├── tests/                             # 单元测试 (Qt Test, 11 个测试目标)
├── third_party/vosk/                  # Vosk SDK (手动下载)
├── resources/                         # 颜文字/模型/管线模板 (模型不提交)
├── build/  bin/                       # 产物 (不提交)
```

## 非 Git 追踪的资源

| 资源 | 下载地址 | 放置位置 |
|---|---|---|
| Vosk 运行时 DLL | [vosk-win64-0.3.45.zip](https://github.com/alphacep/vosk-api/releases) | `third_party/vosk/lib/` |
| Vosk 中文模型 | [vosk-model-small-cn-0.22.zip](https://alphacephei.com/vosk/models) | `resources/model/vosk-model-small-cn-0.22/` |
| Whisper 模型 | [ggml-small.bin](https://huggingface.co/ggerganov/whisper.cpp) | `resources/model/` |
| API Key | 本地创建 | `apikey.txt`（项目根目录） |

> 运行 `.\check-deps.ps1` 可扫描以上资源的就绪状态。

## 致谢

| 项目 | 用途 | 协议 |
|---|---|---|
| [Qt 6](https://www.qt.io/) | UI 框架与多媒体 | LGPL v3 |
| [QtNodes](https://github.com/paceholder/nodeeditor) | 节点编辑器 | BSD-3-Clause |
| [Vosk API](https://alphacephei.com/vosk/) | 离线流式语音识别 | Apache 2.0 |
| [Whisper.cpp](https://github.com/ggml-org/whisper.cpp) | 高精度语音识别 | MIT |
| [edge-tts](https://github.com/rany2/edge-tts) | TTS 语音合成 (Python) | GPL-3.0 (独立进程调用) |
